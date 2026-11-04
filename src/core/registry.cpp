#include "converter/format_registry.hpp"
#include "converter/error.hpp"
#include <mutex>

namespace converter {

// Forward declarations for format handler registration functions
void register_png_handler(FormatRegistry& registry);
void register_jpeg_handler(FormatRegistry& registry);
void register_pdf_handler(FormatRegistry& registry);
void register_png_handler();
void register_jpeg_handler();
void register_pdf_handler();

FormatRegistry& FormatRegistry::instance() {
    static FormatRegistry registry;
    static std::once_flag init_flag;
    std::call_once(init_flag, [&]() {
        register_builtin_handlers(registry);
    });
    return registry;
}

void FormatRegistry::register_handler(std::unique_ptr<FormatHandler> handler) {
    if (handler) {
        Format fmt = handler->get_format();
        handlers_[fmt] = std::move(handler);
    }
}

FormatHandler* FormatRegistry::get_handler(Format format) {
    auto it = handlers_.find(format);
    return (it != handlers_.end()) ? it->second.get() : nullptr;
}

FormatHandler* FormatRegistry::find_handler_for_conversion(Format from, Format to) {
    // Fast path: check handler for the source format
    auto it = handlers_.find(from);
    if (it != handlers_.end() && it->second && it->second->can_convert(from, to)) {
        return it->second.get();
    }

    // Secondary scan across any other registered handler capable of conversion
    for (const auto& [fmt, handler] : handlers_) {
        if (handler && handler->can_convert(from, to)) {
            return handler.get();
        }
    }
    return nullptr;
}

FormatCapabilities FormatRegistry::get_capabilities(Format format) {
    auto* handler = get_handler(format);
    if (handler) {
        return handler->get_capabilities();
    }
    return FormatCapabilities{};
}

bool FormatRegistry::is_conversion_supported(Format from, Format to) {
    return find_handler_for_conversion(from, to) != nullptr;
}

void FormatRegistry::clear() {
    handlers_.clear();
}

void register_builtin_handlers(FormatRegistry& registry) {
    register_png_handler(registry);
    register_jpeg_handler(registry);
    register_pdf_handler(registry);
}

void register_builtin_handlers() {
    register_builtin_handlers(FormatRegistry::instance());
}

} // namespace converter
