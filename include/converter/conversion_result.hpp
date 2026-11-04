#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>

namespace converter {

// Result of a single file conversion
struct FileConversionResult {
    std::string input_path;
    std::string output_path;
    bool success = false;
    std::string error_message;
    size_t bytes_read = 0;
    size_t bytes_written = 0;
    std::chrono::milliseconds elapsed_time{0};
};

// Result of a batch conversion operation
struct ConversionResult {
    std::vector<FileConversionResult> file_results;
    bool all_succeeded = false;
    std::chrono::milliseconds total_time{0};

    // Convenience accessors
    size_t success_count() const;
    size_t failure_count() const;
    size_t total_count() const { return file_results.size(); }

    // Get first error if any
    std::optional<std::string> first_error() const;
};

} // namespace converter
