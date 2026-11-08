#include "converter/pdf_renderer.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/resource_limits.hpp"
#include "converter/error.hpp"

#include <mupdf/fitz.h>
#include <filesystem>
#include <cstring>
#include <memory>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cmath>
#include <limits>

namespace converter {

namespace {

// RAII wrapper for MuPDF context
struct MuContext {
    fz_context* ctx = nullptr;

    MuContext() {
        ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
        if (!ctx) {
            throw ConversionError("Failed to initialize MuPDF context");
        }
        fz_try(ctx) {
            fz_register_document_handlers(ctx);
        }
        fz_catch(ctx) {
            std::string msg = fz_caught_message(ctx);
            fz_drop_context(ctx);
            ctx = nullptr;
            throw ConversionError("Failed to register document handlers: " + msg);
        }
    }

    ~MuContext() {
        if (ctx) {
            fz_drop_context(ctx);
        }
    }

    MuContext(const MuContext&) = delete;
    MuContext& operator=(const MuContext&) = delete;
};

// RAII wrapper for MuPDF document
struct MuDocument {
    fz_context* ctx = nullptr;
    fz_document* doc = nullptr;

    MuDocument(fz_context* c, const std::string& path) : ctx(c) {
        if (!std::filesystem::exists(path)) {
            throw IOError("PDF file not found: " + path);
        }
        fz_try(ctx) {
            doc = fz_open_document(ctx, path.c_str());
        }
        fz_catch(ctx) {
            std::string msg = fz_caught_message(ctx);
            throw FormatError("Failed to open PDF '" + path + "': " + msg);
        }
        if (!doc) {
            throw FormatError("Failed to open PDF: " + path);
        }
    }

    ~MuDocument() {
        if (doc && ctx) {
            fz_drop_document(ctx, doc);
        }
    }

    int page_count() const {
        int count = 0;
        fz_try(ctx) {
            count = fz_count_pages(ctx, doc);
        }
        fz_catch(ctx) {
            std::string msg = fz_caught_message(ctx);
            throw FormatError("Failed to count PDF pages: " + msg);
        }
        return count;
    }

    MuDocument(const MuDocument&) = delete;
    MuDocument& operator=(const MuDocument&) = delete;
};

void validate_page_render_resources(fz_context* ctx, fz_page* page, int dpi) {
    fz_rect bounds{};
    std::string error_msg;
    bool failed = false;

    fz_try(ctx) {
        bounds = fz_bound_page(ctx, page);
    }
    fz_catch(ctx) {
        error_msg = fz_caught_message(ctx);
        failed = true;
    }

    if (failed) {
        throw FormatError("Failed to determine PDF page bounds: " + error_msg);
    }

    const double scale = static_cast<double>(dpi) / 72.0;
    const double width_pixels = std::ceil(static_cast<double>(bounds.x1 - bounds.x0) * scale);
    const double height_pixels = std::ceil(static_cast<double>(bounds.y1 - bounds.y0) * scale);
    if (!std::isfinite(width_pixels) || !std::isfinite(height_pixels) ||
        width_pixels <= 0.0 || height_pixels <= 0.0 ||
        width_pixels > static_cast<double>(std::numeric_limits<uint32_t>::max()) ||
        height_pixels > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
        throw ResourceLimitError("PDF page dimensions are invalid or exceed supported limits");
    }

    validate_image_resources(
        static_cast<uint32_t>(width_pixels),
        static_cast<uint32_t>(height_pixels),
        3
    );
}

// Internal function to render a single page (0-indexed) to ImageData
ImageData render_single_page_internal(fz_context* ctx, fz_document* doc, int page_idx, int dpi) {
    validate_dpi(dpi);

    fz_page* page = nullptr;
    fz_pixmap* pix = nullptr;
    std::string error_msg;
    bool failed = false;

    fz_try(ctx) {
        page = fz_load_page(ctx, doc, page_idx);
    }
    fz_catch(ctx) {
        error_msg = fz_caught_message(ctx);
        failed = true;
    }

    if (failed || !page) {
        throw FormatError("Failed to load PDF page " + std::to_string(page_idx + 1) + ": " + error_msg);
    }

    try {
        validate_page_render_resources(ctx, page, dpi);
    } catch (...) {
        fz_drop_page(ctx, page);
        throw;
    }

    float scale = static_cast<float>(dpi) / 72.0f;
    fz_matrix ctm = fz_scale(scale, scale);

    failed = false;
    fz_try(ctx) {
        pix = fz_new_pixmap_from_page(ctx, page, ctm, fz_device_rgb(ctx), 0);
    }
    fz_catch(ctx) {
        error_msg = fz_caught_message(ctx);
        failed = true;
    }

    if (page) {
        fz_drop_page(ctx, page);
        page = nullptr;
    }

    if (failed || !pix) {
        if (pix) {
            fz_drop_pixmap(ctx, pix);
        }
        throw FormatError("Failed to render PDF page " + std::to_string(page_idx + 1) + ": " + error_msg);
    }

    int w = fz_pixmap_width(ctx, pix);
    int h = fz_pixmap_height(ctx, pix);
    int n = fz_pixmap_components(ctx, pix);
    int stride = fz_pixmap_stride(ctx, pix);
    const unsigned char* samples = fz_pixmap_samples(ctx, pix);

    if (w <= 0 || h <= 0 || (n != 3 && n != 4)) {
        fz_drop_pixmap(ctx, pix);
        throw ConversionError("Invalid rendered pixmap dimensions or channels");
    }

    // Validate resource limits before memory allocation
    validate_image_resources(static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint8_t>(n));

    ImageData image;
    image.width = static_cast<uint32_t>(w);
    image.height = static_cast<uint32_t>(h);
    image.channels = static_cast<uint8_t>(n);

    try {
        image.pixels.resize(image.expected_size());
    } catch (...) {
        fz_drop_pixmap(ctx, pix);
        throw;
    }

    size_t row_bytes = static_cast<size_t>(w) * static_cast<size_t>(n);
    for (int y = 0; y < h; ++y) {
        std::memcpy(&image.pixels[static_cast<size_t>(y) * row_bytes],
                    samples + y * stride,
                    row_bytes);
    }

    fz_drop_pixmap(ctx, pix);
    return image;
}

// Resolve requested pages according to PDFReadOptions (returns 1-indexed page numbers)
std::vector<int> resolve_page_numbers(int total_pages, const PDFReadOptions& options) {
    std::vector<int> pages;

    if (options.page_number.has_value()) {
        int p = *options.page_number;
        if (p < 1 || p > total_pages) {
            throw ValidationError("Requested page number " + std::to_string(p) +
                                  " is out of range [1, " + std::to_string(total_pages) + "]");
        }
        pages.push_back(p);
    } else if (options.page_start.has_value() || options.page_end.has_value()) {
        int start = options.page_start.value_or(1);
        int end = options.page_end.value_or(total_pages);

        if (start < 1 || start > total_pages) {
            throw ValidationError("Page range start " + std::to_string(start) +
                                  " is out of range [1, " + std::to_string(total_pages) + "]");
        }
        if (end < start || end > total_pages) {
            throw ValidationError("Page range end " + std::to_string(end) +
                                  " is invalid (must be between start=" + std::to_string(start) +
                                  " and " + std::to_string(total_pages) + ")");
        }

        for (int p = start; p <= end; ++p) {
            pages.push_back(p);
        }
    } else {
        // All pages
        for (int p = 1; p <= total_pages; ++p) {
            pages.push_back(p);
        }
    }

    return pages;
}

// Generate the output filename for a given rendered page
std::string make_page_filename(
    const std::string& output_spec,
    const std::string& input_pdf_path,
    int page_number,
    size_t total_pages_to_render,
    Format format
) {
    std::string ext = (format == Format::JPEG) ? ".jpg" : ".png";
    std::filesystem::path out_p(output_spec);

    // If output_spec is a directory or ends with a slash:
    if (std::filesystem::is_directory(out_p) || output_spec.back() == '/' || output_spec.back() == '\\') {
        std::string stem = std::filesystem::path(input_pdf_path).stem().string();
        char buf[32];
        snprintf(buf, sizeof(buf), "-%02d", page_number);
        return (out_p / (stem + buf + ext)).string();
    }

    // Single-page output to an explicit filename:
    if (total_pages_to_render == 1 && out_p.has_extension()) {
        return output_spec;
    }

    // Multi-page output into a filename pattern:
    std::filesystem::path parent = out_p.parent_path();
    std::string stem = out_p.stem().string();
    std::string target_ext = out_p.has_extension() ? out_p.extension().string() : ext;

    char buf[32];
    snprintf(buf, sizeof(buf), "-%02d", page_number);
    std::string new_filename = stem + buf + target_ext;

    if (parent.empty()) {
        return new_filename;
    }
    return (parent / new_filename).string();
}

} // anonymous namespace

int pdf_get_page_count(const std::string& path) {
    MuContext mu_ctx;
    MuDocument doc(mu_ctx.ctx, path);
    int count = doc.page_count();
    validate_pdf_page_count(static_cast<size_t>(count));
    return count;
}

PdfDocumentInfo pdf_get_info(const std::string& path) {
    MuContext mu_ctx;
    MuDocument doc(mu_ctx.ctx, path);

    int total_pages = doc.page_count();
    validate_pdf_page_count(static_cast<size_t>(total_pages));

    PdfDocumentInfo info;
    info.page_count = total_pages;
    info.pages.reserve(static_cast<size_t>(total_pages));

    for (int i = 0; i < total_pages; ++i) {
        fz_page* page = nullptr;
        fz_rect rect{};
        std::string err;
        bool failed = false;

        fz_try(mu_ctx.ctx) {
            page = fz_load_page(mu_ctx.ctx, doc.doc, i);
            rect = fz_bound_page(mu_ctx.ctx, page);
        }
        fz_catch(mu_ctx.ctx) {
            err = fz_caught_message(mu_ctx.ctx);
            failed = true;
        }

        if (page) {
            fz_drop_page(mu_ctx.ctx, page);
        }

        if (failed) {
            throw FormatError("Failed to get bounds for page " + std::to_string(i + 1) + ": " + err);
        }

        PdfPageInfo page_info;
        page_info.page_number = i + 1;
        page_info.width_pt = static_cast<double>(rect.x1 - rect.x0);
        page_info.height_pt = static_cast<double>(rect.y1 - rect.y0);
        info.pages.push_back(page_info);
    }

    return info;
}

ImageData pdf_render_page(const std::string& path, int page_number, int dpi) {
    MuContext mu_ctx;
    MuDocument doc(mu_ctx.ctx, path);

    int total_pages = doc.page_count();
    validate_pdf_page_count(static_cast<size_t>(total_pages));

    if (page_number < 1 || page_number > total_pages) {
        throw ValidationError("Page number " + std::to_string(page_number) +
                              " is out of range [1, " + std::to_string(total_pages) + "]");
    }

    return render_single_page_internal(mu_ctx.ctx, doc.doc, page_number - 1, dpi);
}

std::vector<ImageData> pdf_render_pages(const std::string& path, const PDFReadOptions& options) {
    MuContext mu_ctx;
    MuDocument doc(mu_ctx.ctx, path);

    int total_pages = doc.page_count();
    validate_pdf_page_count(static_cast<size_t>(total_pages));

    std::vector<int> page_nums = resolve_page_numbers(total_pages, options);
    std::vector<ImageData> results;
    results.reserve(page_nums.size());

    for (int p : page_nums) {
        results.push_back(render_single_page_internal(mu_ctx.ctx, doc.doc, p - 1, options.dpi));
    }

    return results;
}

std::vector<std::string> pdf_to_images(
    const std::string& pdf_path,
    const std::string& output_path,
    const ConversionRequest& request
) {
    if (output_path.empty()) {
        throw ValidationError("Output path cannot be empty");
    }

    MuContext mu_ctx;
    MuDocument doc(mu_ctx.ctx, pdf_path);

    int total_pages = doc.page_count();
    validate_pdf_page_count(static_cast<size_t>(total_pages));

    if (total_pages == 0) {
        throw FormatError("PDF document has 0 pages");
    }

    std::vector<int> pages = resolve_page_numbers(total_pages, request.pdf_read_options);
    if (pages.empty()) {
        throw ValidationError("No pages selected for conversion");
    }

    // Ensure parent directory exists
    std::filesystem::path out_p(output_path);
    if (std::filesystem::is_directory(out_p) || output_path.back() == '/' || output_path.back() == '\\') {
        std::filesystem::create_directories(out_p);
    } else if (out_p.has_parent_path()) {
        std::filesystem::create_directories(out_p.parent_path());
    }

    std::vector<std::string> created_files;
    created_files.reserve(pages.size());

    for (size_t i = 0; i < pages.size(); ++i) {
        if (request.cancellation_token && request.cancellation_token->is_cancelled()) {
            throw CancelledException();
        }

        int page_num = pages[i];
        ImageData img = render_single_page_internal(mu_ctx.ctx, doc.doc, page_num - 1, request.pdf_read_options.dpi);

        std::string page_file = make_page_filename(output_path, pdf_path, page_num, pages.size(), request.output_format);

        if (request.output_format == Format::PNG) {
            PngOptions png_opts;
            png_opts.compression_level = request.png_options.compression_level;
            png_opts.preserve_transparency = true;
            png_encode(img, page_file, png_opts);
        } else if (request.output_format == Format::JPEG) {
            JpegOptions jpeg_opts;
            jpeg_opts.quality = request.jpeg_options.quality;
            uint32_t bg = request.jpeg_options.background_color;
            jpeg_opts.background_color = {
                static_cast<uint8_t>((bg >> 16) & 0xFF),
                static_cast<uint8_t>((bg >> 8) & 0xFF),
                static_cast<uint8_t>(bg & 0xFF)
            };
            jpeg_encode(img, page_file, jpeg_opts);
        } else {
            throw ConversionError("Unsupported output format for PDF rendering");
        }

        created_files.push_back(page_file);

        if (request.progress_callback) {
            int pct = static_cast<int>((static_cast<double>(i + 1) / static_cast<double>(pages.size())) * 100.0);
            request.progress_callback(pct, "Rendered page " + std::to_string(page_num) + " (" +
                                      std::to_string(i + 1) + "/" + std::to_string(pages.size()) + ")");
        }
    }

    return created_files;
}

} // namespace converter
