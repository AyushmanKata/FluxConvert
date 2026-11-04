#include "converter/format_handler.hpp"
#include "converter/pdf_writer.hpp"
#include "converter/pdf_renderer.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/error.hpp"
#include "converter/format_registry.hpp"
#include <memory>
#include <filesystem>
#include <chrono>

namespace converter {

class PdfHandler : public FormatHandler {
public:
    Format get_format() const override {
        return Format::PDF;
    }

    FormatCapabilities get_capabilities() const override {
        FormatCapabilities caps;
        caps.can_read = true;
        caps.can_write = true;
        caps.supports_multipage = true;
        caps.supports_transparency = false; // PDF images don't easily support alpha
        return caps;
    }

    bool can_convert(Format from, Format to) const override {
        // Can convert PNG or JPEG to PDF
        if (to == Format::PDF) {
            return from == Format::PNG || from == Format::JPEG;
        }
        // Can convert PDF to PNG or JPEG
        if (from == Format::PDF) {
            return to == Format::PNG || to == Format::JPEG;
        }
        return false;
    }

    FileConversionResult convert(
        const std::string& input_path,
        const std::string& output_path,
        const ConversionRequest& request
    ) override {
        FileConversionResult result;
        result.input_path = input_path;
        result.output_path = output_path;

        auto start_time = std::chrono::steady_clock::now();

        try {
            if (request.output_format == Format::PDF) {
                // Single image to PDF
                std::vector<std::string> images = {input_path};
                images_to_pdf(images, output_path, request.pdf_write_options);

                result.success = true;
                if (std::filesystem::exists(output_path)) {
                    result.bytes_written = std::filesystem::file_size(output_path);
                }
                if (std::filesystem::exists(input_path)) {
                    result.bytes_read = std::filesystem::file_size(input_path);
                }
            } else if (request.output_format == Format::PNG || request.output_format == Format::JPEG) {
                // PDF to image(s)
                std::vector<std::string> generated = pdf_to_images(input_path, output_path, request);
                result.success = true;

                if (!generated.empty()) {
                    result.output_path = generated[0];
                    size_t total_written = 0;
                    for (const auto& path : generated) {
                        if (std::filesystem::exists(path)) {
                            total_written += std::filesystem::file_size(path);
                        }
                    }
                    result.bytes_written = total_written;
                }
                if (std::filesystem::exists(input_path)) {
                    result.bytes_read = std::filesystem::file_size(input_path);
                }
            } else {
                throw ConversionError("Unsupported conversion request for PDF handler");
            }
        } catch (const ConversionError& e) {
            result.success = false;
            result.error_message = e.what();
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("PDF conversion failed: ") + e.what();
        }

        auto end_time = std::chrono::steady_clock::now();
        result.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        return result;
    }

    // Multi-file to single PDF
    FileConversionResult convert_multiple(
        const std::vector<std::string>& input_paths,
        const std::string& output_path,
        const ConversionRequest& request
    ) override {
        FileConversionResult result;
        result.output_path = output_path;

        auto start_time = std::chrono::steady_clock::now();

        try {
            if (input_paths.empty()) {
                throw ConversionError("No input files provided");
            }

            // Multiple images to single PDF
            images_to_pdf(input_paths, output_path, request.pdf_write_options);

            result.success = true;
            result.input_path = input_paths[0]; // Primary input
            if (std::filesystem::exists(output_path)) {
                result.bytes_written = std::filesystem::file_size(output_path);
            }
            size_t total_read = 0;
            for (const auto& in_path : input_paths) {
                if (std::filesystem::exists(in_path)) {
                    total_read += std::filesystem::file_size(in_path);
                }
            }
            result.bytes_read = total_read;
        } catch (const ConversionError& e) {
            result.success = false;
            result.error_message = e.what();
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("PDF conversion failed: ") + e.what();
        }

        auto end_time = std::chrono::steady_clock::now();
        result.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        return result;
    }
};

// Explicit registration function
void register_pdf_handler(FormatRegistry& registry) {
    registry.register_handler(std::make_unique<PdfHandler>());
}

void register_pdf_handler() {
    register_pdf_handler(FormatRegistry::instance());
}

} // namespace converter
