#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace jarvis::util {

std::string trim(std::string_view value);
std::string toLower(std::string_view value);
std::vector<std::string> split(std::string_view value, char delimiter);
std::string join(const std::vector<std::string>& parts, std::string_view delimiter);
std::string readTextFile(const std::filesystem::path& path);
void writeTextFile(const std::filesystem::path& path, std::string_view content);
std::string replaceAll(std::string value, std::string_view needle, std::string_view replacement);

}  // namespace jarvis::util
