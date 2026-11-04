#include "converter/format.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>

namespace converter {

Format extension_to_format(std::string_view extension) {
    // Convert to lowercase for comparison
    std::string ext_lower;
    ext_lower.reserve(extension.size());

    // Remove leading dot if present
    size_t start = (extension.empty() || extension[0] != '.') ? 0 : 1;

    for (size_t i = start; i < extension.size(); ++i) {
        ext_lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(extension[i]))));
    }

    if (ext_lower == "png") return Format::PNG;
    if (ext_lower == "jpg" || ext_lower == "jpeg") return Format::JPEG;
    if (ext_lower == "pdf") return Format::PDF;

    return Format::Unknown;
}

Format detect_format_from_buffer(const uint8_t* data, size_t size) {
    if (size < 3) return Format::Unknown;

    // JPEG signature: FF D8 FF (check first since it only needs 3 bytes)
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return Format::JPEG;
    }

    // PDF signature: %PDF- (needs 5 bytes)
    if (size >= 5 && data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F' && data[4] == '-') {
        return Format::PDF;
    }

    // PNG signature: 89 50 4E 47 0D 0A 1A 0A (needs 8 bytes)
    if (size >= 8 &&
        data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
        return Format::PNG;
    }

    return Format::Unknown;
}

Format detect_format_from_file(std::string_view path) {
    std::ifstream file(std::string(path), std::ios::binary);
    if (!file) return Format::Unknown;

    uint8_t buffer[8];
    file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
    size_t bytes_read = static_cast<size_t>(file.gcount());

    return detect_format_from_buffer(buffer, bytes_read);
}

} // namespace converter
