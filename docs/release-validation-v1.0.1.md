# FluxConvert 1.0.1 Release Validation Archive

**Validation date:** 2026-11-25
**Release commit:** `befa72b9d1d8a66c697dc061a02dfa971f6a99b1`
**Configuration:** Release (`build-release`)

## Automated tests

Command: `ctest --test-dir build-release --output-on-failure`

All 6 tests passed in 0.99 seconds:

| Test | Result |
|:---|:---|
| FormatDetection | Passed |
| Registry | Passed |
| ImageConversion | Passed |
| PdfGeneration | Passed |
| PdfRendering | Passed |
| BatchExecution | Passed |

## Benchmarks

Command: `./build-release/benchmark --iterations 10`

The benchmark reported 12-way CPU concurrency, 5 MB baseline idle RSS, and
199 MB peak process RSS. Image and PDF scenarios ran 10 timed iterations;
batch scenarios ran 3.

| Scenario | Runs | Min (ms) | Mean (ms) | Max (ms) | Throughput | Peak RSS |
|:---|---:|---:|---:|---:|---:|---:|
| PNG -> JPEG (1 MP) | 10 | 8.65 | 9.59 | 11.42 | 104 MP/s | 152 MB |
| JPEG -> PNG (1 MP) | 10 | 68.38 | 69.47 | 71.54 | 14 MP/s | 152 MB |
| PNG -> JPEG (4 MP) | 10 | 32.97 | 34.15 | 40.44 | 117 MP/s | 152 MB |
| JPEG -> PNG (4 MP) | 10 | 178.43 | 179.88 | 184.83 | 22 MP/s | 152 MB |
| PNG -> JPEG (16 MP) | 10 | 167.08 | 168.41 | 169.42 | 95 MP/s | 199 MB |
| JPEG -> PNG (16 MP) | 10 | 455.27 | 458.42 | 465.04 | 34 MP/s | 199 MB |
| 10x PNG -> PDF (A4) | 10 | 238.05 | 264.79 | 289.96 | 37 MP/s | 199 MB |
| 10x JPEG -> PDF (direct embed) | 10 | 0.61 | 0.73 | 1.23 | 13,783 MP/s | 199 MB |
| PDF (10 pages) -> PNG at 150 DPI | 10 | 734.46 | 742.04 | 751.27 | 23 MP/s | 199 MB |
| PDF (10 pages) -> PNG at 300 DPI | 10 | 2,253.76 | 2,308.90 | 2,361.71 | 30 MP/s | 199 MB |
| Batch (24 files, 1 thread) | 3 | 215.72 | 219.80 | 226.04 | 109 MP/s | 199 MB |
| Batch (24 files, 4 threads) | 3 | 77.20 | 78.65 | 79.98 | 305 MP/s | 199 MB |
| Batch (24 files, 12 threads) | 3 | 62.36 | 63.84 | 66.30 | 375 MP/s | 199 MB |

Batch conversion scaled 3.44x from one to twelve worker threads (219.80 ms to
63.84 ms).

## Published release asset checksums

Both uploaded GitHub release assets were downloaded and verified with
`sha256sum`.

| Asset | SHA-256 |
|:---|:---|
| `fluxconvert-1.0.1.tar.gz` | `852079284ee2880102f7c633474f46dde7d045cccea3160365223cd11d1666ec` |
| `fluxconvert_1.0.1_amd64.deb` | `94f89d537dfd554f0cce0b65856411d3a1bf64458dc212be32b6c784de7de017` |
