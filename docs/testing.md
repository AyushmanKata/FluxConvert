# Testing & Quality Assurance

## Overview

FluxConvert includes a comprehensive automated test and benchmark suite covering format detection, handler registry, image codecs, PDF generation/rendering pipelines, and concurrent batch processing.

All tests operate offline against deterministic test fixtures generated at build time.

---

## Test Organization

Tests are located under the [`tests/`](file:///home/ayushman/Projects/file-converter/tests) directory and divided into two categories:

### 1. Unit Tests (`tests/unit/`)
* **[`test_format_detection`](file:///home/ayushman/Projects/file-converter/tests/unit/test_format_detection.cpp)**:
  * Extension parsing (case-insensitivity, leading dot handling).
  * Magic-number byte sniffing for PNG (`89 50 4E 47...`), JPEG (`FF D8 FF`), and PDF (`%PDF-`).
  * Truncated/corrupted file detection fallback.
* **[`test_registry`](file:///home/ayushman/Projects/file-converter/tests/unit/test_registry.cpp)**:
  * Singleton accessor [`FormatRegistry::instance()`](file:///home/ayushman/Projects/file-converter/include/converter/format_registry.hpp).
  * Independent, isolated [`FormatRegistry`](file:///home/ayushman/Projects/file-converter/include/converter/format_registry.hpp) instances for testing without global state contamination.
  * O(1) handler registration, replacement, and capability querying.

### 2. Integration Tests (`tests/integration/`)
* **[`test_image_conversion`](file:///home/ayushman/Projects/file-converter/tests/integration/test_image_conversion.cpp)**:
  * Lossless PNG $\to$ JPEG round-trips with RGB & RGBA alpha blending.
  * Quality scaling verification.
  * Corrupt input handling and error propagation.
* **[`test_pdf_generation`](file:///home/ayushman/Projects/file-converter/tests/integration/test_pdf_generation.cpp)**:
  * Single-image to PDF generation (A4, Letter, FitToImage).
  * Direct JPEG embedding (zero recompression, `/DCTDecode`).
  * PNG embedding via Flate compression.
  * Multi-page PDF generation combining mixed image formats.
* **[`test_pdf_rendering`](file:///home/ayushman/Projects/file-converter/tests/integration/test_pdf_rendering.cpp)**:
  * MuPDF-backed rendering to PNG and JPEG.
  * DPI scaling verification (72 to 600 DPI).
  * Page range extraction and metadata inspection.
* **[`test_batch_execution`](file:///home/ayushman/Projects/file-converter/tests/integration/test_batch_execution.cpp)**:
  * [`JobExecutor`](file:///home/ayushman/Projects/file-converter/include/converter/job_executor.hpp) thread-pool dynamic resizing and task scheduling.
  * Multi-threaded batch conversion ordering and progress callback reporting.
  * Cooperative [`CancellationToken`](file:///home/ayushman/Projects/file-converter/include/converter/progress.hpp) cancellation.

---

## Running the Test Suite

### Via CTest (Recommended)

From the release or debug build directory:

```bash
ctest --test-dir build-release --output-on-failure
```

Or using CMake Presets:

```bash
cmake --build --preset release
ctest --preset release --output-on-failure
```

### Running Specific Tests

Individual test binaries can be executed directly with verbose diagnostic output:

```bash
./build-release/test_format_detection
./build-release/test_registry
./build-release/test_image_conversion
./build-release/test_pdf_generation
./build-release/test_pdf_rendering
./build-release/test_batch_execution
```

---

## Benchmark Suite

FluxConvert includes an automated performance and memory benchmark suite in [`benchmarks/main_benchmark.cpp`](file:///home/ayushman/Projects/file-converter/benchmarks/main_benchmark.cpp).

To run a quick performance check:

```bash
./build-release/benchmark --quick
```

To run the full multi-iteration benchmark suite:

```bash
./build-release/benchmark
```

The benchmark reports:
* Throughput in Megapixels/sec (MP/s) and execution times (min, mean, max) across:
  * Single image conversion (1 MP, 4 MP, 16 MP).
  * PDF generation and rendering pipelines.
  * Multi-core concurrency scaling (1 thread vs. 4 threads vs. all hardware threads).
* Peak Process Resident Set Size (RSS memory in MB) via `/proc/self/statm` and `getrusage`.

---

## Sanitizer Builds

To validate memory safety and undefined behavior:

```bash
cmake --preset asan
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```
