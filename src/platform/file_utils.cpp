#include "converter/file_utils.hpp"
#include <filesystem>
#include <string>

namespace converter {

namespace fs = std::filesystem;

bool ensure_directory_exists(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

std::string get_file_extension(const std::string& path) {
    fs::path p(path);
    return p.extension().string();
}

std::string get_filename_without_extension(const std::string& path) {
    fs::path p(path);
    return p.stem().string();
}

std::string sanitize_filename(const std::string& filename) {
    std::string result;
    result.reserve(filename.size());
    for (char c : filename) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            result += '_';
        } else {
            result += c;
        }
    }
    return result;
}

} // namespace converter
