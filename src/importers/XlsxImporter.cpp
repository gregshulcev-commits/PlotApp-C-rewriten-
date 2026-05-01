#include "plotapp/Importer.h"
#include "plotapp/TextUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <minizip/unzip.h>

namespace plotapp {
namespace {

constexpr std::uint64_t kMaxEntryBytes = 16ull * 1024ull * 1024ull;
constexpr std::size_t kMaxRows = 200000;
constexpr std::size_t kMaxCols = 512;

struct ZipGuard {
    unzFile handle {nullptr};
    ~ZipGuard() { if (handle) unzClose(handle); }
};

bool ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

bool nameChar(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == ':' || c == '_' || c == '-' || c == '.';
}

std::string localName(const std::string& name) {
    const std::size_t colon = name.find(':');
    return colon == std::string::npos ? name : name.substr(colon + 1);
}

std::string xmlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') out.push_back(s[i]);
        else if (s.compare(i, 5, "&amp;") == 0) { out.push_back('&'); i += 4; }
        else if (s.compare(i, 4, "&lt;") == 0) { out.push_back('<'); i += 3; }
        else if (s.compare(i, 4, "&gt;") == 0) { out.push_back('>'); i += 3; }
        else if (s.compare(i, 6, "&quot;") == 0) { out.push_back('"'); i += 5; }
        else if (s.compare(i, 6, "&apos;") == 0) { out.push_back('\''); i += 5; }
        else out.push_back('&');
    }
    return out;
}

std::string attrValue(const std::string& attrs, const std::string& wanted) {
    std::size_t i = 0;
    while (i < attrs.size()) {
        while (i < attrs.size() && (std::isspace(static_cast<unsigned char>(attrs[i])) || attrs[i] == '/')) ++i;
        const std::size_t keyStart = i;
        while (i < attrs.size() && !std::isspace(static_cast<unsigned char>(attrs[i])) && attrs[i] != '=') ++i;
        if (i == keyStart) break;
        const std::string key = attrs.substr(keyStart, i - keyStart);
        while (i < attrs.size() && std::isspace(static_cast<unsigned char>(attrs[i]))) ++i;
        if (i >= attrs.size() || attrs[i] != '=') continue;
        ++i;
        while (i < attrs.size() && std::isspace(static_cast<unsigned char>(attrs[i]))) ++i;
        if (i >= attrs.size()) break;
        std::string value;
        const char quote = attrs[i];
        if (quote == '"' || quote == '\'') {
            ++i;
            const std::size_t begin = i;
            while (i < attrs.size() && attrs[i] != quote) ++i;
            value = attrs.substr(begin, i - begin);
            if (i < attrs.size()) ++i;
        } else {
            const std::size_t begin = i;
            while (i < attrs.size() && !std::isspace(static_cast<unsigned char>(attrs[i])) && attrs[i] != '/') ++i;
            value = attrs.substr(begin, i - begin);
        }
        if (key == wanted) return xmlDecode(value);
    }
    return {};
}

bool findOpenTag(const std::string& xml, const std::string& wanted, std::size_t& pos,
                 std::string& attrs, std::size_t& bodyStart, bool& selfClosing) {
    while (pos < xml.size()) {
        const std::size_t open = xml.find('<', pos);
        if (open == std::string::npos) return false;
        std::size_t i = open + 1;
        while (i < xml.size() && std::isspace(static_cast<unsigned char>(xml[i]))) ++i;
        if (i >= xml.size()) return false;
        if (xml[i] == '/' || xml[i] == '!' || xml[i] == '?') {
            const std::size_t gt = xml.find('>', i + 1);
            pos = gt == std::string::npos ? xml.size() : gt + 1;
            continue;
        }
        const std::size_t nameStart = i;
        while (i < xml.size() && nameChar(xml[i])) ++i;
        if (i == nameStart) { pos = open + 1; continue; }
        const std::string tagName = xml.substr(nameStart, i - nameStart);
        const std::size_t gt = xml.find('>', i);
        if (gt == std::string::npos) return false;
        pos = gt + 1;
        if (!ieq(localName(tagName), wanted)) continue;
        std::size_t attrEnd = gt;
        while (attrEnd > i && std::isspace(static_cast<unsigned char>(xml[attrEnd - 1]))) --attrEnd;
        selfClosing = attrEnd > i && xml[attrEnd - 1] == '/';
        if (selfClosing) --attrEnd;
        attrs = xml.substr(i, attrEnd - i);
        bodyStart = gt + 1;
        return true;
    }
    return false;
}

bool findCloseTag(const std::string& xml, const std::string& wanted, std::size_t from,
                  std::size_t& bodyEnd, std::size_t& closeEnd) {
    while (from < xml.size()) {
        const std::size_t close = xml.find("</", from);
        if (close == std::string::npos) return false;
        std::size_t i = close + 2;
        while (i < xml.size() && std::isspace(static_cast<unsigned char>(xml[i]))) ++i;
        const std::size_t begin = i;
        while (i < xml.size() && nameChar(xml[i])) ++i;
        const std::size_t gt = xml.find('>', i);
        if (gt == std::string::npos) return false;
        if (i > begin && ieq(localName(xml.substr(begin, i - begin)), wanted)) {
            bodyEnd = close;
            closeEnd = gt + 1;
            return true;
        }
        from = gt + 1;
    }
    return false;
}

bool nextElement(const std::string& xml, const std::string& tag, std::size_t& pos,
                 std::string& attrs, std::string& body) {
    std::size_t bodyStart = 0;
    bool selfClosing = false;
    if (!findOpenTag(xml, tag, pos, attrs, bodyStart, selfClosing)) return false;
    if (selfClosing) { body.clear(); return true; }
    std::size_t bodyEnd = 0;
    std::size_t closeEnd = 0;
    if (!findCloseTag(xml, tag, bodyStart, bodyEnd, closeEnd)) return false;
    body = xml.substr(bodyStart, bodyEnd - bodyStart);
    pos = closeEnd;
    return true;
}

std::string stripXml(const std::string& input) {
    std::string out;
    bool tag = false;
    for (char c : input) {
        if (c == '<') tag = true;
        else if (c == '>') tag = false;
        else if (!tag) out.push_back(c);
    }
    return xmlDecode(out);
}

bool normalizeZipPath(std::string base, std::string target, std::string& out) {
    std::replace(target.begin(), target.end(), '\\', '/');
    if (target.empty()) return false;
    const std::size_t colon = target.find(':');
    const std::size_t slash = target.find('/');
    if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) return false;
    while (!target.empty() && target.front() == '/') target.erase(target.begin());
    std::string combined = target.rfind("xl/", 0) == 0 || base.empty() ? target : base + "/" + target;
    std::vector<std::string> parts;
    std::string part;
    for (char c : combined) {
        if (c == '/') {
            if (part == "..") return false;
            if (!part.empty() && part != ".") parts.push_back(part);
            part.clear();
        } else part.push_back(c);
    }
    if (part == "..") return false;
    if (!part.empty() && part != ".") parts.push_back(part);
    if (parts.empty()) return false;
    out.clear();
    for (std::size_t i = 0; i < parts.size(); ++i) { if (i) out.push_back('/'); out += parts[i]; }
    return true;
}

bool readEntry(unzFile zip, const std::string& rawName, std::string& data) {
    std::string name;
    if (!normalizeZipPath({}, rawName, name)) return false;
    if (unzLocateFile(zip, name.c_str(), 0) != UNZ_OK) return false;
    unz_file_info64 info {};
    if (unzGetCurrentFileInfo64(zip, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) return false;
    if (info.uncompressed_size > kMaxEntryBytes) return false;
    if (unzOpenCurrentFile(zip) != UNZ_OK) return false;
    data.clear();
    data.reserve(static_cast<std::size_t>(info.uncompressed_size));
    char buffer[8192];
    bool ok = true;
    for (;;) {
        const int n = unzReadCurrentFile(zip, buffer, sizeof(buffer));
        if (n < 0) { ok = false; break; }
        if (n == 0) break;
        data.append(buffer, buffer + n);
        if (data.size() > kMaxEntryBytes) { ok = false; break; }
    }
    unzCloseCurrentFile(zip);
    return ok;
}

std::vector<std::string> parseSharedStrings(const std::string& xml) {
    std::vector<std::string> strings;
    std::size_t pos = 0;
    std::string attrs;
    std::string body;
    while (nextElement(xml, "si", pos, attrs, body)) {
        strings.push_back(stripXml(body));
        if (strings.size() > kMaxRows * 8) break;
    }
    return strings;
}

std::string firstBody(const std::string& xml, const std::string& tag) {
    std::size_t pos = 0;
    std::string attrs;
    std::string body;
    return nextElement(xml, tag, pos, attrs, body) ? body : std::string();
}

std::string sharedStringAt(const std::vector<std::string>& strings, const std::string& indexText) {
    const std::string s = plotapp::text::trim(indexText);
    if (s.empty()) return {};
    std::size_t value = 0;
    for (unsigned char c : s) {
        if (!std::isdigit(c)) return {};
        value = value * 10u + static_cast<unsigned>(c - '0');
        if (value > strings.size() + 1000000u) return {};
    }
    return value < strings.size() ? strings[value] : std::string();
}

int columnFromRef(const std::string& ref) {
    int value = 0;
    bool any = false;
    for (char c : ref) {
        if (c >= 'A' && c <= 'Z') { value = value * 26 + c - 'A' + 1; any = true; }
        else if (c >= 'a' && c <= 'z') { value = value * 26 + c - 'a' + 1; any = true; }
        else break;
        if (value > static_cast<int>(kMaxCols)) break;
    }
    return any ? value - 1 : -1;
}

std::string readCell(const std::string& attrs, const std::string& body, const std::vector<std::string>& strings) {
    const std::string type = attrValue(attrs, "t");
    if (type == "inlineStr") return stripXml(firstBody(body, "is"));
    const std::string value = xmlDecode(firstBody(body, "v"));
    if (value.empty()) return {};
    return type == "s" ? sharedStringAt(strings, value) : value;
}

std::vector<std::vector<std::string>> parseRows(const std::string& xml, const std::vector<std::string>& strings) {
    std::vector<std::vector<std::string>> rows;
    std::size_t rowPos = 0;
    std::string rowAttrs;
    std::string rowBody;
    while (nextElement(xml, "row", rowPos, rowAttrs, rowBody)) {
        std::vector<std::string> row;
        std::size_t cellPos = 0;
        std::string cellAttrs;
        std::string cellBody;
        int nextCol = 0;
        while (nextElement(rowBody, "c", cellPos, cellAttrs, cellBody)) {
            int col = nextCol;
            const std::string ref = attrValue(cellAttrs, "r");
            if (!ref.empty()) col = columnFromRef(ref);
            if (col < 0 || static_cast<std::size_t>(col) >= kMaxCols) continue;
            if (row.size() <= static_cast<std::size_t>(col)) row.resize(static_cast<std::size_t>(col) + 1);
            row[static_cast<std::size_t>(col)] = readCell(cellAttrs, cellBody, strings);
            nextCol = col + 1;
        }
        if (!row.empty()) {
            rows.push_back(row);
            if (rows.size() > kMaxRows) break;
        }
    }
    return rows;
}

bool emptyRow(const std::vector<std::string>& row) {
    for (const std::string& cell : row) if (!plotapp::text::trim(cell).empty()) return false;
    return true;
}

} // namespace

bool XlsxImporter::supports(const std::string& path) const {
    return plotapp::text::toLower(std::filesystem::path(path).extension().string()) == ".xlsx";
}

TableData XlsxImporter::load(const std::string& path) const {
    ZipGuard zip {unzOpen(path.c_str())};
    if (!zip.handle) throw std::runtime_error("Cannot open xlsx archive: " + path);

    std::string xml;
    std::vector<std::string> strings;
    if (readEntry(zip.handle, "xl/sharedStrings.xml", xml)) strings = parseSharedStrings(xml);

    std::string workbook;
    if (!readEntry(zip.handle, "xl/workbook.xml", workbook)) throw std::runtime_error("Cannot read XLSX workbook metadata");
    std::size_t sheetPos = 0;
    std::string sheetAttrs;
    std::size_t unusedStart = 0;
    bool unusedSelfClosing = false;
    if (!findOpenTag(workbook, "sheet", sheetPos, sheetAttrs, unusedStart, unusedSelfClosing)) throw std::runtime_error("XLSX workbook has no sheets");
    std::string sheetName = attrValue(sheetAttrs, "name");
    if (sheetName.empty()) sheetName = "Sheet1";
    std::string relId = attrValue(sheetAttrs, "r:id");
    if (relId.empty()) relId = attrValue(sheetAttrs, "id");
    if (relId.empty()) throw std::runtime_error("XLSX sheet relationship missing");

    std::string rels;
    if (!readEntry(zip.handle, "xl/_rels/workbook.xml.rels", rels)) throw std::runtime_error("Cannot read XLSX relationships");
    std::string target;
    std::size_t relPos = 0;
    std::string relAttrs;
    while (findOpenTag(rels, "Relationship", relPos, relAttrs, unusedStart, unusedSelfClosing)) {
        if (attrValue(relAttrs, "Id") != relId) continue;
        if (plotapp::text::toLower(attrValue(relAttrs, "TargetMode")) == "external") throw std::runtime_error("External XLSX sheet target rejected");
        if (!normalizeZipPath("xl", attrValue(relAttrs, "Target"), target)) throw std::runtime_error("Unsafe XLSX sheet target");
        break;
    }
    if (target.empty()) throw std::runtime_error("Cannot resolve XLSX sheet target");

    std::string sheetXml;
    if (!readEntry(zip.handle, target, sheetXml)) throw std::runtime_error("Cannot read XLSX sheet");
    std::vector<std::vector<std::string>> rows = parseRows(sheetXml, strings);
    while (!rows.empty() && emptyRow(rows.front())) rows.erase(rows.begin());
    if (rows.empty()) throw std::runtime_error("No rows found in xlsx sheet");
    std::size_t columns = 0;
    for (const std::vector<std::string>& row : rows) columns = std::max(columns, row.size());
    if (columns == 0 || columns > kMaxCols) throw std::runtime_error("Invalid XLSX column count");

    TableData table;
    table.sourcePath = path;
    table.sheetName = sheetName;
    bool hasHeader = false;
    for (const std::string& cell : rows.front()) {
        bool ok = false;
        plotapp::text::toDouble(cell, &ok);
        if (!ok && !plotapp::text::trim(cell).empty()) hasHeader = true;
    }
    if (hasHeader) {
        table.headers = rows.front();
        table.headers.resize(columns);
        rows.erase(rows.begin());
    } else {
        for (std::size_t i = 0; i < columns; ++i) table.headers.push_back("col" + std::to_string(i + 1));
    }
    for (std::vector<std::string>& row : rows) {
        row.resize(table.headers.size());
        table.rows.push_back(row);
    }
    return table;
}

} // namespace plotapp
