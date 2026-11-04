#include "converter/conversion_request.hpp"
#include <filesystem>

namespace converter {

bool ConversionRequest::validate() const {
    // Must have at least one input file
    if (input_paths.empty()) {
        return false;
    }

    // All input files must exist
    for (const auto& path : input_paths) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) {
            return false;
        }
    }

    // Output path must be specified
    if (output_path.empty()) {
        return false;
    }

    // Validate format-specific options
    if (output_format == Format::JPEG) {
        if (jpeg_options.quality < 1 || jpeg_options.quality > 100) {
            return false;
        }
    }

    if (output_format == Format::PNG) {
        if (png_options.compression_level < 0 || png_options.compression_level > 9) {
            return false;
        }
    }

    if (output_format == Format::PDF) {
        if (pdf_write_options.margin_mm < 0.0) {
            return false;
        }
    }

    // Check if PDF reading is involved (input is PDF)
    bool has_pdf_input = false;
    for (const auto& path : input_paths) {
        auto ext = std::filesystem::path(path).extension().string();
        if (ext == ".pdf" || ext == ".PDF") {
            has_pdf_input = true;
            break;
        }
    }

    if (has_pdf_input) {
        if (pdf_read_options.dpi < 72 || pdf_read_options.dpi > 600) {
            return false;
        }
    }

    return true;
}

} // namespace converter
