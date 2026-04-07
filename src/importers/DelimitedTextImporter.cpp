#include "plotapp/Importer.h"
#include "plotapp/TextUtil.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace plotapp {
namespace {

constexpr std::uintmax_t kMaxDelimitedFileBytes = 16u * 1024u * 1024u;
constexpr std::size_t kMaxDelimitedRows = 200'000;

std::string trimBom(std::string line) {
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
    return line;
}

bool hasBinarySignature(const std::string& sample) {
    const std::string png("\x89PNG\r\n\x1A\n", 8);
    const std::string gif87("GIF87a", 6);
    const std::string gif89("GIF89a", 6);
    const std::string pdf("%PDF", 4);
    const std::string bmp("BM", 2);
    const std::string jpeg("\xFF\xD8\xFF", 3);
    const std::string zip("PK\x03\x04", 4);
    const std::string riff("RIFF", 4);

    return plotapp::text::startsWith(sample, png)
        || plotapp::text::startsWith(sample, gif87)
        || plotapp::text::startsWith(sample, gif89)
        || plotapp::text::startsWith(sample, pdf)
        || plotapp::text::startsWith(sample, bmp)
        || plotapp::text::startsWith(sample, jpeg)
        || plotapp::text::startsWith(sample, zip)
        || plotapp::text::startsWith(sample, riff);
}

bool looksLikeDelimitedTextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;

    std::string sample(4096, '\0');
    input.read(sample.data(), static_cast<std::streamsize>(sample.size()));
    sample.resize(static_cast<std::size_t>(input.gcount()));
    if (sample.empty()) return false;
    if (hasBinarySignature(sample)) return false;

    std::size_t controlCount = 0;
    std::size_t newlineCount = 0;
    std::size_t separatorCount = 0;
    std::size_t numericOrAlphaCount = 0;
    for (unsigned char c : sample) {
        if (c == '\0') return false;
        if (c == '\n' || c == '\r') ++newlineCount;
        if (c == ',' || c == ';' || c == '\t' || c == '|' || c == ' ') ++separatorCount;
        if (std::isalnum(c)) ++numericOrAlphaCount;
        if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c == 0x7F) ++controlCount;
    }

    if (controlCount > sample.size() / 32) return false;
    if (numericOrAlphaCount < 4) return false;
    return newlineCount > 0 || separatorCount >= 2;
}

char detectDelimiterFromSample(const std::vector<std::string>& lines, const std::string& extension) {
    const std::array<char, 4> preferred = {',', ';', '\t', '|'};
    int bestScore = -1;
    char best = ',';

    for (char delimiter : preferred) {
        int score = 0;
        int firstCount = -1;
        bool consistent = true;
        for (const auto& line : lines) {
            const int count = static_cast<int>(std::count(line.begin(), line.end(), delimiter));
            if (count == 0) continue;
            if (firstCount < 0) firstCount = count;
            if (count != firstCount) consistent = false;
            score += count;
        }
        if (score == 0) continue;
        if (consistent) score += 5;
        if (extension == ".csv" && delimiter == ',') score += 2;
        if ((extension == ".txt" || extension == ".dat" || extension == ".tsv") && delimiter == '\t') score += 2;
        if (score > bestScore) {
            bestScore = score;
            best = delimiter;
        }
    }

    if (bestScore >= 0) return best;

    for (const auto& line : lines) {
        bool sawWhitespace = false;
        for (unsigned char c : line) {
            if (std::isspace(c)) {
                sawWhitespace = true;
                break;
            }
        }
        if (sawWhitespace) return ' ';
    }
    return ',';
}

double parseNumericCell(const std::string& raw, bool* ok) {
    double value = plotapp::text::toDouble(raw, ok);
    if (ok && *ok) return value;

    std::string normalized = plotapp::text::trim(raw);
    if (normalized.find(',') != std::string::npos && normalized.find('.') == std::string::npos) {
        std::replace(normalized.begin(), normalized.end(), ',', '.');
        value = plotapp::text::toDouble(normalized, ok);
        if (ok && *ok) return value;
    }
    return 0.0;
}

} // namespace

bool DelimitedTextImporter::supports(const std::string& path) const {
    const auto ext = text::toLower(std::filesystem::path(path).extension().string());
    if (ext == ".csv" || ext == ".txt" || ext == ".dat" || ext == ".tsv") return true;
    if (!ext.empty()) return false;
    return looksLikeDelimitedTextFile(path);
}

TableData DelimitedTextImporter::load(const std::string& path) const {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (!ec && size > kMaxDelimitedFileBytes) {
        throw std::runtime_error("Delimited text file is too large: " + path);
    }
    if (!looksLikeDelimitedTextFile(path)) {
        throw std::runtime_error("File does not look like supported tabular text data: " + path);
    }

    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    std::vector<std::string> lines;
    lines.reserve(256);
    std::string line;
    while (std::getline(input, line)) {
        line = trimBom(line);
        if (!text::trim(line).empty()) {
            lines.push_back(line);
            if (lines.size() > kMaxDelimitedRows) {
                throw std::runtime_error("Delimited text file has too many rows: " + path);
            }
        }
    }
    if (lines.empty()) {
        throw std::runtime_error("File is empty: " + path);
    }

    const auto ext = text::toLower(std::filesystem::path(path).extension().string());
    const std::vector<std::string> sampleLines(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(lines.size(), 8)));
    const char delimiter = detectDelimiterFromSample(sampleLines, ext);

    TableData table;
    table.sourcePath = path;

    auto firstRow = split(lines.front(), delimiter);
    bool headerLikely = false;
    for (const auto& cell : firstRow) {
        bool ok = false;
        parseNumericCell(cell, &ok);
        if (!ok) {
            headerLikely = true;
            break;
        }
    }

    if (headerLikely) {
        table.headers = firstRow;
        lines.erase(lines.begin());
    }

    for (const auto& raw : lines) {
        auto row = split(raw, delimiter);
        if (!table.headers.empty()) {
            row.resize(table.headers.size());
        }
        table.rows.push_back(std::move(row));
    }

    if (table.headers.empty()) {
        std::size_t maxCols = 0;
        for (const auto& row : table.rows) {
            maxCols = std::max(maxCols, row.size());
        }
        table.headers.reserve(maxCols);
        for (std::size_t i = 0; i < maxCols; ++i) {
            table.headers.push_back("col" + std::to_string(i + 1));
        }
        for (auto& row : table.rows) {
            row.resize(maxCols);
        }
    }

    return table;
}

char DelimitedTextImporter::detectDelimiter(const std::string& line, const std::string& extension) {
    return detectDelimiterFromSample({line}, extension);
}

std::vector<std::string> DelimitedTextImporter::split(const std::string& line, char delimiter) {
    std::vector<std::string> out;
    if (delimiter == ' ') {
        std::stringstream ss(line);
        std::string token;
        while (ss >> token) out.push_back(token);
        return out;
    }

    std::string current;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }
        if (c == delimiter && !inQuotes) {
            out.push_back(text::trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    out.push_back(text::trim(current));
    return out;
}

NumericSeries extractNumericSeries(const TableData& table, std::size_t xColumn, std::size_t yColumn, std::optional<std::size_t> errorColumn) {
    if (xColumn >= table.headers.size() || yColumn >= table.headers.size()) {
        throw std::runtime_error("Column index is out of range");
    }

    NumericSeries series;
    series.xLabel = table.headers[xColumn];
    series.yLabel = table.headers[yColumn];

    for (std::size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex) {
        const auto& row = table.rows[rowIndex];
        if (xColumn >= row.size() || yColumn >= row.size()) {
            ++series.skippedRows;
            continue;
        }
        bool xOk = false;
        bool yOk = false;
        const double x = parseNumericCell(row[xColumn], &xOk);
        const double y = parseNumericCell(row[yColumn], &yOk);
        if (!xOk || !yOk) {
            ++series.skippedRows;
            continue;
        }

        double errorValue = 0.0;
        bool errorOk = true;
        if (errorColumn.has_value()) {
            if (*errorColumn >= row.size()) {
                ++series.skippedRows;
                continue;
            }
            errorValue = parseNumericCell(row[*errorColumn], &errorOk);
            if (!errorOk) {
                ++series.skippedRows;
                continue;
            }
        }

        series.points.push_back(Point{x, y});
        series.rowIndices.push_back(rowIndex);
        if (errorColumn.has_value()) series.errorValues.push_back(errorValue);
    }

    if (series.points.empty()) {
        throw std::runtime_error("No numeric rows could be extracted from the selected columns");
    }
    return series;
}

} // namespace plotapp
