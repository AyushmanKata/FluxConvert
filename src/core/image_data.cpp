#include "converter/image_data.hpp"
#include "converter/error.hpp"

namespace converter {

void blend_alpha(
    const ImageData& rgba_image,
    ImageData& rgb_image,
    const std::array<uint8_t, 3>& bg_color
) {
    if (!rgba_image.is_valid() || rgba_image.channels != 4) {
        throw ConversionError("Invalid RGBA source image for alpha blending");
    }

    const size_t total_pixels = static_cast<size_t>(rgba_image.width) * static_cast<size_t>(rgba_image.height);
    if (rgb_image.pixels.size() < total_pixels * 3) {
        rgb_image.width = rgba_image.width;
        rgb_image.height = rgba_image.height;
        rgb_image.channels = 3;
        rgb_image.pixels.resize(total_pixels * 3);
    }

    const uint8_t* src = rgba_image.pixels.data();
    uint8_t* dst = rgb_image.pixels.data();

    const uint32_t bg0 = bg_color[0];
    const uint32_t bg1 = bg_color[1];
    const uint32_t bg2 = bg_color[2];

    // Fast branchless integer blending:
    // val = src * a + bg * (255 - a)
    // ((val + 128) * 257) >> 16 produces exact division by 255 with round-to-nearest
    for (size_t i = 0; i < total_pixels; ++i) {
        const uint32_t a = src[3];
        const uint32_t inv_a = 255 - a;

        dst[0] = static_cast<uint8_t>(((src[0] * a + bg0 * inv_a + 128) * 257) >> 16);
        dst[1] = static_cast<uint8_t>(((src[1] * a + bg1 * inv_a + 128) * 257) >> 16);
        dst[2] = static_cast<uint8_t>(((src[2] * a + bg2 * inv_a + 128) * 257) >> 16);

        src += 4;
        dst += 3;
    }
}

ImageData convert_rgba_to_rgb(
    const ImageData& rgba_image,
    const std::array<uint8_t, 3>& background_color
) {
    if (!rgba_image.is_valid()) {
        throw ConversionError("Invalid source image data");
    }

    if (rgba_image.channels == 3) {
        return rgba_image;
    }

    if (rgba_image.channels != 4) {
        throw ConversionError("Unsupported channel count for RGB conversion (expected 4)");
    }

    ImageData rgb_image;
    rgb_image.width = rgba_image.width;
    rgb_image.height = rgba_image.height;
    rgb_image.channels = 3;
    rgb_image.pixels.resize(rgb_image.expected_size());

    blend_alpha(rgba_image, rgb_image, background_color);
    return rgb_image;
}

} // namespace converter
