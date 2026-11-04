#include "converter/format.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace converter;

void test_extension_to_format() {
    std::cout << "Testing extension_to_format...\n";

    assert(extension_to_format("png") == Format::PNG);
    assert(extension_to_format(".png") == Format::PNG);
    assert(extension_to_format("PNG") == Format::PNG);
    assert(extension_to_format(".PNG") == Format::PNG);

    assert(extension_to_format("jpg") == Format::JPEG);
    assert(extension_to_format("jpeg") == Format::JPEG);
    assert(extension_to_format(".jpg") == Format::JPEG);
    assert(extension_to_format("JPEG") == Format::JPEG);

    assert(extension_to_format("pdf") == Format::PDF);
    assert(extension_to_format(".pdf") == Format::PDF);

    assert(extension_to_format("txt") == Format::Unknown);
    assert(extension_to_format("") == Format::Unknown);

    std::cout << "  ✓ All extension tests passed\n";
}

void test_magic_number_detection() {
    std::cout << "Testing magic number detection...\n";

    // PNG signature
    uint8_t png_sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    assert(detect_format_from_buffer(png_sig, 8) == Format::PNG);

    // JPEG signature
    uint8_t jpeg_sig[] = {0xFF, 0xD8, 0xFF, 0xE0};
    assert(detect_format_from_buffer(jpeg_sig, sizeof(jpeg_sig)) == Format::JPEG);

    // PDF signature
    uint8_t pdf_sig[] = {'%', 'P', 'D', 'F', '-', '1', '.', '4'};
    assert(detect_format_from_buffer(pdf_sig, 8) == Format::PDF);

    // Unknown format
    uint8_t unknown[] = {0x00, 0x00, 0x00, 0x00};
    assert(detect_format_from_buffer(unknown, 4) == Format::Unknown);

    // Too small buffer
    assert(detect_format_from_buffer(png_sig, 2) == Format::Unknown);

    std::cout << "  ✓ All magic number tests passed\n";
}

void test_format_to_string() {
    std::cout << "Testing format_to_string...\n";

    assert(format_to_string(Format::PNG) == "PNG");
    assert(format_to_string(Format::JPEG) == "JPEG");
    assert(format_to_string(Format::PDF) == "PDF");
    assert(format_to_string(Format::Unknown) == "Unknown");

    std::cout << "  ✓ All string conversion tests passed\n";
}

int main() {
    std::cout << "=== Format Detection Tests ===\n";

    try {
        test_extension_to_format();
        test_magic_number_detection();
        test_format_to_string();

        std::cout << "\n✓ All format detection tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << "\n";
        return 1;
    }
}
