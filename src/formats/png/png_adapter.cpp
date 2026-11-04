#include "converter/png_adapter.hpp"
#include "converter/resource_limits.hpp"
#include "converter/error.hpp"
#include <png.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace converter {

namespace {

// RAII wrapper for FILE*
struct FileCloser {
    void operator()(FILE* fp) const { if (fp) fclose(fp); }
};
using FilePtr = std::unique_ptr<FILE, FileCloser>;

// RAII wrapper for PNG read structures
struct PngReadDeleter {
    png_infop* info_ptr;
    void operator()(png_structp png) const {
        if (png) {
            png_destroy_read_struct(&png, info_ptr, nullptr);
        }
    }
};

// RAII wrapper for PNG write structures
struct PngWriteDeleter {
    png_infop* info_ptr;
    void operator()(png_structp png) const {
        if (png) {
            png_destroy_write_struct(&png, info_ptr);
        }
    }
};

// Error handler for libpng
void png_error_handler(png_structp, const char* error_msg) {
    throw ConversionError(std::string("PNG error: ") + error_msg);
}

// Warning handler (we'll ignore warnings)
void png_warning_handler(png_structp, const char*) {
    // Ignore warnings
}

} // anonymous namespace

ImageData png_decode(const std::string& path) {
    // Open file
    FilePtr fp(fopen(path.c_str(), "rb"));
    if (!fp) {
        throw ConversionError("Cannot open PNG file: " + path);
    }

    // Verify PNG signature
    uint8_t sig[8];
    if (fread(sig, 1, 8, fp.get()) != 8) {
        throw ConversionError("Cannot read PNG signature");
    }
    if (png_sig_cmp(sig, 0, 8) != 0) {
        throw ConversionError("Not a valid PNG file");
    }

    // Create read struct
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                              png_error_handler, png_warning_handler);
    if (!png) {
        throw ConversionError("Failed to create PNG read struct");
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        throw ConversionError("Failed to create PNG info struct");
    }

    // Set up RAII cleanup
    std::unique_ptr<png_struct, PngReadDeleter> png_guard(png, PngReadDeleter{&info});

    // Initialize IO
    png_init_io(png, fp.get());
    png_set_sig_bytes(png, 8);

    // Read image info
    png_read_info(png, info);

    uint32_t width = png_get_image_width(png, info);
    uint32_t height = png_get_image_height(png, info);
    uint8_t color_type = png_get_color_type(png, info);
    uint8_t bit_depth = png_get_bit_depth(png, info);

    // Validate dimensions
    validate_image_resources(width, height, 4);  // Worst case: RGBA

    // Transform to RGBA format
    if (bit_depth == 16) {
        png_set_strip_16(png);  // Strip 16-bit to 8-bit
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }

    // Update info after transformations
    png_read_update_info(png, info);

    // Allocate image buffer
    ImageData image;
    image.width = width;
    image.height = height;
    image.channels = 4;  // RGBA
    image.pixels.resize(image.expected_size());

    // Allocate row pointers
    std::vector<png_bytep> row_pointers(height);
    for (uint32_t y = 0; y < height; ++y) {
        row_pointers[y] = image.pixels.data() + y * width * 4;
    }

    // Read image data
    png_read_image(png, row_pointers.data());
    png_read_end(png, nullptr);

    return image;
}

void png_encode(const ImageData& image, const std::string& path, const PngOptions& options) {
    if (!image.is_valid()) {
        throw ConversionError("Invalid image data");
    }

    // Open output file
    FilePtr fp(fopen(path.c_str(), "wb"));
    if (!fp) {
        throw ConversionError("Cannot create PNG file: " + path);
    }

    // Create write struct
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                               png_error_handler, png_warning_handler);
    if (!png) {
        throw ConversionError("Failed to create PNG write struct");
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        throw ConversionError("Failed to create PNG info struct");
    }

    // Set up RAII cleanup
    std::unique_ptr<png_struct, PngWriteDeleter> png_guard(png, PngWriteDeleter{&info});

    // Initialize IO
    png_init_io(png, fp.get());

    // Set compression level
    png_set_compression_level(png, options.compression_level);

    // Set image properties
    int color_type = (image.channels == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
    png_set_IHDR(png, info, image.width, image.height, 8, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, info);

    // Prepare row pointers
    std::vector<png_const_bytep> row_pointers(image.height);
    for (uint32_t y = 0; y < image.height; ++y) {
        row_pointers[y] = image.pixels.data() + y * image.width * image.channels;
    }

    // Write image data
    png_write_image(png, const_cast<png_bytepp>(row_pointers.data()));
    png_write_end(png, nullptr);
}

ImageInfo png_get_info(const std::string& path) {
    // Open file
    FilePtr fp(fopen(path.c_str(), "rb"));
    if (!fp) {
        throw ConversionError("Cannot open PNG file: " + path);
    }

    // Verify PNG signature
    uint8_t sig[8];
    if (fread(sig, 1, 8, fp.get()) != 8) {
        throw ConversionError("Cannot read PNG signature");
    }
    if (png_sig_cmp(sig, 0, 8) != 0) {
        throw ConversionError("Not a valid PNG file");
    }

    // Create read struct
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                              png_error_handler, png_warning_handler);
    if (!png) {
        throw ConversionError("Failed to create PNG read struct");
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        throw ConversionError("Failed to create PNG info struct");
    }

    std::unique_ptr<png_struct, PngReadDeleter> png_guard(png, PngReadDeleter{&info});

    png_init_io(png, fp.get());
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    ImageInfo result;
    result.width = png_get_image_width(png, info);
    result.height = png_get_image_height(png, info);

    uint8_t color_type = png_get_color_type(png, info);
    result.has_transparency = (color_type & PNG_COLOR_MASK_ALPHA) ||
                              png_get_valid(png, info, PNG_INFO_tRNS);

    // Determine channels after standard transformations
    if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        result.channels = 4;
    } else {
        result.channels = 3;
    }

    return result;
}

} // namespace converter
