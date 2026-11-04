#include "converter/image_data.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include <iostream>
#include <cstdlib>

using namespace converter;

// Generate a simple RGB gradient image
ImageData generate_rgb_gradient(uint32_t width, uint32_t height) {
    ImageData image;
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.pixels.resize(image.expected_size());

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t* pixel = image.get_pixel(x, y);
            pixel[0] = static_cast<uint8_t>(255 * x / width);        // R gradient
            pixel[1] = static_cast<uint8_t>(255 * y / height);       // G gradient
            pixel[2] = static_cast<uint8_t>(128);                    // B constant
        }
    }

    return image;
}

// Generate an RGBA image with transparency gradient
ImageData generate_rgba_gradient(uint32_t width, uint32_t height) {
    ImageData image;
    image.width = width;
    image.height = height;
    image.channels = 4;
    image.pixels.resize(image.expected_size());

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t* pixel = image.get_pixel(x, y);
            pixel[0] = static_cast<uint8_t>(255);                    // R constant (red)
            pixel[1] = static_cast<uint8_t>(128 * x / width);        // G gradient
            pixel[2] = static_cast<uint8_t>(255 * y / height);       // B gradient
            pixel[3] = static_cast<uint8_t>(255 * x / width);        // A gradient (left transparent, right opaque)
        }
    }

    return image;
}

// Generate a solid color image
ImageData generate_solid_color(uint32_t width, uint32_t height, uint8_t r, uint8_t g, uint8_t b) {
    ImageData image;
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.pixels.resize(image.expected_size());

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t* pixel = image.get_pixel(x, y);
            pixel[0] = r;
            pixel[1] = g;
            pixel[2] = b;
        }
    }

    return image;
}

int main() {
    try {
        std::cout << "Generating test fixtures...\n";

        // 1. Small RGB PNG (100x100)
        std::cout << "  Creating test_rgb.png (100x100)...\n";
        ImageData rgb = generate_rgb_gradient(100, 100);
        png_encode(rgb, "tests/fixtures/test_rgb.png");

        // 2. Small RGBA PNG with transparency (100x100)
        std::cout << "  Creating test_rgba.png (100x100 with alpha)...\n";
        ImageData rgba = generate_rgba_gradient(100, 100);
        png_encode(rgba, "tests/fixtures/test_rgba.png");

        // 3. Small JPEG (100x100)
        std::cout << "  Creating test.jpg (100x100)...\n";
        ImageData rgb2 = generate_solid_color(100, 100, 128, 200, 64);
        jpeg_encode(rgb2, "tests/fixtures/test.jpg");

        // 4. Larger PNG for memory tests (2000x2000)
        std::cout << "  Creating test_large.png (2000x2000)...\n";
        ImageData large = generate_rgb_gradient(2000, 2000);
        png_encode(large, "tests/fixtures/test_large.png");

        // 5. Create corrupted files by truncating
        std::cout << "  Creating corrupt.png (truncated)...\n";
        ImageData small = generate_solid_color(50, 50, 255, 0, 0);
        png_encode(small, "tests/fixtures/corrupt.png");
        // Truncate the file
        std::system("truncate -s 100 tests/fixtures/corrupt.png");

        std::cout << "  Creating corrupt.jpg (truncated)...\n";
        jpeg_encode(small, "tests/fixtures/corrupt.jpg");
        std::system("truncate -s 100 tests/fixtures/corrupt.jpg");

        std::cout << "All test fixtures generated successfully!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error generating fixtures: " << e.what() << "\n";
        return 1;
    }
}
