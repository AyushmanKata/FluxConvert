#pragma once

#include "image_data.hpp"
#include "conversion_request.hpp"
#include <string>
#include <vector>

namespace converter {

// Information about a single page in a PDF document
struct PdfPageInfo {
    int page_number = 1;      // 1-indexed
    double width_pt = 0.0;    // Width in points (1/72 inch)
    double height_pt = 0.0;   // Height in points (1/72 inch)
};

// Information about a PDF document
struct PdfDocumentInfo {
    int page_count = 0;
    std::vector<PdfPageInfo> pages;
};

// Get the number of pages in a PDF document
int pdf_get_page_count(const std::string& path);

// Get metadata and dimensions for all pages in a PDF document
PdfDocumentInfo pdf_get_info(const std::string& path);

// Render a single page from a PDF document to ImageData (1-indexed page_number)
ImageData pdf_render_page(
    const std::string& path,
    int page_number,
    int dpi = 150
);

// Render multiple pages from a PDF document according to PDFReadOptions
// Returns vector of rendered ImageData corresponding to the selected pages
std::vector<ImageData> pdf_render_pages(
    const std::string& path,
    const PDFReadOptions& options = {}
);

// High-level API: Convert PDF pages to image files (PNG or JPEG)
// Returns list of created output image paths
std::vector<std::string> pdf_to_images(
    const std::string& pdf_path,
    const std::string& output_path,
    const ConversionRequest& request
);

} // namespace converter
