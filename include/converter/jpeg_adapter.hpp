#pragma once

#include "image_data.hpp"
#include <string>
#include <array>

namespace converter {

// JPEG encoding options
struct JpegOptions {
    int quality = 90;  // 1-100, default 90
    std::array<uint8_t, 3> background_color = {255, 255, 255};  // White default for transparency
};

// Decode JPEG file to ImageData (always RGB, no alpha)
ImageData jpeg_decode(const std::string& path);

// Encode ImageData to JPEG file (converts RGBA to RGB using background color)
void jpeg_encode(const ImageData& image, const std::string& path, const JpegOptions& options = {});

// Get JPEG metadata without full decode
ImageInfo jpeg_get_info(const std::string& path);

} // namespace converter
