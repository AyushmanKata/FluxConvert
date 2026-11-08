# FluxConvert

A fast, offline desktop file converter for Ubuntu-based Linux systems, with
Zorin OS as the primary target and minimal resource footprint.

**Version**: 1.0.0
**License**: AGPL-3.0-or-later (required by the MuPDF dependency)

## Features

✓ **Image Format Conversion**
- PNG ↔ JPEG with quality control
- PNG/JPEG → PDF (single or multi-page)
- PDF → PNG/JPEG with DPI control

✓ **Batch Processing**
- Convert multiple files in one operation
- Combine multiple images into one PDF

✓ **Desktop Integration**
- Native GTK4 interface for GNOME/Zorin OS
- Drag-and-drop file input
- Real-time progress reporting
- Keyboard accessible

✓ **Resource Efficient**
- Fast startup (<1s cold launch target)
- Low memory usage (<50MB idle target)
- Compact binary size (<5MB target)
- Fully offline operation

## Requirements

### Runtime Dependencies
- Ubuntu 24.04 LTS or later, including compatible Ubuntu-based distributions
  such as Zorin OS 17 or later. Zorin OS is the primary tested target.
- GTK4 (≥4.10)
- libpng (≥1.6)
- libjpeg-turbo (≥2.0)
- MuPDF (≥1.23)

The Debian package targets amd64 systems with compatible Ubuntu 24.04
libraries. Other Linux distributions may work when built from source with
equivalent dependencies, but are not officially tested.

### Build Dependencies
- CMake (≥3.20)
- GCC (≥11) with C++20 support
- pkg-config
- Development headers for runtime dependencies

## Installation

### From Source

```bash
# Install dependencies
sudo apt-get install cmake g++ pkg-config \
  libgtkmm-4.0-dev libpng-dev libjpeg-turbo8-dev libmupdf-dev

# Clone repository
git clone https://github.com/AyushmanKata/FluxConvert.git
cd fluxconvert

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Install (optional)
sudo cmake --install build
```

### Running Without Installation

```bash
./build-release/fluxconvert-gui
```

### Debian Package

The Release build can produce a Debian package for Ubuntu/Zorin OS:

```bash
cmake --preset release
cmake --build --preset release
cd build-release
cpack -G DEB
sudo apt install ./fluxconvert_1.0.0_*.deb
```

The package installs the GUI launcher, icon, AppStream metadata, CLI, and
license documentation.

## Usage

### GUI Mode

Launch the application:
```bash
fluxconvert-gui
```

1. Drag and drop files or click "Add Files"
2. Select output format
3. Configure options (quality, DPI, page size)
4. Click "Convert"
5. Open output folder when complete

### Command-Line Mode (for testing)

```bash
# Convert single file
fluxconvert-cli input.png output.jpg --quality 85

# Batch convert
fluxconvert-cli *.png --output-dir converted/ --format jpeg

# Multi-page PDF
fluxconvert-cli page1.jpg page2.jpg page3.jpg --output document.pdf
```

## Supported Conversions

| Input → Output | Single File | Batch | Multi-page PDF |
|----------------|-------------|-------|----------------|
| PNG → JPEG     | ✓           | ✓     | N/A            |
| JPEG → PNG     | ✓           | ✓     | N/A            |
| PNG → PDF      | ✓           | ✓     | ✓              |
| JPEG → PDF     | ✓           | ✓     | ✓              |
| PDF → PNG      | ✓ (pages)   | ✓     | N/A            |
| PDF → JPEG     | ✓ (pages)   | ✓     | N/A            |

**Note**: JPG and JPEG extensions are equivalent.

## Known Limitations

- Encrypted PDFs are not supported.
- File extensions are not a substitute for valid file contents; FluxConvert
  detects formats from file signatures and may reject files whose names do not
  match their contents.
- Very large PDFs require additional resource-limit validation and may use
  substantial memory while rendering.
- PDF output is raster-only; OCR, PDF text extraction, forms, and annotations
  are not supported.

## Configuration Options

### JPEG Options
- **Quality**: 1-100 (default: 90)
- **Background**: Color for PNG transparency (default: white)

### PNG Options
- **Compression**: 0-9 (default: 6)

### PDF Generation Options
- **Page size**: Fit to image, A4, Letter
- **Margins**: Configurable (default: 0mm)
- **Orientation**: Auto, portrait, landscape

### PDF Rendering Options
- **DPI**: 72-600 (default: 150)
- **Pages**: All, specific range, or single page
- **Output naming**: Auto-suffix for multi-page

## Building for Release

```bash
# Optimized release build
cmake -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

cmake --build build-release -j$(nproc)

# Strip symbols
strip build-release/fluxconvert-gui

# Check binary size
ls -lh build-release/fluxconvert-gui
```

## Testing

```bash
# Build with tests enabled
cmake -B build-test -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build-test

# Run all tests
ctest --test-dir build-test --output-on-failure

# Run with sanitizers (development)
cmake -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=ON \
  -DBUILD_TESTS=ON

cmake --build build-sanitize
ctest --test-dir build-sanitize
```

## Performance Benchmarks

```bash
cmake --build build-release --target benchmark
./build-release/benchmark --iterations 10
```

See `docs/performance.md` for detailed results.

## Documentation

- [Architecture](docs/architecture.md) - System design and module structure
- [Dependencies](docs/dependencies.md) - Dependency decisions and licenses
- [Supported Formats](docs/supported-formats.md) - Format specifications
- [Testing](docs/testing.md) - Test suite and coverage
- [Extending](docs/extending.md) - Adding new format support
- [Release packaging](packaging/) - Desktop integration and package validation

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program uses MuPDF, which is licensed under AGPL-3.0. Therefore, this application is also licensed under AGPL-3.0-or-later.

See [LICENSES.md](LICENSES.md) for full license texts and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency licenses.

## Contributing

Contributions are welcome! Please ensure:
- Code follows project style (clang-format provided)
- All tests pass
- New features include tests
- Memory leaks checked (sanitizers)
- Documentation updated

## Project Status

**Current Stage**: Stage 8 Complete - Release validation and Debian packaging

### Roadmap
- [x] Stage 1: Architecture and dependency selection
- [x] Stage 2: Core interfaces and test infrastructure
- [x] Stage 3: Image conversion engine (PNG/JPEG)
- [x] Stage 4: PDF generation
- [x] Stage 5: PDF rendering integration
- [x] Stage 6: GUI implementation
- [x] Stage 7: Optimization and benchmarking
- [x] Stage 8: Release validation and Debian packaging

## Support

For issues, questions, or contributions, please open an issue on GitHub.

## Acknowledgments

- **libpng**: PNG reference library
- **libjpeg-turbo**: High-performance JPEG codec
- **MuPDF**: PDF rendering engine by Artifex Software
- **GTK Project**: Desktop toolkit

---

**Note**: This is a work in progress. Not all features are implemented yet.
