#pragma once

#include "format.hpp"
#include "conversion_request.hpp"
#include "conversion_result.hpp"

namespace converter {

// Format handler capabilities
struct FormatCapabilities {
    bool can_read = false;
    bool can_write = false;
    bool supports_multipage = false;
    bool supports_transparency = false;
};

// Abstract interface for format handlers
class FormatHandler {
public:
    virtual ~FormatHandler() = default;

    // Get format this handler manages
    virtual Format get_format() const = 0;

    // Get capabilities
    virtual FormatCapabilities get_capabilities() const = 0;

    // Check if this handler can perform a specific conversion
    virtual bool can_convert(Format from, Format to) const = 0;

    // Perform conversion (may throw ConversionError)
    virtual FileConversionResult convert(
        const std::string& input_path,
        const std::string& output_path,
        const ConversionRequest& request
    ) = 0;

    // For multi-file to single file (e.g., images to PDF)
    virtual FileConversionResult convert_multiple(
        const std::vector<std::string>& input_paths,
        const std::string& output_path,
        const ConversionRequest& request
    );
};

} // namespace converter
