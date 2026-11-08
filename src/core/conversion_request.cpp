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

    // Never allow an output path to overwrite one of the source files.
    // Normalize relative paths so equivalent spellings are treated alike.
    const auto normalized_path = [](const std::filesystem::path& path) {
        std::error_code ec;
        const auto absolute = std::filesystem::absolute(path, ec);
        return (ec ? path : absolute).lexically_normal();
    };
    const auto normalized_output = normalized_path(output_path);
    for (const auto& input : input_paths) {
        std::error_code equivalent_error;
        const bool refers_to_same_file =
            std::filesystem::exists(output_path, equivalent_error) &&
            !equivalent_error &&
            std::filesystem::equivalent(input, output_path, equivalent_error) &&
            !equivalent_error;
        if (normalized_path(input) == normalized_output || refers_to_same_file) {
            return false;
        }
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
