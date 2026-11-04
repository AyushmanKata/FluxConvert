#pragma once

#include <cstdint>
#include <cstddef>

namespace converter {

// Resource limit constants
constexpr uint32_t MAX_IMAGE_WIDTH = 32768;
constexpr uint32_t MAX_IMAGE_HEIGHT = 32768;
constexpr uint64_t MAX_PIXEL_COUNT = 256 * 1024 * 1024;  // 256 megapixels
constexpr size_t MAX_IMAGE_MEMORY = 4ULL * 1024 * 1024 * 1024;  // 4 GB
constexpr size_t MAX_PDF_PAGES = 1000;
constexpr int MIN_DPI = 72;
constexpr int MAX_DPI = 600;
constexpr int DEFAULT_DPI = 150;

// Validate image dimensions against resource limits
void validate_dimensions(uint32_t width, uint32_t height);

// Validate total pixel count
void validate_pixel_count(uint32_t width, uint32_t height);

// Estimate memory required for image buffer
size_t estimate_memory(uint32_t width, uint32_t height, uint8_t channels);

// Comprehensive validation before allocation
void validate_image_resources(uint32_t width, uint32_t height, uint8_t channels);

// Validate PDF rendering DPI
void validate_dpi(int dpi);

// Validate PDF page count
void validate_pdf_page_count(size_t page_count);

} // namespace converter
