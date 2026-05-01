#include "plotapp/Importer.h"
#include "plotapp/TextUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <minizip/unzip.h>

namespace plotapp {
namespace {

constexpr std::uintmax_t kMaxXlsxFileBytes = 64ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxXlsxEntryBytes = 16ull * 1024ull * 1024ull;
constexpr std::size_t kMaxSheetRows = 200'000;
constexpr std::size_t kMaxSheetColumns = 512;
constexpr std::size_t kMaxSharedStrings = kMaxSheetRows * 8;

class ZipEntryNotFound final : public std::runtime_error {
public:
    explicit ZipEntryNotFound(const std::string& name)
        : std::runtime_error("ZIP entry not found: " + name) {}
};

class ZipArchive final {
public:
    explicit ZipArchive(const std::string& path) : archive_(unzOpen64(path.c_str())) {
        if (!archive_) throw std::runtime_error("Cannot open xlsx archive: " + path);
    }

    ~ZipArchive() {
        if (archive_ != nullptr) unzClose(archive_);
    }

    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;

    unzFile get() const { return archive_; }

private:
    unzFile archive_ {nullptr};
};

void appendUtf8(std::string& out, std::uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return;
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::optional<std::uint32_t> parseXmlNumericEntity(std::string_view entity) {
    if (entity.size() < 2 || entity.front() != '#') return std::nullopt;
    std::uint32_t value = 0;
    std::size_t pos = 1;
    const bool hex = pos < entity.size() && (entity[pos] == 'x' || entity[pos] == 'X');
    if (hex) ++pos;
    if (pos >= entity.size()) return std::nullopt;
    for (; pos < entity.size(); ++pos) {
        const unsigned char c = static_cast<unsigned char>(entity[pos]);
        int digit = -1;
        if (hex) {
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        } else if (c >= '0' && c <= '9') {
            digit = c - '0';
        }
        if (digit < 0) return std::nullopt;
        const auto base = static_cast<std::uint32_t>(hex ? 16 : 10);
        if (value > (0x10FFFFu - static_cast<std::uint32_t>(digit)) / base) return std::nullopt;
        value = value * base + static_cast<std::uint32_t>(digit);
    }
    return value;
}

std::string xmlDecode(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '&') {
            out.push_back(value[i]);
            continue;
        }
        const auto end = value.find(';', i + 1);
        if (end == std::string_view::npos) {
            out.push_back(value[i]);
            continue;
        }
        const auto entity = value.substr(i + 1, end - i - 1);
        if (entity == "amp") out.push_back('&');
        else if (entity == "lt") out.push_back('<');
        else if (entity == "gt") out.push_back('>');
        else if (entity == "quot") out.push_back('"');
        else if (entity == "apos") out.push_back('\'');
        else if (auto codepoint = parseXmlNumericEntity(entity)) appendUtf8(out, *codepoint);
        else {
            out.push_back('&');
            out.append(entity.begin(), entity.end());
            out.push_back(';');
        }
        i = end;
    }
    return out;
}

std::string stripXml(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    bool insideTag = false;
    for (char c : input) {
        if (c == '<') {
            insideTag = true;
        } else if (c == '>') {
            insideTag = false;
        } else if (!insideTag) {
            out.push_back(c);
        }
    }
    return xmlDecode(out);
}

std::string localName(std::string_view name) {
    const auto colon = name.rfind(':');
    if (colon == std::string_view::npos) return std::string(name);
    return std::string(name.substr(colon + 1));
}

bool isXmlNameChar(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_' || c == '-' || c == ':' || c == '.';
}

void skipWhitespace(std::string_view text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
}

std::map<std::string, std::string> parseAttributes(std::string_view tagText) {
    std::map<std::string, std::string> attrs;
    std::size_t pos = 0;
    if (pos < tagText.size() && tagText[pos] == '<') ++pos;
    if (pos < tagText.size() && tagText[pos] == '/') ++pos;
    while (pos < tagText.size() && isXmlNameChar(tagText[pos])) ++pos;

    while (pos < tagText.size()) {
        skipWhitespace(tagText, pos);
        if (pos >= tagText.size() || tagText[pos] == '>' || tagText[pos] == '/') break;
        const auto nameStart = pos;
        while (pos < tagText.size() && isXmlNameChar(tagText[pos])) ++pos;
        if (pos == nameStart) {
            ++pos;
            continue;
        }
        const std::string name(tagText.substr(nameStart, pos - nameStart));
        skipWhitespace(tagText, pos);
        if (pos >= tagText.size() || tagText[pos] != '=') continue;
        ++pos;
        skipWhitespace(tagText, pos);
        if (pos >= tagText.size() || (tagText[pos] != '"' && tagText[pos] != '\'')) continue;
        const char quote = tagText[pos++];
        const auto valueStart = pos;
        while (pos < tagText.size() && tagText[pos] != quote) ++pos;
        attrs[name] = xmlDecode(tagText.substr(valueStart, pos - valueStart));
        if (pos < tagText.size()) ++pos;
    }
    return attrs;
}

std::optional<std::string> attributeValue(const std::map<std::string, std::string>& attrs,
                                          std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const auto it = attrs.find(name);
        if (it != attrs.end()) return it->second;
    }
    return std::nullopt;
}

bool findNextOpeningTag(const std::string& xml, const std::string& wantedLocalName,
                        std::size_t& searchPos, std::size_t& tagStart, std::size_t& tagEnd) {
    while (true) {
        tagStart = xml.find('<', searchPos);
        if (tagStart == std::string::npos) return false;
        if (tagStart + 1 >= xml.size()) return false;
        const char after = xml[tagStart + 1];
        if (after == '/' || after == '!' || after == '?') {
            searchPos = tagStart + 1;
            continue;
        }
        std::size_t nameStart = tagStart + 1;
        std::size_t nameEnd = nameStart;
        while (nameEnd < xml.size() && isXmlNameChar(xml[nameEnd])) ++nameEnd;
        if (nameEnd == nameStart) {
            searchPos = tagStart + 1;
            continue;
        }
        tagEnd = xml.find('>', nameEnd);
        if (tagEnd == std::string::npos) return false;
        if (localName(std::string_view(xml.data() + nameStart, nameEnd - nameStart)) == wantedLocalName) {
            searchPos = tagEnd + 1;
            return true;
        }
        searchPos = tagEnd + 1;
    }
}

bool isSelfClosingTag(const std::string& xml, std::size_t tagStart, std::size_t tagEnd) {
    (void)tagStart;
    if (tagEnd == 0) return false;
    std::size_t pos = tagEnd;
    while (pos > 0 && std::isspace(static_cast<unsigned char>(xml[pos - 1]))) --pos;
    return pos > 0 && xml[pos - 1] == '/';
}

bool findClosingTag(const std::string& xml, const std::string& wantedLocalName,
                    std::size_t searchPos, std::size_t& closingStart, std::size_t& closingEnd) {
    while (true) {
        closingStart = xml.find("</", searchPos);
        if (closingStart == std::string::npos) return false;
        std::size_t nameStart = closingStart + 2;
        std::size_t nameEnd = nameStart;
        while (nameEnd < xml.size() && isXmlNameChar(xml[nameEnd])) ++nameEnd;
        closingEnd = xml.find('>', nameEnd);
        if (closingEnd == std::string::npos) return false;
        if (localName(std::string_view(xml.data() + nameStart, nameEnd - nameStart)) == wantedLocalName) return true;
        searchPos = closingEnd + 1;
    }
}

struct XmlElement {
    std::string openTag;
    std::string_view body;
};

bool findNextElement(const std::string& xml, const std::string& localNameToFind,
                     std::size_t& searchPos, XmlElement& element) {
    std::size_t tagStart = 0;
    std::size_t tagEnd = 0;
    if (!findNextOpeningTag(xml, localNameToFind, searchPos, tagStart, tagEnd)) return false;
    element.openTag = xml.substr(tagStart, tagEnd - tagStart + 1);
    if (isSelfClosingTag(xml, tagStart, tagEnd)) {
        element.body = std::string_view(xml.data() + tagEnd + 1, 0);
        searchPos = tagEnd + 1;
        return true;
    }
    std::size_t closingStart = 0;
    std::size_t closingEnd = 0;
    if (!findClosingTag(xml, localNameToFind, tagEnd + 1, closingStart, closingEnd)) return false;
    element.body = std::string_view(xml.data() + tagEnd + 1, closingStart - tagEnd - 1);
    searchPos = closingEnd + 1;
    return true;
}

std::string readZipEntry(unzFile archive, const std::string& name) {
    if (unzLocateFile(archive, name.c_str(), 0) != UNZ_OK) {
        throw ZipEntryNotFound(name);
    }

    unz_file_info64 info {};
    if (unzGetCurrentFileInfo64(archive, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) {
        throw std::runtime_error("Cannot inspect ZIP entry: " + name);
    }
    if (info.uncompressed_size > kMaxXlsxEntryBytes) {
        throw std::runtime_error("XLSX entry is too large: " + name);
    }

    if (unzOpenCurrentFile(archive) != UNZ_OK) {
        throw std::runtime_error("Cannot open ZIP entry: " + name);
    }

    std::string data;
    data.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(info.uncompressed_size, kMaxXlsxEntryBytes)));
    char buffer[8192];
    for (;;) {
        const int bytesRead = unzReadCurrentFile(archive, buffer, sizeof(buffer));
        if (bytesRead < 0) {
            unzCloseCurrentFile(archive);
            throw std::runtime_error("Cannot read ZIP entry: " + name);
        }
        if (bytesRead == 0) break;
        data.append(buffer, buffer + bytesRead);
        if (data.size() > kMaxXlsxEntryBytes) {
            unzCloseCurrentFile(archive);
            throw std::runtime_error("XLSX entry exceeded maximum allowed size: " + name);
        }
    }

    unzCloseCurrentFile(archive);
    return data;
}

std::optional<std::string> tryReadZipEntry(unzFile archive, const std::string& name) {
    try {
        return readZipEntry(archive, name);
    } catch (const ZipEntryNotFound&) {
        return std::nullopt;
    }
}

std::string collectTextElements(std::string_view xmlView) {
    const std::string xml(xmlView);
    std::string text;
    std::size_t pos = 0;
    XmlElement element;
    while (findNextElement(xml, "t", pos, element)) {
        text += stripXml(element.body);
    }
    return text;
}

std::optional<std::string> firstElementText(std::string_view xmlView, const std::string& elementName) {
    const std::string xml(xmlView);
    std::size_t pos = 0;
    XmlElement element;
    if (!findNextElement(xml, elementName, pos, element)) return std::nullopt;
    return stripXml(element.body);
}

std::vector<std::string> parseSharedStrings(const std::string& xml) {
    std::vector<std::string> result;
    std::size_t pos = 0;
    XmlElement si;
    while (findNextElement(xml, "si", pos, si)) {
        std::string value = collectTextElements(si.body);
        if (value.empty()) value = stripXml(si.body);
        result.push_back(std::move(value));
        if (result.size() > kMaxSharedStrings) {
            throw std::runtime_error("sharedStrings.xml is unexpectedly large");
        }
    }
    return result;
}

std::pair<std::string, std::string> firstSheetInfo(const std::string& workbookXml) {
    std::size_t pos = 0;
    XmlElement sheet;
    while (findNextElement(workbookXml, "sheet", pos, sheet)) {
        const auto attrs = parseAttributes(sheet.openTag);
        const auto name = attributeValue(attrs, {"name"}).value_or("Sheet1");
        const auto relId = attributeValue(attrs, {"r:id", "id"});
        if (relId.has_value() && !relId->empty()) return {name, *relId};
    }
    throw std::runtime_error("Cannot find first worksheet in workbook.xml");
}

std::string normalizeZipPath(const std::string& baseDirectory, std::string target) {
    std::replace(target.begin(), target.end(), '\\', '/');
    if (target.empty() || target.find("://") != std::string::npos) {
        throw std::runtime_error("Unsafe or empty XLSX relationship target");
    }

    std::string combined;
    if (!target.empty() && target.front() == '/') {
        combined = target.substr(1);
    } else if (plotapp::text::startsWith(target, "xl/")) {
        combined = target;
    } else {
        combined = baseDirectory;
        if (!combined.empty() && combined.back() != '/') combined.push_back('/');
        combined += target;
    }

    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= combined.size()) {
        const auto slash = combined.find('/', start);
        const auto part = combined.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part.empty() || part == ".") {
            // ignore
        } else if (part == "..") {
            if (parts.empty()) throw std::runtime_error("XLSX relationship target escapes package root");
            parts.pop_back();
        } else {
            parts.push_back(part);
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }

    std::string normalized;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) normalized.push_back('/');
        normalized += parts[i];
    }
    if (normalized.empty()) throw std::runtime_error("Unsafe or empty XLSX relationship target");
    return normalized;
}

std::string resolveSheetTarget(const std::string& relsXml, const std::string& relId) {
    std::size_t pos = 0;
    XmlElement relationship;
    while (findNextElement(relsXml, "Relationship", pos, relationship)) {
        const auto attrs = parseAttributes(relationship.openTag);
        const auto id = attributeValue(attrs, {"Id"});
        if (!id.has_value() || *id != relId) continue;
        const auto targetMode = attributeValue(attrs, {"TargetMode"}).value_or(std::string{});
        if (targetMode == "External" || targetMode == "external") {
            throw std::runtime_error("External worksheet relationships are not supported");
        }
        const auto target = attributeValue(attrs, {"Target"});
        if (!target.has_value() || target->empty()) break;
        return normalizeZipPath("xl", *target);
    }
    throw std::runtime_error("Cannot resolve worksheet relationship: " + relId);
}

bool parseUnsignedInteger(const std::string& value, std::size_t& out) {
    const auto trimmed = plotapp::text::trim(value);
    if (trimmed.empty()) return false;
    std::size_t result = 0;
    for (unsigned char c : trimmed) {
        if (!std::isdigit(c)) return false;
        const std::size_t digit = static_cast<std::size_t>(c - '0');
        if (result > (std::numeric_limits<std::size_t>::max() - digit) / 10u) return false;
        result = result * 10u + digit;
    }
    out = result;
    return true;
}

int columnFromReference(const std::string& cellRef) {
    std::size_t result = 0;
    bool sawColumn = false;
    for (unsigned char c : cellRef) {
        if (c >= 'A' && c <= 'Z') {
            sawColumn = true;
            result = result * 26u + static_cast<std::size_t>(c - 'A' + 1);
        } else if (c >= 'a' && c <= 'z') {
            sawColumn = true;
            result = result * 26u + static_cast<std::size_t>(c - 'a' + 1);
        } else {
            break;
        }
        if (result > kMaxSheetColumns) break;
    }
    if (!sawColumn || result == 0 || result > static_cast<std::size_t>(std::numeric_limits<int>::max())) return -1;
    return static_cast<int>(result - 1u);
}

std::string cellValueFromXml(std::string_view cellBody, const std::string& type,
                             const std::vector<std::string>& sharedStrings) {
    if (type == "inlineStr") {
        std::string value = collectTextElements(cellBody);
        if (value.empty()) value = stripXml(cellBody);
        return value;
    }

    const auto rawValue = firstElementText(cellBody, "v");
    if (!rawValue.has_value()) {
        if (type == "str") return collectTextElements(cellBody);
        return {};
    }

    if (type == "s") {
        std::size_t index = 0;
        if (!parseUnsignedInteger(*rawValue, index) || index >= sharedStrings.size()) return {};
        return sharedStrings[index];
    }
    return *rawValue;
}

std::vector<std::vector<std::string>> parseSheetRows(const std::string& xml, const std::vector<std::string>& sharedStrings) {
    std::vector<std::vector<std::string>> rows;
    std::size_t rowPos = 0;
    XmlElement row;
    while (findNextElement(xml, "row", rowPos, row)) {
        const std::string rowXml(row.body);
        std::map<int, std::string> rowCells;
        int nextImplicitColumn = 0;

        std::size_t cellPos = 0;
        XmlElement cell;
        while (findNextElement(rowXml, "c", cellPos, cell)) {
            const auto attrs = parseAttributes(cell.openTag);
            int column = nextImplicitColumn;
            if (const auto ref = attributeValue(attrs, {"r"}); ref.has_value() && !ref->empty()) {
                column = columnFromReference(*ref);
            }
            if (column < 0 || static_cast<std::size_t>(column) >= kMaxSheetColumns) {
                throw std::runtime_error("Worksheet column limit exceeded");
            }
            nextImplicitColumn = column + 1;

            const auto type = attributeValue(attrs, {"t"}).value_or(std::string{});
            rowCells[column] = cellValueFromXml(cell.body, type, sharedStrings);
        }

        if (!rowCells.empty()) {
            const int width = rowCells.rbegin()->first + 1;
            std::vector<std::string> parsedRow(static_cast<std::size_t>(width));
            for (const auto& [index, value] : rowCells) {
                parsedRow[static_cast<std::size_t>(index)] = value;
            }
            rows.push_back(std::move(parsedRow));
            if (rows.size() > kMaxSheetRows) {
                throw std::runtime_error("Worksheet row limit exceeded");
            }
        }
    }
    return rows;
}

bool firstRowLooksLikeHeader(const std::vector<std::string>& row) {
    for (const auto& cell : row) {
        const auto trimmed = plotapp::text::trim(cell);
        if (trimmed.empty()) continue;
        bool ok = false;
        plotapp::text::toDouble(trimmed, &ok);
        if (!ok) return true;
    }
    return false;
}

std::vector<std::string> defaultHeaders(std::size_t count) {
    std::vector<std::string> headers;
    headers.reserve(count);
    for (std::size_t i = 0; i < count; ++i) headers.push_back("col" + std::to_string(i + 1));
    return headers;
}

void extendHeaders(std::vector<std::string>& headers, std::size_t count) {
    while (headers.size() < count) headers.push_back("col" + std::to_string(headers.size() + 1));
    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (plotapp::text::trim(headers[i]).empty()) headers[i] = "col" + std::to_string(i + 1);
    }
}

std::size_t maxRowWidth(const std::vector<std::vector<std::string>>& rows) {
    std::size_t width = 0;
    for (const auto& row : rows) width = std::max(width, row.size());
    return width;
}

void ensureXlsxFileIsSafe(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || !std::filesystem::is_regular_file(status)) {
        throw std::runtime_error("XLSX path is not a regular file: " + path);
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (!ec && size > kMaxXlsxFileBytes) {
        throw std::runtime_error("XLSX file is too large: " + path);
    }
}

} // namespace

bool XlsxImporter::supports(const std::string& path) const {
    return plotapp::text::toLower(std::filesystem::path(path).extension().string()) == ".xlsx";
}

TableData XlsxImporter::load(const std::string& path) const {
    ensureXlsxFileIsSafe(path);
    ZipArchive archive(path);

    const auto workbookXml = readZipEntry(archive.get(), "xl/workbook.xml");
    const auto [sheetName, relId] = firstSheetInfo(workbookXml);

    std::string sheetTarget;
    if (const auto relsXml = tryReadZipEntry(archive.get(), "xl/_rels/workbook.xml.rels")) {
        sheetTarget = resolveSheetTarget(*relsXml, relId);
    } else {
        sheetTarget = "xl/worksheets/sheet1.xml";
    }

    std::vector<std::string> sharedStrings;
    if (const auto sharedXml = tryReadZipEntry(archive.get(), "xl/sharedStrings.xml")) {
        sharedStrings = parseSharedStrings(*sharedXml);
    }

    const auto sheetXml = readZipEntry(archive.get(), sheetTarget);
    auto rows = parseSheetRows(sheetXml, sharedStrings);
    if (rows.empty()) {
        throw std::runtime_error("No rows found in xlsx sheet");
    }

    TableData table;
    table.sourcePath = path;
    table.sheetName = sheetName;

    const bool headerLikely = firstRowLooksLikeHeader(rows.front());
    if (headerLikely) {
        table.headers = rows.front();
        rows.erase(rows.begin());
        extendHeaders(table.headers, std::max(table.headers.size(), maxRowWidth(rows)));
    } else {
        table.headers = defaultHeaders(maxRowWidth(rows));
    }

    if (table.headers.empty()) {
        throw std::runtime_error("No columns found in xlsx sheet");
    }
    if (table.headers.size() > kMaxSheetColumns) {
        throw std::runtime_error("Worksheet column limit exceeded");
    }

    for (auto& row : rows) {
        row.resize(table.headers.size());
        table.rows.push_back(std::move(row));
    }
    if (table.rows.empty()) {
        throw std::runtime_error("No data rows found in xlsx sheet");
    }
    return table;
}

} // namespace plotapp
