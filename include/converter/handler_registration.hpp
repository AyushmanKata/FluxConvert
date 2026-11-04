#pragma once

namespace converter {

class FormatRegistry;

// Forward declarations for manual registration
class PngHandler;
class JpegHandler;
class PdfHandler;

// Registration functions (defined in handler implementation files)
void register_png_handler(FormatRegistry& registry);
void register_png_handler();
void register_jpeg_handler(FormatRegistry& registry);
void register_jpeg_handler();
void register_pdf_handler(FormatRegistry& registry);
void register_pdf_handler();

} // namespace converter
