# Architecture Documentation

## Overview

Ultra-lightweight modular file converter for Zorin OS, designed for offline operation with minimal resource footprint.

**Version:** 1.0  
**Date:** 2026-10-24

## Design Principles

1. **Low resource usage**: Target <50MB idle RAM, fast startup, bounded memory during conversion
2. **Modularity**: Plugin-style format handlers with compile-time registry
3. **Dependency inversion**: Core logic independent of GUI and specific codec implementations
4. **Offline-first**: No network dependencies, no telemetry, no cloud services
5. **Correctness over cleverness**: Reliable conversions with comprehensive testing

## Technology Stack

### Core Implementation
- **Language**: C++20 (GCC 13.3.0)
- **Build system**: CMake 3.28.3 + CTest
- **Standard library**: libstdc++

### GUI
- **Toolkit**: GTK4 (4.14.5) with gtkmm-4.0 (4.10.0)
- **Threading**: Asynchronous conversion jobs with progress callbacks
- **Integration**: Native GNOME/Zorin OS desktop experience

### Image Processing
- **PNG**: libpng 1.6.43 (zlib license - permissive)
- **JPEG**: libjpeg-turbo 2.1.5 (BSD-style license - permissive)
- **Rationale**: Industry-standard, battle-tested, optimized codecs

### PDF Support
- **Generation**: Custom minimal PDF writer module
  - Direct JPEG embedding (DCTDecode filter)
  - PNG: RGB/RGBA → FlateDecode compressed streams
  - Supports single and multi-page documents
- **Rendering**: MuPDF 1.23.10 (AGPL-3+)
  - **License implication**: Application must be AGPL-3+ licensed
  - **Distribution requirement**: Source availability obligation
  - **Justification**: Compact, high-quality renderer; alternatives (Poppler) are larger
  - **Replaceability**: PDF renderer isolated behind adapter interface

## Architecture Layers

```
┌──────────────────────────────────────┐
│  Presentation Layer (GTK4 GUI/CLI)   │
└──────────────────────────────────────┘
              ↓
┌──────────────────────────────────────┐
│   Conversion Manager & Registry      │
│  (format routing, job orchestration) │
└──────────────────────────────────────┘
              ↓
┌──────────────────────────────────────┐
│    Format Plugin Interfaces          │
│  (capabilities, conversion requests) │
└──────────────────────────────────────┘
              ↓
┌──────────────────────────────────────┐
│   Format Implementations             │
│  PNG Handler | JPEG Handler | PDF    │
└──────────────────────────────────────┘
              ↓
┌──────────────────────────────────────┐
│   Codec Adapters (thin wrappers)     │
│  libpng | libjpeg-turbo | MuPDF      │
└──────────────────────────────────────┘
```

### Key Abstractions

#### ConversionRequest
- Source file path(s)
- Detected input format
- Desired output format
- Output path or directory
- Format-specific options (quality, DPI, page size, etc.)
- Progress callback
- Cancellation token

#### ConversionResult
- Success/failure status
- Output file path(s)
- Metrics (bytes processed, time elapsed)
- Error information with context

#### FormatHandler Interface
- `can_read(format) -> bool`
- `can_write(format) -> bool`
- `get_capabilities() -> Capabilities`
- `convert(request) -> Result`
- `identify_format(stream) -> Format`

#### FormatRegistry
- Compile-time registration of handlers
- Capability queries
- Routing logic: finds conversion path (direct or via intermediate format)

## Module Structure

```
fluxconvert/
├── include/converter/          # Public API headers
│   ├── conversion_request.hpp
│   ├── conversion_result.hpp
│   ├── format_handler.hpp
│   ├── format_registry.hpp
│   ├── conversion_manager.hpp
│   ├── progress.hpp
│   ├── cancellation.hpp
│   └── error.hpp
├── src/
│   ├── core/                   # Core engine (format-agnostic)
│   │   ├── registry.cpp
│   │   ├── manager.cpp
│   │   └── format_detection.cpp
│   ├── formats/                # Format handlers
│   │   ├── png/
│   │   │   ├── png_handler.hpp
│   │   │   ├── png_handler.cpp
│   │   │   └── png_adapter.cpp (libpng wrapper)
│   │   ├── jpeg/
│   │   │   ├── jpeg_handler.hpp
│   │   │   ├── jpeg_handler.cpp
│   │   │   └── jpeg_adapter.cpp (libjpeg-turbo wrapper)
│   │   └── pdf/
│   │       ├── pdf_handler.hpp
│   │       ├── pdf_handler.cpp
│   │       ├── pdf_writer.cpp (minimal PDF generator)
│   │       └── pdf_renderer.cpp (MuPDF wrapper)
│   ├── pipeline/               # Job execution
│   │   ├── job_executor.cpp
│   │   └── resource_limits.cpp
│   ├── platform/               # OS utilities
│   │   ├── file_utils.cpp
│   │   └── tempfile.cpp
│   ├── cli/                    # Command-line test runner
│   │   └── main_cli.cpp
│   └── gui/                    # GTK4 interface
│       ├── main_window.hpp
│       ├── main_window.cpp
│       ├── conversion_queue_view.cpp
│       └── main.cpp
├── tests/                      # Comprehensive test suite
└── benchmarks/                 # Performance measurement
```

## Conversion Support Matrix

| From ↓ To → | PNG | JPEG | PDF |
|-------------|-----|------|-----|
| PNG         | N/A | ✓    | ✓   |
| JPEG        | ✓   | N/A  | ✓   |
| PDF         | ✓   | ✓    | N/A |

**Note**: JPG and JPEG are equivalent (same handler).

### Special Features
- **Multi-image to PDF**: Combine multiple PNG/JPEG into one multipage PDF
- **Batch conversion**: Process multiple files in one operation
- **Transparency handling**: Configurable background when PNG → JPEG
- **PDF page selection**: Convert specific pages or all pages

## Resource Management

### Memory Limits
- **Max image dimensions**: 32768 × 32768 (configurable)
- **Max decoded pixel count**: 256 MP per image (configurable)
- **Max PDF pages processed**: 1000 (configurable)
- **Sequential processing**: One file at a time by default to bound memory

### File Safety
- Atomic writes: convert to temp file → rename on success
- Input validation: magic number check, not just extension
- Output collision handling: prompt or auto-rename
- Temp cleanup: RAII guards for partial results
- No in-place conversion: never overwrite input

### Threading Model (GUI)
- **Main thread**: GTK event loop only
- **Worker thread**: conversion job execution
- **Communication**: thread-safe progress queue
- **Cancellation**: checked at key points (per-page, per-file)

## Extension Model

### Adding a New Format (e.g., WebP)

1. **Create handler module**: `src/formats/webp/`
2. **Implement FormatHandler interface**:
   ```cpp
   class WebPHandler : public FormatHandler {
     Capabilities get_capabilities() override;
     Result convert(const Request& req) override;
   };
   ```
3. **Register in registry**: `registry.register_handler(std::make_unique<WebPHandler>());`
4. **Add tests**: unit tests + integration tests for WebP ↔ PNG/JPEG/PDF
5. **Update docs**: supported-formats.md

**No changes required to**:
- Conversion manager routing logic
- GUI code
- Existing PNG/JPEG/PDF handlers

## Performance Targets

*Validated on target system: AMD Ryzen 5 7535HS, 12GB RAM*

- **Cold startup**: <1 second (GUI launch)
- **Idle RAM**: <50 MB
- **Small image (1MP PNG→JPEG)**: <100ms
- **Large image (24MP PNG→JPEG)**: Bounded memory, responsive UI
- **PDF generation (10 pages)**: <2 seconds
- **Binary size (stripped)**: <5 MB (measured post-build)

## Security Considerations

- **Input validation**: Reject malformed images gracefully (no crash)
- **Resource exhaustion**: Dimension/page limits prevent DoS
- **Path traversal**: Output paths sanitized
- **No shell execution**: Direct API calls only, no `system()` with user input
- **Temp file security**: Atomic creation in user-controlled directory

## Licensing

**Application License**: AGPL-3+ (required by MuPDF dependency)

**Dependency Licenses**:
- libpng: zlib license (permissive, compatible)
- libjpeg-turbo: BSD-style (permissive, compatible)
- MuPDF: AGPL-3+ (copyleft, same as application)
- GTK4/gtkmm: LGPL-2.1+ (library exception, compatible)

**Distribution Obligations**:
- Source code availability (AGPL requirement)
- License and copyright notices
- Build instructions

See LICENSES.md for full text.
