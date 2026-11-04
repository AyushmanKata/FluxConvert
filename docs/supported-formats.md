# Supported Formats

**Version 1.0** - Image and PDF conversion support

## Input Formats

### PNG (Portable Network Graphics)
- **Extensions**: `.png`
- **Magic number**: `89 50 4E 47 0D 0A 1A 0A`
- **Features**:
  - Lossless compression
  - Full transparency support (alpha channel)
  - 8-bit and 16-bit color depths
  - RGB and RGBA color spaces
- **Limitations**:
  - 16-bit images converted to 8-bit
  - Color profiles may not be preserved

### JPEG (Joint Photographic Experts Group)
- **Extensions**: `.jpg`, `.jpeg` (treated identically)
- **Magic number**: `FF D8 FF`
- **Features**:
  - Lossy compression
  - EXIF metadata support
  - JFIF format
- **Limitations**:
  - No transparency support
  - Progressive JPEGs supported but may convert to baseline

### PDF (Portable Document Format)
- **Extension**: `.pdf`
- **Magic number**: `%PDF-`
- **Features**:
  - Multi-page document support
  - Page selection for conversion
  - Rendering to image formats
- **Limitations**:
  - Encrypted PDFs not supported
  - Complex vector graphics rendered as raster
  - Text extraction not implemented (image output only)
  - OCR not performed

## Output Formats

### PNG Output
- **Compression**: Configurable (0-9, default: 6)
- **Color space**: RGB or RGBA
- **Bit depth**: 8-bit
- **Use cases**: Screenshots, graphics with transparency, lossless archival

### JPEG Output
- **Quality**: Configurable (1-100, default: 90)
- **Color space**: RGB (transparency converted with background color)
- **Subsampling**: 4:2:0 (standard)
- **Background color**: Configurable (default: white) for transparent images
- **Use cases**: Photographs, file size reduction

### PDF Output
- **Version**: PDF 1.4
- **Compression**: JPEG passthrough (DCTDecode) + PNG compression (FlateDecode)
- **Features**:
  - Single image per page
  - Multiple images → multipage document
  - Configurable page sizes (fit to image, A4, Letter)
  - Configurable margins
  - Auto or manual orientation
- **Limitations**: Image-based only (no text, no forms, no annotations)

## Conversion Matrix

| From ↓ To → | PNG | JPEG | PDF |
|-------------|-----|------|-----|
| **PNG**     | —   | ✓    | ✓   |
| **JPEG**    | ✓   | —    | ✓   |
| **PDF**     | ✓   | ✓    | —   |

### Special Conversions

#### PNG → JPEG
- Transparency replaced with background color (configurable)
- Quality setting applies
- Generally reduces file size

#### JPEG → PNG
- Lossless re-encoding
- No transparency added
- Generally increases file size
- Use case: further lossless editing

#### Images → PDF
- **Single image**: One-page PDF
- **Multiple images**: One image per page, sequential
- Page sizing options preserve aspect ratio
- JPEG images embedded directly (no re-encoding)
- PNG images compressed with FlateDecode

#### PDF → Images
- Renders each page to an image
- Configurable DPI (72-600, default: 150)
- Page selection: all pages, single page, or page range
- Output filenames: automatic suffixes for multi-page (`output-01.png`, `output-02.png`, etc.)

## Format-Specific Options

### JPEG Options
```cpp
struct JPEGOptions {
    int quality = 90;              // 1-100
    uint32_t background_color = 0xFFFFFF;  // RGB for transparency
};
```

### PNG Options
```cpp
struct PNGOptions {
    int compression_level = 6;     // 0 (none) to 9 (max)
};
```

### PDF Write Options
```cpp
struct PDFWriteOptions {
    enum class PageSize { FitToImage, A4, Letter };
    enum class Orientation { Auto, Portrait, Landscape };
    
    PageSize page_size = PageSize::FitToImage;
    Orientation orientation = Orientation::Auto;
    double margin_mm = 0.0;
};
```

### PDF Read Options
```cpp
struct PDFReadOptions {
    int dpi = 150;                      // 72-600
    std::optional<int> page_number;     // Single page
    std::optional<int> page_start;      // Range start (1-indexed)
    std::optional<int> page_end;        // Range end (inclusive)
};
```

## Resource Limits

To prevent resource exhaustion:

- **Max image dimensions**: 32,768 × 32,768 pixels (configurable)
- **Max decoded size**: 256 megapixels per image (configurable)
- **Max PDF pages**: 1,000 pages (configurable)
- **Memory management**: Sequential processing, one file at a time

## Format Detection

Format detection uses **magic number** (file signature) checking rather than
relying solely on file extensions:

1. Check file signature
2. Fall back to extension if signature not recognized
3. Reject files with mismatched signatures and extensions

This prevents accidental processing of incorrectly-named files.

## Future Format Support

Potential additions for v1.1+:

- **WebP**: Modern image format with good compression
- **TIFF**: Multi-page image format
- **HEIF/AVIF**: Next-generation image formats
- **SVG**: Vector graphics (rasterization only)

These would be added as new format handler modules without changing the core architecture.

## Not Supported (Out of Scope)

The following are **explicitly excluded** from this application:

- ✗ Office documents (DOCX, XLSX, PPTX)
- ✗ Video formats
- ✗ Audio formats
- ✗ OCR (Optical Character Recognition)
- ✗ PDF text extraction or editing
- ✗ RAW camera formats
- ✗ Animated images (GIF, APNG)
- ✗ 3D formats
- ✗ CAD formats
