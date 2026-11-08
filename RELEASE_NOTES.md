# FluxConvert 1.0.0

FluxConvert 1.0.0 is the first stable release of the offline image and PDF
converter for Ubuntu 24.04 and compatible Ubuntu-based distributions.
Zorin OS 17 or later is the primary tested target.

## Highlights

- PNG and JPEG conversion with configurable quality and compression.
- PNG/JPEG to single- or multi-page PDF conversion.
- PDF rendering to PNG or JPEG with configurable DPI.
- Concurrent batch conversion with progress reporting and cancellation.
- Native GTK4 desktop application and command-line interface.
- Debian package with desktop entry, icon, AppStream metadata, and license
  documentation.

## Known limitations

- PDF input is rasterized; PDF text and vector content are not preserved.
- Encrypted PDFs are not supported.
- OCR, PDF text extraction, animation, and unsupported image formats are not
  provided.
- Cancellation is cooperative and may finish the current file before stopping.

## Installation

Build from source using the `release` CMake preset, or install the generated
`fluxconvert_1.0.0_*.deb` package on a supported Debian-based system. See
`README.md` for dependency and usage details.
