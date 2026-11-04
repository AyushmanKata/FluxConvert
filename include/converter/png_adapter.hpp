#pragma once

#include "image_data.hpp"
#include <string>

namespace converter {

// PNG encoding options
struct PngOptions {
    int compression_level = 6;  // 0-9, default 6
    bool preserve_transparency = true;
};

// Decode PNG file to ImageData
ImageData png_decode(const std::string& path);

// Encode ImageData to PNG file
void png_encode(const ImageData& image, const std::string& path, const PngOptions& options = {});

// Get PNG metadata without full decode
ImageInfo png_get_info(const std::string& path);

} // namespace converter
