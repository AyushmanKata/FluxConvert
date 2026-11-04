#include "converter/format_handler.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/png_adapter.hpp"
#include "converter/error.hpp"
#include "converter/format_registry.hpp"
#include <memory>

namespace converter {

class JpegHandler : public FormatHandler {
public:
    Format get_format() const override {
        return Format::JPEG;
    }

    FormatCapabilities get_capabilities() const override {
        FormatCapabilities caps;
        caps.can_read = true;
        caps.can_write = true;
        caps.supports_multipage = false;
        caps.supports_transparency = false;
        return caps;
    }

    bool can_convert(Format from, Format to) const override {
        if (from != Format::JPEG) {
            return false;
        }
        // JPEG handler handles image-to-image conversions only.
        // JPEG → PDF is handled by PdfHandler.
        return to == Format::PNG || to == Format::JPEG;
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
            // Decode JPEG
            ImageData image = jpeg_decode(input_path);

            // Convert based on target format
            if (request.output_format == Format::PNG) {
                // Convert to PNG
                PngOptions png_opts;
                png_opts.compression_level = request.png_options.compression_level;
                png_encode(image, output_path, png_opts);
            } else if (request.output_format == Format::JPEG) {
                // JPEG to JPEG (recompress with different quality)
                JpegOptions jpeg_opts;
                jpeg_opts.quality = request.jpeg_options.quality;
                jpeg_encode(image, output_path, jpeg_opts);
            } else {
                throw ConversionError("JPEG handler: Unsupported target format");
            }

            result.success = true;
        } catch (const ConversionError& e) {
            result.success = false;
            result.error_message = e.what();
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("JPEG conversion failed: ") + e.what();
        }

        return result;
    }
};

// Explicit registration function
void register_jpeg_handler(FormatRegistry& registry) {
    registry.register_handler(std::make_unique<JpegHandler>());
}

void register_jpeg_handler() {
    register_jpeg_handler(FormatRegistry::instance());
}

} // namespace converter
