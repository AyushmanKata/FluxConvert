#include "converter/pdf_writer.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/format_handler.hpp"
#include "converter/format_registry.hpp"
#include "converter/conversion_request.hpp"
#include "converter/error.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>

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

// Helper: Check if file is a valid PDF
bool is_valid_pdf(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    char header[8];
    file.read(header, 5);

    // Check for PDF magic number
    return file && std::string(header, 5) == "%PDF-";
}

// Helper: Get file size
size_t get_file_size(const std::string& path) {
    return fs::file_size(path);
}

void test_single_jpeg_to_pdf() {
    TEST("Single JPEG to PDF conversion") {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::FitToImage;
        opts.margin_mm = 10.0;

        std::vector<std::string> inputs = {"tests/fixtures/test.jpg"};
        images_to_pdf(inputs, "tests/output/single_jpeg.pdf", opts);

        assert(fs::exists("tests/output/single_jpeg.pdf"));
        assert(is_valid_pdf("tests/output/single_jpeg.pdf"));

        // PDF should be relatively small (JPEG is embedded directly)
        size_t pdf_size = get_file_size("tests/output/single_jpeg.pdf");
        assert(pdf_size > 500);  // Has content
        assert(pdf_size < 10000); // Not huge

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_single_png_to_pdf() {
    TEST("Single PNG to PDF conversion") {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::FitToImage;
        opts.margin_mm = 10.0;

        std::vector<std::string> inputs = {"tests/fixtures/test_rgb.png"};
        images_to_pdf(inputs, "tests/output/single_png.pdf", opts);

        assert(fs::exists("tests/output/single_png.pdf"));
        assert(is_valid_pdf("tests/output/single_png.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_multi_page_pdf() {
    TEST("Multi-page PDF from multiple images") {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::A4;
        opts.orientation = PDFWriteOptions::Orientation::Auto;
        opts.margin_mm = 15.0;

        std::vector<std::string> inputs = {
            "tests/fixtures/test.jpg",
            "tests/fixtures/test_rgb.png",
            "tests/fixtures/test_rgba.png"
        };

        images_to_pdf(inputs, "tests/output/multi_page.pdf", opts);

        assert(fs::exists("tests/output/multi_page.pdf"));
        assert(is_valid_pdf("tests/output/multi_page.pdf"));

        // Multi-page PDF should be larger than single page
        size_t multi_size = get_file_size("tests/output/multi_page.pdf");
        size_t single_size = get_file_size("tests/output/single_jpeg.pdf");
        assert(multi_size > single_size);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_a4_page_size() {
    TEST("PDF with A4 page size") {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::A4;
        opts.orientation = PDFWriteOptions::Orientation::Portrait;
        opts.margin_mm = 10.0;

        std::vector<std::string> inputs = {"tests/fixtures/test.jpg"};
        images_to_pdf(inputs, "tests/output/a4_portrait.pdf", opts);

        assert(fs::exists("tests/output/a4_portrait.pdf"));
        assert(is_valid_pdf("tests/output/a4_portrait.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_letter_page_size() {
    TEST("PDF with Letter page size") {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::Letter;
        opts.orientation = PDFWriteOptions::Orientation::Landscape;
        opts.margin_mm = 20.0;

        std::vector<std::string> inputs = {"tests/fixtures/test_rgb.png"};
        images_to_pdf(inputs, "tests/output/letter_landscape.pdf", opts);

        assert(fs::exists("tests/output/letter_landscape.pdf"));
        assert(is_valid_pdf("tests/output/letter_landscape.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_large_image_to_pdf() {
    TEST("Large image (2000x2000) to PDF") {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::FitToImage;
        opts.margin_mm = 5.0;

        std::vector<std::string> inputs = {"tests/fixtures/test_large.png"};
        images_to_pdf(inputs, "tests/output/large_image.pdf", opts);

        assert(fs::exists("tests/output/large_image.pdf"));
        assert(is_valid_pdf("tests/output/large_image.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_transparency_handling() {
    TEST("PNG with transparency to PDF (alpha blending)") {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::A4;
        opts.margin_mm = 10.0;

        std::vector<std::string> inputs = {"tests/fixtures/test_rgba.png"};
        images_to_pdf(inputs, "tests/output/transparent_to_pdf.pdf", opts);

        assert(fs::exists("tests/output/transparent_to_pdf.pdf"));
        assert(is_valid_pdf("tests/output/transparent_to_pdf.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_pdf_writer_api() {
    TEST("PdfWriter API direct usage") {
        PdfWriter writer;

        // Add a JPEG page
        ImageInfo jpeg_info = jpeg_get_info("tests/fixtures/test.jpg");
        writer.add_jpeg_page(
            "tests/fixtures/test.jpg",
            jpeg_info.width,
            jpeg_info.height,
            PageSizes::A4_PORTRAIT,
            mm_to_pt(10.0),
            PDFWriteOptions::Orientation::Auto
        );

        // Add a PNG page
        ImageData png_img = png_decode("tests/fixtures/test_rgb.png");
        writer.add_page(
            png_img,
            PageSizes::LETTER_PORTRAIT,
            mm_to_pt(15.0),
            PDFWriteOptions::Orientation::Auto
        );

        assert(writer.page_count() == 2);

        writer.write_to_file("tests/output/api_direct.pdf");

        assert(fs::exists("tests/output/api_direct.pdf"));
        assert(is_valid_pdf("tests/output/api_direct.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_format_handler_jpeg_to_pdf() {
    TEST("FormatHandler: JPEG to PDF via registry") {
        FormatRegistry& registry = FormatRegistry::instance();
        FormatHandler* handler = registry.find_handler_for_conversion(Format::JPEG, Format::PDF);

        assert(handler != nullptr);
        assert(handler->can_convert(Format::JPEG, Format::PDF));

        ConversionRequest request;
        request.output_format = Format::PDF;
        request.pdf_write_options.page_size = PDFWriteOptions::PageSize::A4;
        request.pdf_write_options.margin_mm = 10.0;

        FileConversionResult result = handler->convert(
            "tests/fixtures/test.jpg",
            "tests/output/handler_jpeg_to_pdf.pdf",
            request
        );

        if (!result.success) {
            std::cerr << "Conversion failed: " << result.error_message << "\n";
        }

        assert(result.success);
        assert(fs::exists("tests/output/handler_jpeg_to_pdf.pdf"));
        assert(is_valid_pdf("tests/output/handler_jpeg_to_pdf.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_format_handler_png_to_pdf() {
    TEST("FormatHandler: PNG to PDF via registry") {
        FormatRegistry& registry = FormatRegistry::instance();
        FormatHandler* handler = registry.find_handler_for_conversion(Format::PNG, Format::PDF);

        assert(handler != nullptr);
        assert(handler->can_convert(Format::PNG, Format::PDF));

        ConversionRequest request;
        request.output_format = Format::PDF;
        request.pdf_write_options.page_size = PDFWriteOptions::PageSize::Letter;
        request.pdf_write_options.margin_mm = 20.0;

        FileConversionResult result = handler->convert(
            "tests/fixtures/test_rgb.png",
            "tests/output/handler_png_to_pdf.pdf",
            request
        );

        assert(result.success);
        assert(fs::exists("tests/output/handler_png_to_pdf.pdf"));
        assert(is_valid_pdf("tests/output/handler_png_to_pdf.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_format_handler_multi_page() {
    TEST("FormatHandler: Multiple images to single PDF") {
        FormatRegistry& registry = FormatRegistry::instance();
        FormatHandler* handler = registry.get_handler(Format::PDF);

        assert(handler != nullptr);

        ConversionRequest request;
        request.output_format = Format::PDF;
        request.pdf_write_options.page_size = PDFWriteOptions::PageSize::A4;
        request.pdf_write_options.margin_mm = 10.0;

        std::vector<std::string> inputs = {
            "tests/fixtures/test.jpg",
            "tests/fixtures/test_rgb.png"
        };

        FileConversionResult result = handler->convert_multiple(
            inputs,
            "tests/output/handler_multi_page.pdf",
            request
        );

        assert(result.success);
        assert(fs::exists("tests/output/handler_multi_page.pdf"));
        assert(is_valid_pdf("tests/output/handler_multi_page.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_empty_input() {
    TEST("Empty input list throws exception") {
        bool caught_exception = false;
        try {
            PDFWriteOptions opts;
            std::vector<std::string> inputs;
            images_to_pdf(inputs, "tests/output/should_not_exist.pdf", opts);
        } catch (const ConversionError& e) {
            caught_exception = true;
        }

        assert(caught_exception);
        assert(!fs::exists("tests/output/should_not_exist.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_invalid_input() {
    TEST("Invalid image file throws exception") {
        bool caught_exception = false;
        try {
            PDFWriteOptions opts;
            std::vector<std::string> inputs = {"tests/fixtures/corrupt.png"};
            images_to_pdf(inputs, "tests/output/should_fail.pdf", opts);
        } catch (const std::exception& e) {
            caught_exception = true;
        }

        assert(caught_exception);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_default_margin_zero() {
    TEST("Default margin is 0.0 and creates valid borderless PDF") {
        PDFWriteOptions opts;
        assert(opts.margin_mm == 0.0);

        std::vector<std::string> inputs = {"tests/fixtures/test.jpg"};
        images_to_pdf(inputs, "tests/output/zero_margin.pdf", opts);

        assert(fs::exists("tests/output/zero_margin.pdf"));
        assert(is_valid_pdf("tests/output/zero_margin.pdf"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

int main() {
    std::cout << "=== PDF Generation Integration Tests ===\n\n";

    // Ensure output directory exists
    fs::create_directories("tests/output");

    // Register all built-in handlers
    converter::register_builtin_handlers();

    // Verify PDF handler is registered
    FormatRegistry& registry = FormatRegistry::instance();
    if (registry.get_handler(Format::PDF) == nullptr) {
        std::cerr << "ERROR: PDF handler not registered!\n";
        return 1;
    }
    std::cout << "✓ Handlers registered successfully\n\n";

    // Run all tests
    test_single_jpeg_to_pdf();
    test_single_png_to_pdf();
    test_multi_page_pdf();
    test_a4_page_size();
    test_letter_page_size();
    test_large_image_to_pdf();
    test_transparency_handling();
    test_pdf_writer_api();
    test_format_handler_jpeg_to_pdf();
    test_format_handler_png_to_pdf();
    test_format_handler_multi_page();
    test_empty_input();
    test_invalid_input();
    test_default_margin_zero();

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
