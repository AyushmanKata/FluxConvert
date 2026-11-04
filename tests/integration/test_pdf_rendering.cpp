#include "converter/pdf_renderer.hpp"
#include "converter/pdf_writer.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/format.hpp"
#include "converter/format_handler.hpp"
#include "converter/format_registry.hpp"
#include "converter/conversion_request.hpp"
#include "converter/error.hpp"

#include <iostream>
#include <filesystem>
#include <fstream>
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

// Helper: check file size
size_t get_file_size(const std::string& path) {
    return fs::file_size(path);
}

// Setup test PDFs if not present
void setup_test_pdfs() {
    fs::create_directories("tests/output");

    // Single JPEG to PDF
    if (!fs::exists("tests/output/single_jpeg.pdf")) {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::FitToImage;
        opts.margin_mm = 5.0;
        images_to_pdf({"tests/fixtures/test.jpg"}, "tests/output/single_jpeg.pdf", opts);
    }

    // Single PNG to PDF
    if (!fs::exists("tests/output/single_png.pdf")) {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::FitToImage;
        opts.margin_mm = 5.0;
        images_to_pdf({"tests/fixtures/test_rgb.png"}, "tests/output/single_png.pdf", opts);
    }

    // Multi-page PDF (3 pages)
    if (!fs::exists("tests/output/multi_page.pdf")) {
        PDFWriteOptions opts;
        opts.page_size = PDFWriteOptions::PageSize::A4;
        opts.orientation = PDFWriteOptions::Orientation::Auto;
        opts.margin_mm = 10.0;
        images_to_pdf({
            "tests/fixtures/test.jpg",
            "tests/fixtures/test_rgb.png",
            "tests/fixtures/test_rgba.png"
        }, "tests/output/multi_page.pdf", opts);
    }
}

void test_pdf_page_count() {
    TEST("PDF page count check") {
        int single_pages = pdf_get_page_count("tests/output/single_jpeg.pdf");
        assert(single_pages == 1);

        int multi_pages = pdf_get_page_count("tests/output/multi_page.pdf");
        assert(multi_pages == 3);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_pdf_page_info() {
    TEST("PDF document and page metadata info") {
        PdfDocumentInfo info = pdf_get_info("tests/output/multi_page.pdf");
        assert(info.page_count == 3);
        assert(info.pages.size() == 3);

        for (int i = 0; i < 3; ++i) {
            assert(info.pages[static_cast<size_t>(i)].page_number == i + 1);
            // Multi-page PDF was created as A4: 595 x 842 points
            assert(info.pages[static_cast<size_t>(i)].width_pt > 500.0);
            assert(info.pages[static_cast<size_t>(i)].height_pt > 800.0);
        }

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_render_single_page() {
    TEST("Render single page to ImageData") {
        ImageData img = pdf_render_page("tests/output/single_jpeg.pdf", 1, 150);

        assert(img.is_valid());
        assert(img.width > 0);
        assert(img.height > 0);
        assert(img.channels == 3);
        assert(img.pixels.size() == img.expected_size());

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_render_dpi_scaling() {
    TEST("PDF rendering DPI scaling (72, 150, 300 DPI)") {
        ImageData img_72 = pdf_render_page("tests/output/multi_page.pdf", 1, 72);
        ImageData img_150 = pdf_render_page("tests/output/multi_page.pdf", 1, 150);
        ImageData img_300 = pdf_render_page("tests/output/multi_page.pdf", 1, 300);

        assert(img_72.is_valid());
        assert(img_150.is_valid());
        assert(img_300.is_valid());

        // Width and height should increase proportionally
        assert(img_150.width > img_72.width);
        assert(img_150.height > img_72.height);
        assert(img_300.width > img_150.width);
        assert(img_300.height > img_150.height);

        // 300 DPI should be roughly 2x 150 DPI dimensions (+- 2 pixels due to rounding)
        double ratio_w = static_cast<double>(img_300.width) / static_cast<double>(img_150.width);
        assert(std::abs(ratio_w - 2.0) < 0.05);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_pdf_to_png_single() {
    TEST("PDF to PNG conversion (single page)") {
        ConversionRequest req;
        req.output_format = Format::PNG;
        req.pdf_read_options.dpi = 150;
        req.png_options.compression_level = 6;

        std::string out = "tests/output/render_single.png";
        if (fs::exists(out)) fs::remove(out);

        std::vector<std::string> created = pdf_to_images("tests/output/single_jpeg.pdf", out, req);

        assert(created.size() == 1);
        assert(fs::exists(out));
        assert(detect_format_from_file(out) == Format::PNG);
        assert(get_file_size(out) > 0);

        // Verify decoded dimensions
        ImageInfo info = png_get_info(out);
        assert(info.width > 0);
        assert(info.height > 0);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_pdf_to_jpeg_single() {
    TEST("PDF to JPEG conversion (single page)") {
        ConversionRequest req;
        req.output_format = Format::JPEG;
        req.pdf_read_options.dpi = 150;
        req.jpeg_options.quality = 90;

        std::string out = "tests/output/render_single.jpg";
        if (fs::exists(out)) fs::remove(out);

        std::vector<std::string> created = pdf_to_images("tests/output/single_png.pdf", out, req);

        assert(created.size() == 1);
        assert(fs::exists(out));
        assert(detect_format_from_file(out) == Format::JPEG);
        assert(get_file_size(out) > 0);

        ImageInfo info = jpeg_get_info(out);
        assert(info.width > 0);
        assert(info.height > 0);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_pdf_to_jpeg_quality() {
    TEST("PDF to JPEG quality setting impacts file size") {
        ConversionRequest req_low;
        req_low.output_format = Format::JPEG;
        req_low.pdf_read_options.dpi = 150;
        req_low.jpeg_options.quality = 10;

        ConversionRequest req_high;
        req_high.output_format = Format::JPEG;
        req_high.pdf_read_options.dpi = 150;
        req_high.jpeg_options.quality = 95;

        std::string out_low = "tests/output/quality_low.jpg";
        std::string out_high = "tests/output/quality_high.jpg";

        pdf_to_images("tests/output/single_jpeg.pdf", out_low, req_low);
        pdf_to_images("tests/output/single_jpeg.pdf", out_high, req_high);

        size_t size_low = get_file_size(out_low);
        size_t size_high = get_file_size(out_high);

        assert(size_high > static_cast<size_t>(static_cast<double>(size_low) * 1.5));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_multipage_pdf_all_pages_png() {
    TEST("Extract all pages from multi-page PDF to PNG") {
        ConversionRequest req;
        req.output_format = Format::PNG;
        req.pdf_read_options.dpi = 100;

        std::string out_pattern = "tests/output/multipage_all.png";
        std::vector<std::string> created = pdf_to_images("tests/output/multi_page.pdf", out_pattern, req);

        assert(created.size() == 3);
        assert(fs::exists("tests/output/multipage_all-01.png"));
        assert(fs::exists("tests/output/multipage_all-02.png"));
        assert(fs::exists("tests/output/multipage_all-03.png"));

        for (const auto& path : created) {
            assert(detect_format_from_file(path) == Format::PNG);
            assert(get_file_size(path) > 100);
        }

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_multipage_pdf_all_pages_jpeg() {
    TEST("Extract all pages from multi-page PDF to JPEG") {
        ConversionRequest req;
        req.output_format = Format::JPEG;
        req.pdf_read_options.dpi = 100;
        req.jpeg_options.quality = 85;

        std::string out_pattern = "tests/output/multipage_all.jpg";
        std::vector<std::string> created = pdf_to_images("tests/output/multi_page.pdf", out_pattern, req);

        assert(created.size() == 3);
        assert(fs::exists("tests/output/multipage_all-01.jpg"));
        assert(fs::exists("tests/output/multipage_all-02.jpg"));
        assert(fs::exists("tests/output/multipage_all-03.jpg"));

        for (const auto& path : created) {
            assert(detect_format_from_file(path) == Format::JPEG);
            assert(get_file_size(path) > 100);
        }

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_page_number_selection() {
    TEST("Page selection: specific single page") {
        ConversionRequest req;
        req.output_format = Format::PNG;
        req.pdf_read_options.dpi = 100;
        req.pdf_read_options.page_number = 2; // Request page 2 only

        std::string out = "tests/output/page_two_only.png";
        if (fs::exists(out)) fs::remove(out);

        std::vector<std::string> created = pdf_to_images("tests/output/multi_page.pdf", out, req);

        assert(created.size() == 1);
        assert(fs::exists(out));
        assert(detect_format_from_file(out) == Format::PNG);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_page_range_selection() {
    TEST("Page selection: page range (start to end)") {
        ConversionRequest req;
        req.output_format = Format::PNG;
        req.pdf_read_options.dpi = 100;
        req.pdf_read_options.page_start = 2;
        req.pdf_read_options.page_end = 3;

        std::string out = "tests/output/range.png";
        std::vector<std::string> created = pdf_to_images("tests/output/multi_page.pdf", out, req);

        assert(created.size() == 2);
        assert(fs::exists("tests/output/range-02.png"));
        assert(fs::exists("tests/output/range-03.png"));
        assert(!fs::exists("tests/output/range-01.png"));

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_format_handler_via_registry_pdf_to_png() {
    TEST("FormatHandler: PDF to PNG via registry") {
        FormatRegistry& registry = FormatRegistry::instance();
        FormatHandler* handler = registry.find_handler_for_conversion(Format::PDF, Format::PNG);

        assert(handler != nullptr);
        assert(handler->can_convert(Format::PDF, Format::PNG));
        assert(handler->get_capabilities().can_read);

        ConversionRequest request;
        request.output_format = Format::PNG;
        request.pdf_read_options.dpi = 120;

        FileConversionResult result = handler->convert(
            "tests/output/single_jpeg.pdf",
            "tests/output/handler_rendered.png",
            request
        );

        if (!result.success) {
            std::cerr << "Handler conversion failed: " << result.error_message << "\n";
        }
        assert(result.success);
        assert(fs::exists("tests/output/handler_rendered.png"));
        assert(detect_format_from_file("tests/output/handler_rendered.png") == Format::PNG);
        assert(result.bytes_written > 0);
        assert(result.bytes_read > 0);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_format_handler_via_registry_pdf_to_jpeg() {
    TEST("FormatHandler: PDF to JPEG via registry") {
        FormatRegistry& registry = FormatRegistry::instance();
        FormatHandler* handler = registry.find_handler_for_conversion(Format::PDF, Format::JPEG);

        assert(handler != nullptr);
        assert(handler->can_convert(Format::PDF, Format::JPEG));

        ConversionRequest request;
        request.output_format = Format::JPEG;
        request.pdf_read_options.dpi = 120;
        request.jpeg_options.quality = 80;

        FileConversionResult result = handler->convert(
            "tests/output/single_png.pdf",
            "tests/output/handler_rendered.jpg",
            request
        );

        assert(result.success);
        assert(fs::exists("tests/output/handler_rendered.jpg"));
        assert(detect_format_from_file("tests/output/handler_rendered.jpg") == Format::JPEG);
        assert(result.bytes_written > 0);
        assert(result.bytes_read > 0);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_cancellation() {
    TEST("Cancellation token cancels operation") {
        auto token = std::make_shared<CancellationToken>();
        token->cancel();

        ConversionRequest req;
        req.output_format = Format::PNG;
        req.pdf_read_options.dpi = 100;
        req.cancellation_token = token;

        bool caught_cancel = false;
        try {
            pdf_to_images("tests/output/multi_page.pdf", "tests/output/cancelled.png", req);
        } catch (const CancelledException&) {
            caught_cancel = true;
        }

        assert(caught_cancel);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_invalid_page_number() {
    TEST("Invalid page number throws ValidationError") {
        bool caught = false;
        try {
            pdf_render_page("tests/output/multi_page.pdf", 99, 150);
        } catch (const ValidationError&) {
            caught = true;
        }

        assert(caught);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_invalid_dpi() {
    TEST("Invalid DPI throws ConversionError") {
        bool caught_low = false;
        try {
            pdf_render_page("tests/output/single_jpeg.pdf", 1, 10);
        } catch (const ConversionError&) {
            caught_low = true;
        }
        assert(caught_low);

        bool caught_high = false;
        try {
            pdf_render_page("tests/output/single_jpeg.pdf", 1, 1200);
        } catch (const ConversionError&) {
            caught_high = true;
        }
        assert(caught_high);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_corrupt_pdf() {
    TEST("Corrupted PDF file fails gracefully") {
        // Write invalid data
        std::ofstream corrupt("tests/output/corrupt.pdf", std::ios::binary);
        corrupt << "%PDF-1.4\ncorrupted content that cannot be parsed as valid PDF\n";
        corrupt.close();

        bool caught = false;
        try {
            pdf_get_page_count("tests/output/corrupt.pdf");
        } catch (const ConversionError&) {
            caught = true;
        }

        assert(caught);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_progress_callback() {
    TEST("Progress callback is invoked with valid percentages") {
        int call_count = 0;
        int last_percent = -1;

        ConversionRequest req;
        req.output_format = Format::PNG;
        req.pdf_read_options.dpi = 100;
        req.progress_callback = [&](int pct, const std::string& msg) {
            call_count++;
            last_percent = pct;
            assert(pct >= 0 && pct <= 100);
            assert(!msg.empty());
        };

        pdf_to_images("tests/output/multi_page.pdf", "tests/output/progress_test.png", req);

        assert(call_count == 3);
        assert(last_percent == 100);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

int main() {
    std::cout << "=== PDF Rendering Integration Tests ===\n\n";

    // Setup handlers
    converter::register_builtin_handlers();

    // Setup test PDFs
    setup_test_pdfs();
    std::cout << "✓ Test PDFs ready\n\n";

    // Run tests
    test_pdf_page_count();
    test_pdf_page_info();
    test_render_single_page();
    test_render_dpi_scaling();
    test_pdf_to_png_single();
    test_pdf_to_jpeg_single();
    test_pdf_to_jpeg_quality();
    test_multipage_pdf_all_pages_png();
    test_multipage_pdf_all_pages_jpeg();
    test_page_number_selection();
    test_page_range_selection();
    test_format_handler_via_registry_pdf_to_png();
    test_format_handler_via_registry_pdf_to_jpeg();
    test_cancellation();
    test_invalid_page_number();
    test_invalid_dpi();
    test_corrupt_pdf();
    test_progress_callback();

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
