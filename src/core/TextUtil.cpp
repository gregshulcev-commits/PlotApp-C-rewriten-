#include "plotapp/TextUtil.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <charconv>
#include <sstream>
#include <stdexcept>

namespace plotapp::text {

std::string trim(const std::string& value) {
    auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> out;
    std::string current;
    std::stringstream stream(value);
    while (std::getline(stream, current, delimiter)) {
        out.push_back(current);
    }
    if (!value.empty() && value.back() == delimiter) {
        out.emplace_back();
    }
    return out;
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::string escape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == ' ') {
            out << static_cast<char>(c);
        } else {
            const char* digits = "0123456789ABCDEF";
            out << '%' << digits[c >> 4] << digits[c & 0xF];
        }
    }
    return out.str();
}

std::string unescape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            int hi = hexValue(value[i + 1]);
            int lo = hexValue(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(value[i]);
    }
    return out;
}

double toDouble(const std::string& value, bool* ok) {
    auto trimmed = trim(value);
    double out = 0.0;
    auto begin = trimmed.data();
    auto end = trimmed.data() + trimmed.size();
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    auto result = std::from_chars(begin, end, out);
    bool success = result.ec == std::errc() && result.ptr == end && std::isfinite(out);
    if (ok) *ok = success;
    if (!success) return 0.0;
    return out;
#else
    try {
        std::size_t consumed = 0;
        out = std::stod(trimmed, &consumed);
        bool success = consumed == trimmed.size() && std::isfinite(out);
        if (ok) *ok = success;
        return success ? out : 0.0;
    } catch (...) {
        if (ok) *ok = false;
        return 0.0;
    }
#endif
}

std::vector<std::string> shellSplit(const std::string& value) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    char quoteChar = '\0';
    for (std::size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if ((c == '"' || c == '\'') && (!inQuotes || c == quoteChar)) {
            if (inQuotes && c == quoteChar) {
                inQuotes = false;
                quoteChar = '\0';
            } else if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            }
            continue;
        }
        if (!inQuotes && std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

} // namespace plotapp::text
