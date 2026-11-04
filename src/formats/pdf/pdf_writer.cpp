#include "converter/pdf_writer.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/format.hpp"
#include "converter/error.hpp"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <locale>
#include <zlib.h>

namespace converter {

namespace {

template <typename Stream>
Stream use_pdf_locale(Stream stream) {
    stream.imbue(std::locale::classic());
    return stream;
}

// Compress data using zlib (Flate)
std::vector<uint8_t> flate_compress(const uint8_t* data, size_t size) {
    std::vector<uint8_t> compressed;

    z_stream stream = {};
    stream.next_in = const_cast<uint8_t*>(data);
    stream.avail_in = static_cast<uInt>(size);

    if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
        throw ConversionError("Failed to initialize zlib compression");
    }

    compressed.resize(deflateBound(&stream, size));
    stream.next_out = compressed.data();
    stream.avail_out = static_cast<uInt>(compressed.size());

    int ret = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        throw ConversionError("Failed to compress data");
    }

    compressed.resize(stream.total_out);
    return compressed;
}

// Get current timestamp for PDF metadata
std::string pdf_timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::gmtime(&now);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "D:%Y%m%d%H%M%SZ", tm);
    return buffer;
}

} // anonymous namespace

// Calculate image placement on page
ImagePlacement calculate_placement(
    uint32_t image_width,
    uint32_t image_height,
    const PageDimensions& page,
    double margin_pt,
    [[maybe_unused]] PDFWriteOptions::Orientation orientation
) {
    ImagePlacement placement;

    // Available space after margins
    double available_width = page.width_pt - 2 * margin_pt;
    double available_height = page.height_pt - 2 * margin_pt;

    // Calculate scaling to fit within available space
    double scale_x = available_width / image_width;
    double scale_y = available_height / image_height;
    double scale = std::min(scale_x, scale_y);

    // Scaled dimensions
    placement.width = image_width * scale;
    placement.height = image_height * scale;

    // Center on page
    placement.x = margin_pt + (available_width - placement.width) / 2.0;
    placement.y = margin_pt + (available_height - placement.height) / 2.0;

    return placement;
}

// PDF Writer implementation

PdfWriter::PdfWriter() {
    // Reserve object IDs for catalog and pages
    next_object_id_ = 1;
}

PdfWriter::~PdfWriter() = default;

uint32_t PdfWriter::allocate_object_id() {
    return next_object_id_++;
}

void PdfWriter::add_page(
    const ImageData& image,
    const PageDimensions& page_size,
    double margin_pt,
    PDFWriteOptions::Orientation orientation
) {
    if (!image.is_valid()) {
        throw ConversionError("Invalid image data for PDF page");
    }

    // Create image object
    ImageObject img_obj;
    img_obj.obj_id = allocate_object_id();
    img_obj.width = image.width;
    img_obj.height = image.height;

    // Convert RGBA to RGB if needed (PDF doesn't support alpha in images easily)
    if (image.channels == 4) {
        ImageData rgb_image = convert_rgba_to_rgb(image);
        img_obj.data = flate_compress(rgb_image.pixels.data(), rgb_image.pixels.size());
        img_obj.color_components = 3;
        img_obj.is_jpeg = false;
    } else if (image.channels == 3) {
        // RGB - compress with flate
        img_obj.data = flate_compress(image.pixels.data(), image.pixels.size());
        img_obj.color_components = 3;
        img_obj.is_jpeg = false;
    } else {
        throw ConversionError("Unsupported image channel count for PDF");
    }

    uint32_t image_idx = static_cast<uint32_t>(images_.size());
    images_.push_back(std::move(img_obj));

    // Create page
    Page page;
    page.obj_id = allocate_object_id();
    page.dimensions = page_size;
    page.placement = calculate_placement(image.width, image.height, page_size, margin_pt, orientation);
    page.image_ref = image_idx;

    pages_.push_back(page);
}

void PdfWriter::add_jpeg_page(
    const std::string& jpeg_path,
    uint32_t width,
    uint32_t height,
    const PageDimensions& page_size,
    double margin_pt,
    PDFWriteOptions::Orientation orientation
) {
    // Read JPEG file directly
    std::ifstream file(jpeg_path, std::ios::binary);
    if (!file) {
        throw ConversionError("Cannot open JPEG file: " + jpeg_path);
    }

    file.seekg(0, std::ios::end);
    auto end_pos = file.tellg();
    if (end_pos < 0) {
        throw ConversionError("Cannot determine JPEG file size: " + jpeg_path);
    }
    size_t file_size = static_cast<size_t>(end_pos);
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> jpeg_data(file_size);
    file.read(reinterpret_cast<char*>(jpeg_data.data()), static_cast<std::streamsize>(file_size));

    if (!file) {
        throw ConversionError("Failed to read JPEG file: " + jpeg_path);
    }

    // Create image object with raw JPEG data
    ImageObject img_obj;
    img_obj.obj_id = allocate_object_id();
    img_obj.width = width;
    img_obj.height = height;
    img_obj.color_components = 3; // JPEG is typically RGB
    img_obj.data = std::move(jpeg_data);
    img_obj.is_jpeg = true;

    uint32_t image_idx = static_cast<uint32_t>(images_.size());
    images_.push_back(std::move(img_obj));

    // Create page
    Page page;
    page.obj_id = allocate_object_id();
    page.dimensions = page_size;
    page.placement = calculate_placement(width, height, page_size, margin_pt, orientation);
    page.image_ref = image_idx;

    pages_.push_back(page);
}

void PdfWriter::write_to_file(const std::string& output_path) {
    if (pages_.empty()) {
        throw ConversionError("Cannot create PDF with no pages");
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        throw ConversionError("Cannot create PDF file: " + output_path);
    }
    out.imbue(std::locale::classic());

    std::vector<size_t> xref_offsets;
    xref_offsets.push_back(0); // Object 0 is always null

    // PDF header
    out << "%PDF-1.4\n";
    out << "%\xE2\xE3\xCF\xD3\n"; // Binary marker

    // Helper to write object and track offset
    auto write_object = [&](uint32_t obj_id, const std::string& content) {
        while (xref_offsets.size() <= obj_id) {
            xref_offsets.push_back(0);
        }
        xref_offsets[obj_id] = static_cast<size_t>(out.tellp());
        out << obj_id << " 0 obj\n" << content << "\nendobj\n";
    };

    // Catalog and Pages IDs
    uint32_t catalog_id = allocate_object_id();
    uint32_t pages_obj_id = allocate_object_id();

    // Catalog
    {
        auto oss = use_pdf_locale(std::ostringstream{});
        oss << "<<\n"
            << "  /Type /Catalog\n"
            << "  /Pages " << pages_obj_id << " 0 R\n"
            << ">>";
        write_object(catalog_id, oss.str());
    }

    // Pages object
    {
        auto oss = use_pdf_locale(std::ostringstream{});
        oss << "<<\n"
            << "  /Type /Pages\n"
            << "  /Kids [";
        for (const auto& page : pages_) {
            oss << " " << page.obj_id << " 0 R";
        }
        oss << " ]\n"
            << "  /Count " << pages_.size() << "\n"
            << ">>";
        write_object(pages_obj_id, oss.str());
    }

    // Image XObjects
    for (const auto& img : images_) {
        auto oss = use_pdf_locale(std::ostringstream{});
        oss << "<<\n"
            << "  /Type /XObject\n"
            << "  /Subtype /Image\n"
            << "  /Width " << img.width << "\n"
            << "  /Height " << img.height << "\n"
            << "  /ColorSpace ";

        if (img.color_components == 3) {
            oss << "/DeviceRGB\n";
        } else if (img.color_components == 1) {
            oss << "/DeviceGray\n";
        } else {
            oss << "/DeviceRGB\n"; // Default
        }

        oss << "  /BitsPerComponent 8\n"
            << "  /Filter " << (img.is_jpeg ? "/DCTDecode" : "/FlateDecode") << "\n"
            << "  /Length " << img.data.size() << "\n"
            << ">>\n"
            << "stream\n";

        while (xref_offsets.size() <= img.obj_id) {
            xref_offsets.push_back(0);
        }
        xref_offsets[img.obj_id] = static_cast<size_t>(out.tellp());
        out << img.obj_id << " 0 obj\n"
            << oss.str();
        out.write(reinterpret_cast<const char*>(img.data.data()), static_cast<std::streamsize>(img.data.size()));
        out << "\nendstream\nendobj\n";
    }

    // Page objects with content streams
    for (const auto& page : pages_) {
        const auto& img = images_[page.image_ref];

        // Content stream
        uint32_t content_id = allocate_object_id();
        auto content = use_pdf_locale(std::ostringstream{});
        content << "q\n"  // Save graphics state
                << page.placement.width << " 0 0 "
                << page.placement.height << " "
                << page.placement.x << " "
                << page.placement.y << " cm\n"  // Transform matrix
                << "/Im" << page.image_ref << " Do\n"  // Draw image
                << "Q\n";  // Restore graphics state

        std::string content_str = content.str();
        {
            auto oss = use_pdf_locale(std::ostringstream{});
            oss << "<<\n"
                << "  /Length " << content_str.size() << "\n"
                << ">>\n"
                << "stream\n"
                << content_str
                << "endstream";
            write_object(content_id, oss.str());
        }

        // Page object
        auto page_obj = use_pdf_locale(std::ostringstream{});
        page_obj << "<<\n"
                 << "  /Type /Page\n"
                 << "  /Parent " << pages_obj_id << " 0 R\n"
                 << "  /MediaBox [0 0 " << page.dimensions.width_pt << " " << page.dimensions.height_pt << "]\n"
                 << "  /Contents " << content_id << " 0 R\n"
                 << "  /Resources <<\n"
                 << "    /XObject <<\n"
                 << "      /Im" << page.image_ref << " " << img.obj_id << " 0 R\n"
                 << "    >>\n"
                 << "  >>\n"
                 << ">>";
        write_object(page.obj_id, page_obj.str());
    }

    // Info dictionary
    uint32_t info_id = allocate_object_id();
    {
        auto oss = use_pdf_locale(std::ostringstream{});
        oss << "<<\n"
            << "  /Producer (FluxConvert 1.0)\n"
            << "  /CreationDate (" << pdf_timestamp() << ")\n"
            << ">>";
        write_object(info_id, oss.str());
    }

    // Cross-reference table
    size_t xref_offset = static_cast<size_t>(out.tellp());
    out << "xref\n";
    out << "0 " << xref_offsets.size() << "\n";
    for (size_t i = 0; i < xref_offsets.size(); ++i) {
        char buf[32];
        if (i == 0) {
            snprintf(buf, sizeof(buf), "%010zu 65535 f \n", xref_offsets[i]);
        } else {
            snprintf(buf, sizeof(buf), "%010zu 00000 n \n", xref_offsets[i]);
        }
        out << buf;
    }

    // Trailer
    out << "trailer\n"
        << "<<\n"
        << "  /Size " << xref_offsets.size() << "\n"
        << "  /Root " << catalog_id << " 0 R\n"
        << "  /Info " << info_id << " 0 R\n"
        << ">>\n"
        << "startxref\n"
        << xref_offset << "\n"
        << "%%EOF\n";
}

// High-level API
void images_to_pdf(
    const std::vector<std::string>& image_paths,
    const std::string& output_path,
    const PDFWriteOptions& options
) {
    if (image_paths.empty()) {
        throw ConversionError("No input images provided");
    }

    PdfWriter writer;

    // Determine page size
    PageDimensions page_dims;
    switch (options.page_size) {
        case PDFWriteOptions::PageSize::A4:
            page_dims = (options.orientation == PDFWriteOptions::Orientation::Landscape)
                ? PageSizes::A4_LANDSCAPE : PageSizes::A4_PORTRAIT;
            break;
        case PDFWriteOptions::PageSize::Letter:
            page_dims = (options.orientation == PDFWriteOptions::Orientation::Landscape)
                ? PageSizes::LETTER_LANDSCAPE : PageSizes::LETTER_PORTRAIT;
            break;
        case PDFWriteOptions::PageSize::FitToImage:
            page_dims = {}; // Will be determined per image
            break;
    }

    double margin_pt = mm_to_pt(options.margin_mm);

    // Add each image as a page
    for (const auto& path : image_paths) {
        // Determine format using magic bytes first
        Format fmt = detect_format_from_file(path);
        ImageInfo info;
        bool is_jpeg = false;

        if (fmt == Format::JPEG) {
            info = jpeg_get_info(path);
            is_jpeg = true;
        } else if (fmt == Format::PNG) {
            info = png_get_info(path);
            is_jpeg = false;
        } else {
            // Fallback for paths without standard headers or special files
            try {
                info = jpeg_get_info(path);
                is_jpeg = true;
            } catch (...) {
                try {
                    info = png_get_info(path);
                    is_jpeg = false;
                } catch (...) {
                    throw ConversionError("Cannot read image: " + path);
                }
            }
        }

        // Determine page size for this image
        PageDimensions this_page = page_dims;
        if (options.page_size == PDFWriteOptions::PageSize::FitToImage) {
            // Fit page to image (assuming 72 DPI)
            this_page.width_pt = info.width + 2 * margin_pt;
            this_page.height_pt = info.height + 2 * margin_pt;
        }

        // Add page
        if (is_jpeg) {
            // Embed JPEG directly
            writer.add_jpeg_page(path, info.width, info.height, this_page, margin_pt, options.orientation);
        } else {
            // Decode PNG and add
            ImageData image = png_decode(path);
            writer.add_page(image, this_page, margin_pt, options.orientation);
        }
    }

    writer.write_to_file(output_path);
}

} // namespace converter
