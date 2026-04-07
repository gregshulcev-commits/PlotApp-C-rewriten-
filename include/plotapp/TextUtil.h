#pragma once

#include <string>
#include <vector>

namespace plotapp::text {

std::string trim(const std::string& value);
std::vector<std::string> split(const std::string& value, char delimiter);
std::string toLower(std::string value);
bool startsWith(const std::string& value, const std::string& prefix);
std::string escape(const std::string& value);
std::string unescape(const std::string& value);
double toDouble(const std::string& value, bool* ok = nullptr);
std::vector<std::string> shellSplit(const std::string& value);

} // namespace plotapp::text
