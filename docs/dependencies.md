# Dependency Decision Record

**Date**: 2026-11-11
**System**: Zorin OS 17 (Ubuntu 24.04 base), Linux kernel 7.0.0-38-generic  
**Hardware**: AMD Ryzen 5 7535HS, 12GB RAM, 12 cores

## Build Dependencies

### Core Build Tools
- **CMake 3.28.3**: Modern build system with excellent IDE integration
- **GCC 13.3.0**: Full C++20 support, battle-tested optimizer
- **pkg-config**: Dependency discovery for system libraries

### GUI Framework
- **GTK4 4.14.5**: Native Linux desktop toolkit, well-integrated with GNOME/Zorin
- **gtkmm-4.0 4.10.0**: Modern C++ bindings for GTK4
- **Rationale**: 
  - Native look and feel
  - Lower resource usage than Electron/Qt
  - Async operation support with GLib main loop
  - Excellent accessibility support
- **Footprint**: Runtime libraries already installed on target system

### Image Codecs

#### PNG Support
- **Library**: libpng 1.6.43
- **License**: zlib/libpng (permissive)
- **Rationale**:
  - Reference implementation, universal compatibility
  - Optimized DEFLATE compression
  - Complete PNG feature support (transparency, metadata)
  - Small footprint
- **Alternative considered**: stb_image (single-header)
  - **Rejected**: Less battle-tested, manual compression handling

#### JPEG Support
- **Library**: libjpeg-turbo 2.1.5
- **License**: BSD-style (permissive)
- **Rationale**:
  - SIMD-optimized (2-6× faster than libjpeg)
  - API-compatible with libjpeg
  - Industry standard
  - Already installed on most Linux systems
- **Alternative considered**: libjpeg (reference)
  - **Rejected**: Significantly slower, no advantage for our use case

### PDF Support

#### PDF Generation (Writing)
- **Implementation**: Custom minimal PDF writer module
- **Rationale**:
  - Full control over output structure
  - No unnecessary dependencies
  - Optimized for image-based PDFs only
  - Direct JPEG passthrough (DCTDecode)
  - Small code footprint (~500-1000 LOC estimated)
- **Scope**: PDF 1.4 subset sufficient for image embedding
- **Not implemented**: Text rendering, forms, annotations, encryption

#### PDF Rendering (Reading)
- **Library**: MuPDF 1.28.6 or later
- **License**: AGPL-3+
- **Rationale**:
  - Compact and fast
  - Excellent rendering quality
  - Lower memory usage than Poppler
  - Clean C API
- **Licensing impact**:
  - **Application must be AGPL-3+ licensed**
  - **Source distribution required**
  - Acceptable for open-source desktop application
  - Documented prominently in README and LICENSE
- **Alternative considered**: Poppler (via poppler-glib)
  - **Rejected**: Larger dependency tree, higher memory usage
  - **Note**: Could be swapped via adapter interface if licensing becomes an issue
- **Commercial licensing**: Artifex offers commercial MuPDF licenses if needed for future non-AGPL distribution

## Runtime Dependencies

### Required System Libraries
- GTK4 runtime (libgtk-4-1)
- gtkmm-4.0 runtime (libgtkmm-4.0-0)
- libpng16
- libjpeg-turbo8
- MuPDF library (libmupdf)
- Standard C++ runtime (libstdc++6)
- GLib/GObject runtime

### Expected Footprint
- **Binary size** (stripped, Release): Target <5 MB
- **Total installed size**: Target <15 MB including assets
- **Runtime RAM** (idle): Target <50 MB
- **Peak RAM** (large conversion): Depends on image size, ~4× pixel data + codec buffers

## Development Dependencies (Not Required for Runtime)
- CMake
- GCC/G++
- pkg-config
- Development headers (-dev packages) for all runtime libraries
- Git (for version control)

## Dependency Installation

### Ubuntu/Zorin OS
```bash
# Runtime dependencies (minimal)
sudo apt-get install libgtk-4-1 libgtkmm-4.0-0 libpng16-16 libjpeg-turbo8

# Build dependencies other than MuPDF
sudo apt-get install cmake g++ pkg-config \
  libgtkmm-4.0-dev libpng-dev libjpeg-turbo8-dev
```

Ubuntu 24.04/Zorin repositories currently provide MuPDF 1.23.10, below the
security baseline enforced by `CMakeLists.txt`. Build MuPDF 1.28.6 or later
from the official source with system JPEG support:

```bash
make -C mupdf-1.28.6-source -j$(nproc) libs \
  HAVE_GLUT=no HAVE_X11=no USE_SYSTEM_LIBJPEG=yes USE_SYSTEM_OPENJPEG=no
```

Configure FluxConvert with `MUPDF_INCLUDE_DIR`, `MUPDF_LIBRARY`, and
`MUPDF_THIRD_LIBRARY` pointing to that MuPDF build.

## Rejected Dependencies

### Office Document Support (DOCX, XLSX, PPTX)
- **Rejected**: LibreOffice, Apache POI, Office automation
- **Reason**: Explicitly out of scope for v1.0
- **Future consideration**: v1.1+ if requested, via separate plugin

### OCR Libraries
- **Rejected**: Tesseract
- **Reason**: Not required for PDF-to-image conversion (rendering only)

### Web Technologies
- **Rejected**: Electron, Node.js, CEF, WebView
- **Reason**: Massive resource footprint incompatible with design goals

### Heavy Frameworks
- **Rejected**: Qt (considered but rejected)
- **Reason**: Larger footprint than GTK4, additional dependency

### Image Libraries Rejected
- **ImageMagick/GraphicsMagick**: Too heavy, shell execution model risky
- **OpenCV**: Overkill for simple format conversion
- **stb_image**: Considered but prefer proven codecs for production

## Dependency Verification

All runtime dependencies other than the MuPDF security baseline are:
✓ Available in Ubuntu 24.04 repositories  
✓ Actively maintained  
✓ Security-patched by distribution  
✓ License-compatible with AGPL-3+  
✓ Well-documented APIs  
✓ Suitable for offline operation  

## Testing Dependencies

- **Test framework**: Consider Catch2 or Google Test (to be determined in Stage 2)
- **Benchmark tools**: Custom benchmark harness with timing measurements
- **Sanitizers**: AddressSanitizer, UBSan (compiler-provided)

## Version Pinning Strategy

- **System packages**: Use distribution-provided versions (security updates)
- **No vendored dependencies**: Rely on system package manager
- **Alternative**: For AppImage distribution, bundle specific versions
- **Documentation**: Document tested versions in README

## Future Extension Considerations

### WebP Support (v1.1 potential)
- **Library**: libwebp
- **License**: BSD-style (compatible)
- **Effort**: ~1-2 days implementation + testing

### TIFF Support (v1.1 potential)
- **Library**: libtiff
- **License**: BSD-style (compatible)
- **Effort**: ~1 day implementation + testing

### HEIF/AVIF Support
- **Library**: libheif
- **License**: LGPL (compatible)
- **Effort**: ~2-3 days (more complex format)
