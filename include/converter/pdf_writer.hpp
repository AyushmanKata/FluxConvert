#pragma once

#include "image_data.hpp"
#include "conversion_request.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace converter {

// PDF page dimensions in points (1 point = 1/72 inch)
struct PageDimensions {
    double width_pt;   // Width in points
    double height_pt;  // Height in points
};

// Standard page sizes
namespace PageSizes {
    constexpr PageDimensions A4_PORTRAIT = {595.0, 842.0};    // 210mm × 297mm
    constexpr PageDimensions A4_LANDSCAPE = {842.0, 595.0};
    constexpr PageDimensions LETTER_PORTRAIT = {612.0, 792.0}; // 8.5" × 11"
    constexpr PageDimensions LETTER_LANDSCAPE = {792.0, 612.0};
}

// Convert millimeters to points
constexpr double mm_to_pt(double mm) {
    return mm * 72.0 / 25.4;
}

// Image placement on PDF page
struct ImagePlacement {
    double x;          // X position in points
    double y;          // Y position in points
    double width;      // Scaled width in points
    double height;     // Scaled height in points
};

// Calculate how to place an image on a page
ImagePlacement calculate_placement(
    uint32_t image_width,
    uint32_t image_height,
    const PageDimensions& page,
    double margin_pt,
    PDFWriteOptions::Orientation orientation
);

// PDF document builder - creates PDF from images
class PdfWriter {
public:
    PdfWriter();
    ~PdfWriter();

    // Add a page with an image
    void add_page(
        const ImageData& image,
        const PageDimensions& page_size,
        double margin_pt,
        PDFWriteOptions::Orientation orientation
    );

    // Add a page from a JPEG file (direct embedding without re-encoding)
    void add_jpeg_page(
        const std::string& jpeg_path,
        uint32_t width,
        uint32_t height,
        const PageDimensions& page_size,
        double margin_pt,
        PDFWriteOptions::Orientation orientation
    );

    // Write the complete PDF to a file
    void write_to_file(const std::string& output_path);

    // Get the number of pages
    size_t page_count() const { return pages_.size(); }

private:
    struct ImageObject {
        uint32_t obj_id;
        uint32_t width;
        uint32_t height;
        uint8_t color_components;
        std::vector<uint8_t> data;
        bool is_jpeg;
    };

    struct Page {
        uint32_t obj_id;
        PageDimensions dimensions;
        ImagePlacement placement;
        uint32_t image_ref;
    };

    std::vector<Page> pages_;
    std::vector<ImageObject> images_;

    uint32_t next_object_id_ = 1;

    uint32_t allocate_object_id();
};

// High-level API: Convert images to PDF
void images_to_pdf(
    const std::vector<std::string>& image_paths,
    const std::string& output_path,
    const PDFWriteOptions& options
);

} // namespace converter
