#include "converter/format_handler.hpp"
#include "converter/error.hpp"

namespace converter {

FileConversionResult FormatHandler::convert_multiple(
    const std::vector<std::string>& /*input_paths*/,
    const std::string& /*output_path*/,
    const ConversionRequest& /*request*/)
{
    FileConversionResult result;
    result.success = false;
    result.error_message = "Multi-file conversion not supported by this handler";
    return result;
}

} // namespace converter
