#include "converter/conversion_result.hpp"

namespace converter {

size_t ConversionResult::success_count() const {
    size_t count = 0;
    for (const auto& result : file_results) {
        if (result.success) ++count;
    }
    return count;
}

size_t ConversionResult::failure_count() const {
    return total_count() - success_count();
}

std::optional<std::string> ConversionResult::first_error() const {
    for (const auto& result : file_results) {
        if (!result.success && !result.error_message.empty()) {
            return result.error_message;
        }
    }
    return std::nullopt;
}

} // namespace converter
