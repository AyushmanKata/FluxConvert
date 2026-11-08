#include "converter/resource_limits.hpp"
#include "converter/error.hpp"
#include <sstream>

namespace converter {

void validate_dimensions(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        throw ConversionError("Image dimensions cannot be zero");
    }

    if (width > MAX_IMAGE_WIDTH) {
        std::ostringstream oss;
        oss << "Image width " << width << " exceeds maximum " << MAX_IMAGE_WIDTH;
        throw ResourceLimitError(oss.str());
    }

    if (height > MAX_IMAGE_HEIGHT) {
        std::ostringstream oss;
        oss << "Image height " << height << " exceeds maximum " << MAX_IMAGE_HEIGHT;
        throw ResourceLimitError(oss.str());
    }
}

void validate_pixel_count(uint32_t width, uint32_t height) {
    uint64_t pixel_count = static_cast<uint64_t>(width) * height;

    if (pixel_count > MAX_PIXEL_COUNT) {
        std::ostringstream oss;
        oss << "Image pixel count " << pixel_count << " exceeds maximum " << MAX_PIXEL_COUNT;
        throw ResourceLimitError(oss.str());
    }
}

size_t estimate_memory(uint32_t width, uint32_t height, uint8_t channels) {
    if (channels == 0 || channels > 4) {
        throw ConversionError("Invalid channel count");
    }

    // Calculate with overflow protection
    uint64_t mem_required = static_cast<uint64_t>(width) * height * channels;

    if (mem_required > MAX_IMAGE_MEMORY) {
        std::ostringstream oss;
        oss << "Image memory requirement " << mem_required
            << " bytes exceeds maximum " << MAX_IMAGE_MEMORY;
        throw ResourceLimitError(oss.str());
    }

    return static_cast<size_t>(mem_required);
}

void validate_image_resources(uint32_t width, uint32_t height, uint8_t channels) {
    validate_dimensions(width, height);
    validate_pixel_count(width, height);
    estimate_memory(width, height, channels);  // Will throw if too large
}

void validate_dpi(int dpi) {
    if (dpi < MIN_DPI || dpi > MAX_DPI) {
        std::ostringstream oss;
        oss << "DPI " << dpi << " is out of valid range [" << MIN_DPI << ", " << MAX_DPI << "]";
        throw ResourceLimitError(oss.str());
    }
}

void validate_pdf_page_count(size_t page_count) {
    if (page_count > MAX_PDF_PAGES) {
        std::ostringstream oss;
        oss << "PDF page count " << page_count << " exceeds maximum limit " << MAX_PDF_PAGES;
        throw ConversionError(oss.str());
    }
}

} // namespace converter
