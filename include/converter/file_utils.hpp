#pragma once

#include <string>

namespace converter {

// Ensures that the directory specified by path exists, creating it if necessary.
bool ensure_directory_exists(const std::string& path);

// Returns the file extension (including the leading dot), or empty string if none.
std::string get_file_extension(const std::string& path);

// Returns filename stem without extension.
std::string get_filename_without_extension(const std::string& path);

// Sanitizes a filename by replacing invalid filesystem characters with underscores.
std::string sanitize_filename(const std::string& filename);

} // namespace converter
