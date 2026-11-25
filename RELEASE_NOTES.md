# FluxConvert 1.0.1

FluxConvert 1.0.1 is a security and reliability update for the offline image
and PDF converter for Ubuntu 24.04 and compatible Ubuntu-based distributions.
Zorin OS 17 or later is the primary tested target.

## Highlights

- Requires MuPDF 1.28.6 or later for PDF rendering security fixes.
- Explicitly rejects encrypted PDFs instead of rendering them unexpectedly.
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
`fluxconvert_1.0.1_*.deb` package on a supported Debian-based system. See
`README.md` for dependency and usage details.

## SHA-256 checksums

| Release asset | SHA-256 |
|:---|:---|
| `fluxconvert-1.0.1.tar.gz` | `852079284ee2880102f7c633474f46dde7d045cccea3160365223cd11d1666ec` |
| `fluxconvert_1.0.1_amd64.deb` | `94f89d537dfd554f0cce0b65856411d3a1bf64458dc212be32b6c784de7de017` |
