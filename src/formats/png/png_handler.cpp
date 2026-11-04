#include "converter/format_handler.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/error.hpp"
#include "converter/format_registry.hpp"
#include <memory>

namespace converter {

class PngHandler : public FormatHandler {
public:
    Format get_format() const override {
        return Format::PNG;
    }

    FormatCapabilities get_capabilities() const override {
        FormatCapabilities caps;
        caps.can_read = true;
        caps.can_write = true;
        caps.supports_multipage = false;
        caps.supports_transparency = true;
        return caps;
    }

    bool can_convert(Format from, Format to) const override {
        if (from != Format::PNG) {
            return false;
        }
        // PNG handler handles image-to-image conversions only.
        // PNG → PDF is handled by PdfHandler.
        return to == Format::JPEG || to == Format::PNG;
    }

    FileConversionResult convert(
        const std::string& input_path,
        const std::string& output_path,
        const ConversionRequest& request
    ) override {
        FileConversionResult result;
        result.input_path = input_path;
        result.output_path = output_path;

        try {
            // Decode PNG
            ImageData image = png_decode(input_path);

            // Convert based on target format
            if (request.output_format == Format::JPEG) {
                // Convert to JPEG with background color
                JpegOptions jpeg_opts;
                jpeg_opts.quality = request.jpeg_options.quality;

                // Extract RGB from background color
                uint32_t bg = request.jpeg_options.background_color;
                jpeg_opts.background_color = {
                    static_cast<uint8_t>((bg >> 16) & 0xFF),  // R
                    static_cast<uint8_t>((bg >> 8) & 0xFF),   // G
                    static_cast<uint8_t>(bg & 0xFF)           // B
                };

                jpeg_encode(image, output_path, jpeg_opts);
            } else if (request.output_format == Format::PNG) {
                // PNG to PNG (recompress with different options)
                PngOptions png_opts;
                png_opts.compression_level = request.png_options.compression_level;
                png_encode(image, output_path, png_opts);
            } else {
                throw ConversionError("PNG handler: Unsupported target format");
            }

            result.success = true;
        } catch (const ConversionError& e) {
            result.success = false;
            result.error_message = e.what();
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("PNG conversion failed: ") + e.what();
        }

        return result;
    }
};

// Explicit registration function
void register_png_handler(FormatRegistry& registry) {
    registry.register_handler(std::make_unique<PngHandler>());
}

void register_png_handler() {
    register_png_handler(FormatRegistry::instance());
}

} // namespace converter
