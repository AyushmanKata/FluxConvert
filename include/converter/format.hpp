#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace converter {

// Supported image and document formats
enum class Format {
    Unknown,
    PNG,
    JPEG,  // Handles both .jpg and .jpeg
    PDF
};

// Convert format to string representation
constexpr std::string_view format_to_string(Format format) {
    switch (format) {
        case Format::PNG:  return "PNG";
        case Format::JPEG: return "JPEG";
        case Format::PDF:  return "PDF";
        default:           return "Unknown";
    }
}

// Convert file extension to format (case-insensitive)
Format extension_to_format(std::string_view extension);

// Detect format from file content (magic number check)
Format detect_format_from_file(std::string_view path);

// Detect format from memory buffer
Format detect_format_from_buffer(const uint8_t* data, size_t size);

} // namespace converter
