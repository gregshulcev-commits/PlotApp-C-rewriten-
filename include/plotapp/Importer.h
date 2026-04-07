#pragma once

#include "plotapp/Project.h"

#include <optional>
#include <string>
#include <vector>

namespace plotapp {

struct TableData {
    std::string sourcePath;
    std::string sheetName;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

struct NumericSeries {
    std::string xLabel;
    std::string yLabel;
    std::vector<Point> points;
    std::vector<double> errorValues;
    std::vector<std::size_t> rowIndices;
    std::size_t skippedRows {0};
};

class Importer {
public:
    virtual ~Importer() = default;
    virtual bool supports(const std::string& path) const = 0;
    virtual TableData load(const std::string& path) const = 0;
};

class DelimitedTextImporter final : public Importer {
public:
    bool supports(const std::string& path) const override;
    TableData load(const std::string& path) const override;

private:
    static char detectDelimiter(const std::string& line, const std::string& extension);
    static std::vector<std::string> split(const std::string& line, char delimiter);
};

class XlsxImporter final : public Importer {
public:
    bool supports(const std::string& path) const override;
    TableData load(const std::string& path) const override;
};

NumericSeries extractNumericSeries(const TableData& table, std::size_t xColumn, std::size_t yColumn, std::optional<std::size_t> errorColumn = std::nullopt);

} // namespace plotapp
