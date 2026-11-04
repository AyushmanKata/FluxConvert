#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/format_handler.hpp"
#include "converter/format_registry.hpp"
#include "converter/conversion_request.hpp"
#include "converter/error.hpp"
#include <iostream>
#include <filesystem>
#include <cassert>
#include <cmath>

using namespace converter;
namespace fs = std::filesystem;

int tests_run = 0;
int tests_passed = 0;

#define TEST(name) \
    std::cout << "TEST: " << name << "... "; \
    tests_run++; \
    try

#define PASS() \
    std::cout << "PASS\n"; \
    tests_passed++

#define FAIL(msg) \
    std::cout << "FAIL: " << msg << "\n"

// Helper: Compare image dimensions
bool same_dimensions(const ImageData& a, const ImageData& b) {
    return a.width == b.width && a.height == b.height;
}

// Helper: Calculate mean absolute difference between images
double image_difference(const ImageData& a, const ImageData& b) {
    if (!same_dimensions(a, b)) {
        return 1000.0; // Very different
    }

    uint64_t sum = 0;
    uint32_t min_channels = std::min(a.channels, b.channels);

    for (uint32_t y = 0; y < a.height; ++y) {
        for (uint32_t x = 0; x < a.width; ++x) {
            const uint8_t* pa = a.get_pixel(x, y);
            const uint8_t* pb = b.get_pixel(x, y);

            for (uint32_t c = 0; c < min_channels; ++c) {
                sum += std::abs(static_cast<int>(pa[c]) - static_cast<int>(pb[c]));
            }
        }
    }

    double total_pixels = a.width * a.height * min_channels;
    return sum / total_pixels;
}

void test_png_decode_encode() {
    TEST("PNG decode and re-encode preserves dimensions") {
        ImageData img1 = png_decode("tests/fixtures/test_rgb.png");
        assert(img1.width == 100);
        assert(img1.height == 100);
        assert(img1.channels == 4); // Decoded to RGBA

        png_encode(img1, "tests/output/test_png_reencoded.png");
        ImageData img2 = png_decode("tests/output/test_png_reencoded.png");

        assert(same_dimensions(img1, img2));
        double diff = image_difference(img1, img2);
        assert(diff < 1.0); // Should be nearly identical

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_jpeg_decode_encode() {
    TEST("JPEG decode and re-encode") {
        ImageData img1 = jpeg_decode("tests/fixtures/test.jpg");
        assert(img1.width == 100);
        assert(img1.height == 100);
        assert(img1.channels == 3); // JPEG is RGB

        JpegOptions opts;
        opts.quality = 90;
        jpeg_encode(img1, "tests/output/test_jpeg_reencoded.jpg", opts);

        ImageData img2 = jpeg_decode("tests/output/test_jpeg_reencoded.jpg");
        assert(same_dimensions(img1, img2));

        // JPEG is lossy, so allow some difference
        double diff = image_difference(img1, img2);
        assert(diff < 10.0);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_png_to_jpeg() {
    TEST("PNG to JPEG conversion") {
        ImageData png_img = png_decode("tests/fixtures/test_rgb.png");

        JpegOptions opts;
        opts.quality = 85;
        jpeg_encode(png_img, "tests/output/png_to_jpeg.jpg", opts);

        ImageData jpeg_img = jpeg_decode("tests/output/png_to_jpeg.jpg");
        assert(same_dimensions(png_img, jpeg_img));

        // Allow difference due to lossy compression
        double diff = image_difference(png_img, jpeg_img);
        assert(diff < 15.0);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_jpeg_to_png() {
    TEST("JPEG to PNG conversion") {
        ImageData jpeg_img = jpeg_decode("tests/fixtures/test.jpg");
        png_encode(jpeg_img, "tests/output/jpeg_to_png.png");

        ImageData png_img = png_decode("tests/output/jpeg_to_png.png");
        assert(same_dimensions(jpeg_img, png_img));

        // Should be nearly identical (PNG is lossless)
        double diff = image_difference(jpeg_img, png_img);
        assert(diff < 1.0);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_transparency_to_jpeg() {
    TEST("PNG with transparency to JPEG (background blending)") {
        ImageData rgba_img = png_decode("tests/fixtures/test_rgba.png");
        assert(rgba_img.channels == 4); // Has alpha

        JpegOptions opts;
        opts.quality = 90;
        opts.background_color = {255, 255, 255}; // White background
        jpeg_encode(rgba_img, "tests/output/rgba_to_jpeg_white_bg.jpg", opts);

        // Verify file exists and is readable
        ImageData result = jpeg_decode("tests/output/rgba_to_jpeg_white_bg.jpg");
        assert(result.width == rgba_img.width);
        assert(result.height == rgba_img.height);
        assert(result.channels == 3); // JPEG has no alpha

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_round_trip() {
    TEST("Round-trip: PNG -> JPEG -> PNG preserves dimensions") {
        ImageData original = png_decode("tests/fixtures/test_rgb.png");

        // PNG -> JPEG
        JpegOptions jpeg_opts;
        jpeg_opts.quality = 90;
        jpeg_encode(original, "tests/output/roundtrip.jpg", jpeg_opts);

        // JPEG -> PNG
        ImageData intermediate = jpeg_decode("tests/output/roundtrip.jpg");
        png_encode(intermediate, "tests/output/roundtrip.png");

        ImageData final_img = png_decode("tests/output/roundtrip.png");

        assert(same_dimensions(original, final_img));

        // Expect significant difference due to JPEG compression
        double diff = image_difference(original, final_img);
        assert(diff < 20.0); // Still recognizable

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_large_image() {
    TEST("Large image handling (2000x2000)") {
        ImageData large = png_decode("tests/fixtures/test_large.png");
        assert(large.width == 2000);
        assert(large.height == 2000);

        // Verify memory is bounded
        size_t expected_size = 2000 * 2000 * 4; // RGBA
        assert(large.pixels.size() == expected_size);

        // Convert to JPEG
        JpegOptions opts;
        opts.quality = 75;
        jpeg_encode(large, "tests/output/large_converted.jpg", opts);

        // Verify it worked
        ImageData converted = jpeg_decode("tests/output/large_converted.jpg");
        assert(same_dimensions(large, converted));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_corrupt_png() {
    TEST("Corrupted PNG file throws exception") {
        bool caught_exception = false;
        try {
            ImageData img = png_decode("tests/fixtures/corrupt.png");
        } catch (const ConversionError& e) {
            caught_exception = true;
        }

        assert(caught_exception);
        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_corrupt_jpeg() {
    TEST("Corrupted JPEG file throws exception") {
        bool caught_exception = false;
        try {
            ImageData img = jpeg_decode("tests/fixtures/corrupt.jpg");
        } catch (const ConversionError& e) {
            caught_exception = true;
        }

        assert(caught_exception);
        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_format_handler_png_to_jpeg() {
    TEST("FormatHandler: PNG to JPEG via registry") {
        FormatRegistry& registry = FormatRegistry::instance();
        FormatHandler* handler = registry.find_handler_for_conversion(Format::PNG, Format::JPEG);

        assert(handler != nullptr);
        assert(handler->can_convert(Format::PNG, Format::JPEG));

        ConversionRequest request;
        request.output_format = Format::JPEG;
        request.jpeg_options.quality = 85;
        request.jpeg_options.background_color = 0xFFFFFF;

        FileConversionResult result = handler->convert(
            "tests/fixtures/test_rgb.png",
            "tests/output/handler_png_to_jpeg.jpg",
            request
        );

        assert(result.success);
        assert(fs::exists("tests/output/handler_png_to_jpeg.jpg"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_format_handler_jpeg_to_png() {
    TEST("FormatHandler: JPEG to PNG via registry") {
        FormatRegistry& registry = FormatRegistry::instance();
        FormatHandler* handler = registry.find_handler_for_conversion(Format::JPEG, Format::PNG);

        assert(handler != nullptr);
        assert(handler->can_convert(Format::JPEG, Format::PNG));

        ConversionRequest request;
        request.output_format = Format::PNG;
        request.png_options.compression_level = 6;

        FileConversionResult result = handler->convert(
            "tests/fixtures/test.jpg",
            "tests/output/handler_jpeg_to_png.png",
            request
        );

        assert(result.success);
        assert(fs::exists("tests/output/handler_jpeg_to_png.png"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_image_info() {
    TEST("Get image info without full decode") {
        ImageInfo png_info = png_get_info("tests/fixtures/test_rgba.png");
        assert(png_info.width == 100);
        assert(png_info.height == 100);
        assert(png_info.has_transparency == true);

        ImageInfo jpeg_info = jpeg_get_info("tests/fixtures/test.jpg");
        assert(jpeg_info.width == 100);
        assert(jpeg_info.height == 100);
        assert(jpeg_info.has_transparency == false);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

int main() {
    std::cout << "=== Image Conversion Integration Tests ===\n\n";

    // Ensure output directory exists
    fs::create_directories("tests/output");

    // Register all built-in handlers
    converter::register_builtin_handlers();

    // Verify handlers are registered
    FormatRegistry& registry = FormatRegistry::instance();
    if (registry.get_handler(Format::PNG) == nullptr) {
        std::cerr << "ERROR: PNG handler not registered!\n";
        return 1;
    }
    if (registry.get_handler(Format::JPEG) == nullptr) {
        std::cerr << "ERROR: JPEG handler not registered!\n";
        return 1;
    }
    std::cout << "✓ Handlers registered successfully\n\n";

    // Run all tests
    test_png_decode_encode();
    test_jpeg_decode_encode();
    test_png_to_jpeg();
    test_jpeg_to_png();
    test_transparency_to_jpeg();
    test_round_trip();
    test_large_image();
    test_corrupt_png();
    test_corrupt_jpeg();
    test_format_handler_png_to_jpeg();
    test_format_handler_jpeg_to_png();
    test_image_info();

    // Summary
    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Tests run: " << tests_run << "\n";
    std::cout << "Tests passed: " << tests_passed << "\n";
    std::cout << "Tests failed: " << (tests_run - tests_passed) << "\n";

    if (tests_passed == tests_run) {
        std::cout << "\n✓ All tests passed!\n";
        return 0;
    } else {
        std::cout << "\n✗ Some tests failed\n";
        return 1;
    }
}
