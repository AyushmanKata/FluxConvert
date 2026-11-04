#pragma once

#include "format.hpp"
#include "progress.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace converter {

// JPEG-specific options
struct JPEGOptions {
    int quality = 90;  // 1-100
    uint32_t background_color = 0xFFFFFF;  // RGB for transparent PNG conversion
};

// PNG-specific options
struct PNGOptions {
    int compression_level = 6;  // 0-9
};

// PDF generation options
struct PDFWriteOptions {
    enum class PageSize { FitToImage, A4, Letter };
    enum class Orientation { Auto, Portrait, Landscape };

    PageSize page_size = PageSize::FitToImage;
    Orientation orientation = Orientation::Auto;
    double margin_mm = 0.0;
};

// PDF rendering options
struct PDFReadOptions {
    int dpi = 150;  // 72-600
    std::optional<int> page_number;  // nullopt = all pages
    std::optional<int> page_start;   // Range start (1-indexed)
    std::optional<int> page_end;     // Range end (inclusive)
};

// Conversion request encapsulating all parameters
struct ConversionRequest {
    // Input files
    std::vector<std::string> input_paths;

    // Output specification
    std::string output_path;  // For single file or directory for batch
    Format output_format;

    // Format-specific options
    JPEGOptions jpeg_options;
    PNGOptions png_options;
    PDFWriteOptions pdf_write_options;
    PDFReadOptions pdf_read_options;

    // Progress and cancellation
    ProgressCallback progress_callback;
    std::shared_ptr<CancellationToken> cancellation_token;

    // Validation
    bool validate() const;
};

} // namespace converter
