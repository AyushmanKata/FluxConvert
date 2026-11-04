#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <stdexcept>

namespace converter {

// In-memory representation of decoded image data
struct ImageData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t channels = 0;  // 3 for RGB, 4 for RGBA
    std::vector<uint8_t> pixels;  // Row-major order, channels interleaved

    // Calculate expected buffer size
    size_t expected_size() const {
        return static_cast<size_t>(width) * height * channels;
    }

    // Validate internal consistency
    bool is_valid() const {
        return width > 0 && height > 0 &&
               (channels == 3 || channels == 4) &&
               pixels.size() == expected_size();
    }

    // Get pixel at (x, y) - returns pointer to first channel
    const uint8_t* get_pixel(uint32_t x, uint32_t y) const {
        if (x >= width || y >= height) {
            throw std::out_of_range("Pixel coordinates out of bounds");
        }
        return &pixels[(y * width + x) * channels];
    }

    uint8_t* get_pixel(uint32_t x, uint32_t y) {
        if (x >= width || y >= height) {
            throw std::out_of_range("Pixel coordinates out of bounds");
        }
        return &pixels[(y * width + x) * channels];
    }
};

// Image metadata without full pixel data
struct ImageInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t channels = 0;
    bool has_transparency = false;
};

// Convert RGBA image to RGB by blending over a background color.
// Uses fast branchless integer arithmetic without divisions.
ImageData convert_rgba_to_rgb(
    const ImageData& rgba_image,
    const std::array<uint8_t, 3>& background_color = {255, 255, 255}
);

// Blend an RGBA image onto an RGB image buffer.
void blend_alpha(
    const ImageData& rgba_image,
    ImageData& rgb_image,
    const std::array<uint8_t, 3>& background_color = {255, 255, 255}
);

} // namespace converter
