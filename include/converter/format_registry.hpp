#pragma once

#include "format_handler.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace converter {

// Registry of format handlers
class FormatRegistry {
public:
    FormatRegistry() = default;
    ~FormatRegistry() = default;

    // Non-copyable, movable
    FormatRegistry(const FormatRegistry&) = delete;
    FormatRegistry& operator=(const FormatRegistry&) = delete;
    FormatRegistry(FormatRegistry&&) = default;
    FormatRegistry& operator=(FormatRegistry&&) = default;

    // Get singleton instance
    static FormatRegistry& instance();

    // Register a format handler (replaces any existing handler for the same format)
    void register_handler(std::unique_ptr<FormatHandler> handler);

    // Get handler for a specific format (O(1) lookup)
    FormatHandler* get_handler(Format format);

    // Find handler that can perform conversion
    FormatHandler* find_handler_for_conversion(Format from, Format to);

    // Get capabilities for a format
    FormatCapabilities get_capabilities(Format format);

    // Check if conversion is supported
    bool is_conversion_supported(Format from, Format to);

    // Clear all handlers (mainly for testing)
    void clear();

private:
    std::unordered_map<Format, std::unique_ptr<FormatHandler>> handlers_;
};

// Helper to register all built-in format handlers
void register_builtin_handlers(FormatRegistry& registry);
void register_builtin_handlers();

} // namespace converter
