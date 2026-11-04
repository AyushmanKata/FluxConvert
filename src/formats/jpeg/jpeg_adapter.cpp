#include "converter/jpeg_adapter.hpp"
#include "converter/resource_limits.hpp"
#include "converter/error.hpp"
#include <jpeglib.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <setjmp.h>

namespace converter {

namespace {

// RAII wrapper for FILE*
struct FileCloser {
    void operator()(FILE* fp) const { if (fp) fclose(fp); }
};
using FilePtr = std::unique_ptr<FILE, FileCloser>;

// Custom error manager for libjpeg
struct JpegErrorMgr {
    jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

// Error handler
void jpeg_error_exit(j_common_ptr cinfo) {
    JpegErrorMgr* err = reinterpret_cast<JpegErrorMgr*>(cinfo->err);
    char buffer[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, buffer);
    longjmp(err->setjmp_buffer, 1);
}

} // anonymous namespace

ImageData jpeg_decode(const std::string& path) {
    // Open file
    FilePtr fp(fopen(path.c_str(), "rb"));
    if (!fp) {
        throw ConversionError("Cannot open JPEG file: " + path);
    }

    // Set up decompression
    jpeg_decompress_struct cinfo;
    JpegErrorMgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        throw ConversionError("JPEG decoding error");
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp.get());
    jpeg_read_header(&cinfo, TRUE);

    // Validate dimensions
    validate_image_resources(cinfo.image_width, cinfo.image_height, 3);

    // Start decompression
    jpeg_start_decompress(&cinfo);

    // Allocate image buffer (RGB only - JPEG doesn't support alpha)
    ImageData image;
    image.width = cinfo.output_width;
    image.height = cinfo.output_height;
    image.channels = 3;  // RGB
    image.pixels.resize(image.expected_size());

    // Read scanlines
    uint32_t row_stride = image.width * image.channels;
    JSAMPROW row_pointer[1];

    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = &image.pixels[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return image;
}

void jpeg_encode(const ImageData& image, const std::string& path, const JpegOptions& options) {
    if (!image.is_valid()) {
        throw ConversionError("Invalid image data");
    }

    // Validate quality parameter
    if (options.quality < 1 || options.quality > 100) {
        throw ConversionError("JPEG quality must be between 1 and 100");
    }

    // Open output file
    FilePtr fp(fopen(path.c_str(), "wb"));
    if (!fp) {
        throw ConversionError("Cannot create JPEG file: " + path);
    }

    // Set up compression
    jpeg_compress_struct cinfo;
    JpegErrorMgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_compress(&cinfo);
        throw ConversionError("JPEG encoding error");
    }

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp.get());

    // Prepare image data (handle RGBA → RGB conversion if needed)
    ImageData rgb_image;
    const ImageData* source_image = &image;

    if (image.channels == 4) {
        // Convert RGBA to RGB with alpha blending
        rgb_image = convert_rgba_to_rgb(image, options.background_color);
        source_image = &rgb_image;
    } else if (image.channels != 3) {
        throw ConversionError("JPEG encoding requires RGB or RGBA image data");
    }

    // Set compression parameters
    cinfo.image_width = source_image->width;
    cinfo.image_height = source_image->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, options.quality, TRUE);

    // Start compression
    jpeg_start_compress(&cinfo, TRUE);

    // Write scanlines
    uint32_t row_stride = source_image->width * 3;
    JSAMPROW row_pointer[1];

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = const_cast<JSAMPROW>(&source_image->pixels[cinfo.next_scanline * row_stride]);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}

ImageInfo jpeg_get_info(const std::string& path) {
    // Open file
    FilePtr fp(fopen(path.c_str(), "rb"));
    if (!fp) {
        throw ConversionError("Cannot open JPEG file: " + path);
    }

    // Set up decompression
    jpeg_decompress_struct cinfo;
    JpegErrorMgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        throw ConversionError("JPEG header reading error");
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp.get());
    jpeg_read_header(&cinfo, TRUE);

    ImageInfo result;
    result.width = cinfo.image_width;
    result.height = cinfo.image_height;
    result.channels = 3;  // JPEG is always RGB
    result.has_transparency = false;  // JPEG doesn't support alpha

    jpeg_destroy_decompress(&cinfo);

    return result;
}

} // namespace converter
