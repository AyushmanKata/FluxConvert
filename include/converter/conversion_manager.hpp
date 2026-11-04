#pragma once

#include "conversion_request.hpp"
#include "conversion_result.hpp"
#include <memory>
#include <vector>

namespace converter {

class FormatRegistry;

// High-level conversion manager with batch execution support
class ConversionManager {
public:
    explicit ConversionManager(size_t worker_threads = 0, FormatRegistry* registry = nullptr);
    ~ConversionManager();

    // Perform conversion based on request (single file or concurrent batch)
    ConversionResult convert(const ConversionRequest& request);

    // Concurrency control
    size_t get_thread_count() const;
    void set_thread_count(size_t threads);

    // Check if conversion is supported
    bool is_supported(Format from, Format to) const;

    // Get list of supported output formats for a given input format
    std::vector<Format> get_supported_outputs(Format input) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace converter
