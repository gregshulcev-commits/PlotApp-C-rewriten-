#include "plotapp/Importer.h"
#include "plotapp/TextUtil.h"

#include <cstring>
#include <filesystem>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include <minizip/unzip.h>

namespace plotapp {
namespace {

constexpr std::uint64_t kMaxXlsxEntryBytes = 16ull * 1024ull * 1024ull;
constexpr std::size_t kMaxSheetRows = 200'000;
constexpr std::size_t kMaxSheetColumns = 512;

std::string readZipEntry(unzFile archive, const std::string& name) {
    if (unzLocateFile(archive, name.c_str(), 0) != UNZ_OK) {
        throw std::runtime_error("ZIP entry not found: " + name);
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
    data.reserve(static_cast<std::size_t>(info.uncompressed_size));
    char buffer[4096];
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

std::string xmlDecode(std::string value) {
    struct Rule { const char* from; const char* to; };
    static const Rule rules[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}
    };
    for (const auto& rule : rules) {
        std::size_t pos = 0;
        while ((pos = value.find(rule.from, pos)) != std::string::npos) {
            value.replace(pos, std::strlen(rule.from), rule.to);
            pos += std::strlen(rule.to);
        }
    }
    return value;
}

std::string stripXml(const std::string& input) {
    std::string out;
    bool inside = false;
    for (char c : input) {
        if (c == '<') inside = true;
        else if (c == '>') inside = false;
        else if (!inside) out.push_back(c);
    }
    return xmlDecode(out);
}

std::vector<std::string> parseSharedStrings(const std::string& xml) {
    std::vector<std::string> result;
    std::regex re(R"delim(<si[^>]*>([\s\S]*?)</si>)delim", std::regex::icase);
    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), re); it != std::sregex_iterator(); ++it) {
        result.push_back(stripXml((*it)[1].str()));
        if (result.size() > kMaxSheetRows * 8) {
            throw std::runtime_error("sharedStrings.xml is unexpectedly large");
        }
    }
    return result;
}

std::pair<std::string, std::string> firstSheetInfo(const std::string& workbookXml) {
    std::regex re(R"delim(<sheet[^>]*name="([^"]+)"[^>]*r:id="([^"]+)")delim", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(workbookXml, match, re)) {
        throw std::runtime_error("Cannot find first worksheet in workbook.xml");
    }
    return {xmlDecode(match[1].str()), match[2].str()};
}

std::string resolveSheetTarget(const std::string& relsXml, const std::string& relId) {
    std::regex relRe(R"delim(<Relationship([^>]*)/?>)delim", std::regex::icase);
    std::regex idRe(R"delim(Id="([^"]+)")delim");
    std::regex targetRe(R"delim(Target="([^"]+)")delim");
    for (auto it = std::sregex_iterator(relsXml.begin(), relsXml.end(), relRe); it != std::sregex_iterator(); ++it) {
        const std::string attrs = (*it)[1].str();
        std::smatch idMatch;
        std::smatch targetMatch;
        if (!std::regex_search(attrs, idMatch, idRe)) continue;
        if (idMatch[1].str() != relId) continue;
        if (!std::regex_search(attrs, targetMatch, targetRe)) break;
        std::string target = targetMatch[1].str();
        if (!target.empty() && target.front() == '/') target.erase(target.begin());
        if (!plotapp::text::startsWith(target, "xl/")) target = "xl/" + target;
        return target;
    }
    throw std::runtime_error("Cannot resolve worksheet relationship: " + relId);
}

int columnFromReference(const std::string& cellRef) {
    int result = 0;
    for (char c : cellRef) {
        if (c >= 'A' && c <= 'Z') {
            result = result * 26 + (c - 'A' + 1);
        } else if (c >= 'a' && c <= 'z') {
            result = result * 26 + (c - 'a' + 1);
        } else {
            break;
        }
    }
    return result - 1;
}

std::vector<std::vector<std::string>> parseSheetRows(const std::string& xml, const std::vector<std::string>& sharedStrings) {
    std::vector<std::vector<std::string>> rows;
    std::regex rowRe(R"delim(<row[^>]*>([\s\S]*?)</row>)delim", std::regex::icase);
    std::regex cellRe(R"delim(<c([^>]*)>([\s\S]*?)</c>)delim", std::regex::icase);
    std::regex refRe(R"delim(r="([A-Za-z]+[0-9]+)")delim");
    std::regex typeRe(R"delim(t="([^"]+)")delim");
    std::regex valueRe(R"delim(<v[^>]*>([\s\S]*?)</v>)delim", std::regex::icase);
    std::regex inlineRe(R"delim(<is[^>]*>([\s\S]*?)</is>)delim", std::regex::icase);

    for (auto rowIt = std::sregex_iterator(xml.begin(), xml.end(), rowRe); rowIt != std::sregex_iterator(); ++rowIt) {
        std::map<int, std::string> rowCells;
        const std::string rowXml = (*rowIt)[1].str();
        for (auto cellIt = std::sregex_iterator(rowXml.begin(), rowXml.end(), cellRe); cellIt != std::sregex_iterator(); ++cellIt) {
            const std::string attrs = (*cellIt)[1].str();
            const std::string body = (*cellIt)[2].str();
            std::smatch refMatch;
            if (!std::regex_search(attrs, refMatch, refRe)) continue;
            const int column = columnFromReference(refMatch[1].str());
            if (column < 0 || static_cast<std::size_t>(column) >= kMaxSheetColumns) {
                throw std::runtime_error("Worksheet column limit exceeded");
            }

            std::smatch typeMatch;
            std::string type;
            if (std::regex_search(attrs, typeMatch, typeRe)) type = typeMatch[1].str();

            std::smatch valueMatch;
            std::string value;
            if (type == "inlineStr") {
                if (std::regex_search(body, valueMatch, inlineRe)) value = stripXml(valueMatch[1].str());
            } else if (std::regex_search(body, valueMatch, valueRe)) {
                value = xmlDecode(valueMatch[1].str());
                if (type == "s") {
                    const std::size_t index = static_cast<std::size_t>(std::stoul(value));
                    value = index < sharedStrings.size() ? sharedStrings[index] : std::string();
                }
            }
            rowCells[column] = value;
        }
        if (!rowCells.empty()) {
            const int width = rowCells.rbegin()->first + 1;
            std::vector<std::string> row(static_cast<std::size_t>(width));
            for (const auto& [index, value] : rowCells) {
                row[static_cast<std::size_t>(index)] = value;
            }
            rows.push_back(std::move(row));
            if (rows.size() > kMaxSheetRows) {
                throw std::runtime_error("Worksheet row limit exceeded");
            }
        }
    }
    return rows;
}

} // namespace

bool XlsxImporter::supports(const std::string& path) const {
    return plotapp::text::toLower(std::filesystem::path(path).extension().string()) == ".xlsx";
}

TableData XlsxImporter::load(const std::string& path) const {
    unzFile archive = unzOpen(path.c_str());
    if (!archive) {
        throw std::runtime_error("Cannot open xlsx archive: " + path);
    }

    try {
        std::vector<std::string> sharedStrings;
        try {
            sharedStrings = parseSharedStrings(readZipEntry(archive, "xl/sharedStrings.xml"));
        } catch (...) {
            sharedStrings.clear();
        }
        const auto workbookXml = readZipEntry(archive, "xl/workbook.xml");
        const auto relsXml = readZipEntry(archive, "xl/_rels/workbook.xml.rels");
        const auto [sheetName, relId] = firstSheetInfo(workbookXml);
        const auto sheetTarget = resolveSheetTarget(relsXml, relId);
        const auto sheetXml = readZipEntry(archive, sheetTarget);

        TableData table;
        table.sourcePath = path;
        table.sheetName = sheetName;
        auto rows = parseSheetRows(sheetXml, sharedStrings);
        if (rows.empty()) {
            throw std::runtime_error("No rows found in xlsx sheet");
        }

        bool headerLikely = false;
        for (const auto& cell : rows.front()) {
            bool ok = false;
            plotapp::text::toDouble(cell, &ok);
            if (!ok && !plotapp::text::trim(cell).empty()) {
                headerLikely = true;
                break;
            }
        }
        if (headerLikely) {
            table.headers = rows.front();
            rows.erase(rows.begin());
        } else {
            table.headers.reserve(rows.front().size());
            for (std::size_t i = 0; i < rows.front().size(); ++i) {
                table.headers.push_back("col" + std::to_string(i + 1));
            }
        }
        for (auto& row : rows) {
            row.resize(table.headers.size());
            table.rows.push_back(std::move(row));
        }
        unzClose(archive);
        return table;
    } catch (...) {
        unzClose(archive);
        throw;
    }
}

} // namespace plotapp
