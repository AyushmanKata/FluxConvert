#include "converter/conversion_manager.hpp"
#include "converter/conversion_request.hpp"
#include "converter/conversion_result.hpp"
#include "converter/format.hpp"
#include "converter/image_data.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"
#include "converter/pdf_writer.hpp"
#include "converter/pdf_renderer.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <sys/resource.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace converter;

struct BenchmarkResult {
    std::string name;
    std::string category;
    size_t iterations = 0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double mean_ms = 0.0;
    double median_ms = 0.0;
    double throughput_mps = 0.0; // Megapixels per second (if applicable)
    double throughput_mbps = 0.0; // Megabytes per second
    size_t peak_rss_mb = 0;
};

// Utility: Read current RSS in KB
size_t get_current_rss_kb() {
    std::ifstream statm("/proc/self/statm");
    if (!statm) return 0;
    long pages = 0, resident = 0;
    statm >> pages >> resident;
    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
    return static_cast<size_t>(resident * page_size_kb);
}

// Utility: Read peak RSS in KB via getrusage
size_t get_peak_rss_kb() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<size_t>(usage.ru_maxrss);
    }
    return 0;
}

// Generate gradient image for testing
ImageData generate_test_image(uint32_t width, uint32_t height, uint8_t channels = 3) {
    ImageData img;
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.pixels.resize(img.expected_size());

    uint8_t* ptr = img.pixels.data();
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            ptr[0] = static_cast<uint8_t>((x * 255) / width);
            ptr[1] = static_cast<uint8_t>((y * 255) / height);
            ptr[2] = static_cast<uint8_t>(((x + y) * 128) / (width + height));
            if (channels == 4) {
                ptr[3] = static_cast<uint8_t>(200 + ((x * 55) / width)); // Slight alpha
            }
            ptr += channels;
        }
    }
    return img;
}

// Run a benchmark task across multiple iterations
template<typename Func>
BenchmarkResult run_benchmark(
    const std::string& name,
    const std::string& category,
    size_t iterations,
    double megapixels,
    Func&& task
) {
    std::vector<double> timings_ms;
    timings_ms.reserve(iterations);

    // Warm-up run
    task();

    for (size_t i = 0; i < iterations; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        task();
        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();
        timings_ms.push_back(elapsed);
    }

    std::sort(timings_ms.begin(), timings_ms.end());
    double min_ms = timings_ms.front();
    double max_ms = timings_ms.back();
    double sum = std::accumulate(timings_ms.begin(), timings_ms.end(), 0.0);
    double mean_ms = sum / static_cast<double>(iterations);
    double median_ms = (iterations % 2 == 0)
        ? (timings_ms[iterations / 2 - 1] + timings_ms[iterations / 2]) / 2.0
        : timings_ms[iterations / 2];

    BenchmarkResult res;
    res.name = name;
    res.category = category;
    res.iterations = iterations;
    res.min_ms = min_ms;
    res.max_ms = max_ms;
    res.mean_ms = mean_ms;
    res.median_ms = median_ms;
    if (megapixels > 0.0 && mean_ms > 0.0) {
        res.throughput_mps = (megapixels / (mean_ms / 1000.0));
    }
    res.peak_rss_mb = get_peak_rss_kb() / 1024;
    return res;
}

void print_table(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n"
              << std::string(98, '=') << "\n"
              << std::left << std::setw(32) << "Benchmark Scenario"
              << std::right << std::setw(8) << "Runs"
              << std::setw(11) << "Min (ms)"
              << std::setw(11) << "Mean (ms)"
              << std::setw(11) << "Max (ms)"
              << std::setw(14) << "Throughput"
              << std::setw(11) << "Peak RSS"
              << "\n"
              << std::string(98, '-') << "\n";

    std::string last_cat;
    for (const auto& r : results) {
        if (r.category != last_cat) {
            std::cout << "--- " << r.category << " " << std::string(92 - r.category.size(), '-') << "\n";
            last_cat = r.category;
        }

        std::string tp_str = (r.throughput_mps > 0.0)
            ? (std::to_string(static_cast<int>(r.throughput_mps)) + " MP/s")
            : "-";

        std::cout << std::left << std::setw(32) << r.name
                  << std::right << std::setw(8) << r.iterations
                  << std::setw(11) << std::fixed << std::setprecision(2) << r.min_ms
                  << std::setw(11) << std::fixed << std::setprecision(2) << r.mean_ms
                  << std::setw(11) << std::fixed << std::setprecision(2) << r.max_ms
                  << std::setw(14) << tp_str
                  << std::setw(9) << r.peak_rss_mb << " MB\n";
    }
    std::cout << std::string(98, '=') << "\n\n";
}

int main(int argc, char* argv[]) {
    size_t iterations = 5;
    bool quick = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations" && i + 1 < argc) {
            iterations = static_cast<size_t>(std::max(1, std::stoi(argv[++i])));
        } else if (arg == "--quick") {
            quick = true;
            iterations = 1;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --iterations <N>   Number of timed iterations per benchmark (default: 5)\n"
                      << "  --quick            Quick run (1 iteration)\n"
                      << "  -h, --help         Show this message\n";
            return 0;
        }
    }

    std::cout << "=================================================================================\n";
    std::cout << " FluxConvert - Performance Benchmark Suite\n";
    std::cout << " System CPU Concurrency: " << std::thread::hardware_concurrency() << " cores\n";
    std::cout << " Iterations per test:    " << iterations << "\n";
    std::cout << " Baseline Idle RSS:      " << (get_current_rss_kb() / 1024) << " MB\n";
    std::cout << "=================================================================================\n";

    fs::path bench_dir = "/tmp/fc_benchmarks";
    fs::create_directories(bench_dir);

    std::vector<BenchmarkResult> results;
    ConversionManager manager;

    try {
        // ── 1. Image Codec Benchmarks (1MP, 4MP, 16MP) ──────────────────────
        std::cout << ">> Generating test image assets...\n";

        // 1MP (1000x1000)
        std::string png_1mp = (bench_dir / "1mp.png").string();
        std::string jpg_1mp = (bench_dir / "1mp.jpg").string();
        ImageData img_1mp = generate_test_image(1000, 1000, 3);
        png_encode(img_1mp, png_1mp);
        jpeg_encode(img_1mp, jpg_1mp);

        // 4MP (2000x2000)
        std::string png_4mp = (bench_dir / "4mp.png").string();
        std::string jpg_4mp = (bench_dir / "4mp.jpg").string();
        ImageData img_4mp = generate_test_image(2000, 2000, 3);
        png_encode(img_4mp, png_4mp);
        jpeg_encode(img_4mp, jpg_4mp);

        // 16MP (4000x4000)
        std::string png_16mp = (bench_dir / "16mp.png").string();
        std::string jpg_16mp = (bench_dir / "16mp.jpg").string();
        ImageData img_16mp = generate_test_image(4000, 4000, 3);
        png_encode(img_16mp, png_16mp);
        jpeg_encode(img_16mp, jpg_16mp);

        std::cout << ">> Running Image Conversion Benchmarks...\n";

        // 1MP PNG -> JPEG
        {
            std::string out = (bench_dir / "out_1mp.jpg").string();
            ConversionRequest req;
            req.input_paths = {png_1mp};
            req.output_path = out;
            req.output_format = Format::JPEG;
            req.jpeg_options.quality = 90;

            results.push_back(run_benchmark("PNG -> JPEG (1 MP)", "Single Image Conversion",
                iterations, 1.0, [&]() {
                    manager.convert(req);
                }));
        }

        // 1MP JPEG -> PNG
        {
            std::string out = (bench_dir / "out_1mp.png").string();
            ConversionRequest req;
            req.input_paths = {jpg_1mp};
            req.output_path = out;
            req.output_format = Format::PNG;
            req.png_options.compression_level = 6;

            results.push_back(run_benchmark("JPEG -> PNG (1 MP)", "Single Image Conversion",
                iterations, 1.0, [&]() {
                    manager.convert(req);
                }));
        }

        // 4MP PNG -> JPEG
        {
            std::string out = (bench_dir / "out_4mp.jpg").string();
            ConversionRequest req;
            req.input_paths = {png_4mp};
            req.output_path = out;
            req.output_format = Format::JPEG;
            req.jpeg_options.quality = 90;

            results.push_back(run_benchmark("PNG -> JPEG (4 MP)", "Single Image Conversion",
                iterations, 4.0, [&]() {
                    manager.convert(req);
                }));
        }

        // 4MP JPEG -> PNG
        {
            std::string out = (bench_dir / "out_4mp.png").string();
            ConversionRequest req;
            req.input_paths = {jpg_4mp};
            req.output_path = out;
            req.output_format = Format::PNG;
            req.png_options.compression_level = 6;

            results.push_back(run_benchmark("JPEG -> PNG (4 MP)", "Single Image Conversion",
                iterations, 4.0, [&]() {
                    manager.convert(req);
                }));
        }

        // 16MP PNG -> JPEG
        {
            std::string out = (bench_dir / "out_16mp.jpg").string();
            ConversionRequest req;
            req.input_paths = {png_16mp};
            req.output_path = out;
            req.output_format = Format::JPEG;
            req.jpeg_options.quality = 90;

            results.push_back(run_benchmark("PNG -> JPEG (16 MP)", "Single Image Conversion",
                iterations, 16.0, [&]() {
                    manager.convert(req);
                }));
        }

        // 16MP JPEG -> PNG
        {
            std::string out = (bench_dir / "out_16mp.png").string();
            ConversionRequest req;
            req.input_paths = {jpg_16mp};
            req.output_path = out;
            req.output_format = Format::PNG;
            req.png_options.compression_level = 6;

            results.push_back(run_benchmark("JPEG -> PNG (16 MP)", "Single Image Conversion",
                iterations, 16.0, [&]() {
                    manager.convert(req);
                }));
        }

        // ── 2. PDF Generation & Rendering Benchmarks ────────────────────────
        std::cout << ">> Running PDF Generation and Rendering Benchmarks...\n";

        // Generate 10-page PDF from PNGs
        std::vector<std::string> ten_pngs(10, png_1mp);
        std::string pdf_from_png = (bench_dir / "doc_10p_png.pdf").string();

        results.push_back(run_benchmark("10x PNG -> PDF (A4)", "PDF Document Pipeline",
            iterations, 10.0, [&]() {
                ConversionRequest req;
                req.input_paths = ten_pngs;
                req.output_path = pdf_from_png;
                req.output_format = Format::PDF;
                req.pdf_write_options.page_size = PDFWriteOptions::PageSize::A4;
                manager.convert(req);
            }));

        // Generate 10-page PDF from JPEGs (direct embedding)
        std::vector<std::string> ten_jpegs(10, jpg_1mp);
        std::string pdf_from_jpeg = (bench_dir / "doc_10p_jpeg.pdf").string();

        results.push_back(run_benchmark("10x JPEG -> PDF (Direct Embed)", "PDF Document Pipeline",
            iterations, 10.0, [&]() {
                ConversionRequest req;
                req.input_paths = ten_jpegs;
                req.output_path = pdf_from_jpeg;
                req.output_format = Format::PDF;
                req.pdf_write_options.page_size = PDFWriteOptions::PageSize::A4;
                manager.convert(req);
            }));

        // Render PDF (10 pages) at 150 DPI
        results.push_back(run_benchmark("PDF (10p) -> PNG @ 150 DPI", "PDF Document Pipeline",
            iterations, 10.0 * 1.75, [&]() {
                ConversionRequest req;
                req.input_paths = {pdf_from_png};
                req.output_path = (bench_dir / "render_150").string();
                req.output_format = Format::PNG;
                req.pdf_read_options.dpi = 150;
                manager.convert(req);
            }));

        // Render PDF (10 pages) at 300 DPI
        results.push_back(run_benchmark("PDF (10p) -> PNG @ 300 DPI", "PDF Document Pipeline",
            iterations, 10.0 * 7.0, [&]() {
                ConversionRequest req;
                req.input_paths = {pdf_from_png};
                req.output_path = (bench_dir / "render_300").string();
                req.output_format = Format::PNG;
                req.pdf_read_options.dpi = 300;
                manager.convert(req);
            }));

        // ── 3. Multi-Threaded Batch Conversion & Concurrency Scaling ────────
        std::cout << ">> Running Batch Throughput and Concurrency Scaling Benchmarks...\n";

        const size_t batch_file_count = 24;
        std::vector<std::string> batch_inputs;
        batch_inputs.reserve(batch_file_count);
        fs::path batch_src_dir = bench_dir / "batch_src";
        fs::create_directories(batch_src_dir);

        for (size_t i = 0; i < batch_file_count; ++i) {
            std::string file_path = (batch_src_dir / ("batch_" + std::to_string(i) + ".png")).string();
            fs::copy_file(png_1mp, file_path, fs::copy_options::overwrite_existing);
            batch_inputs.push_back(file_path);
        }

        // Sequential (1 worker thread)
        double seq_mean_ms = 0.0;
        {
            ConversionManager seq_manager(1);
            fs::path seq_out = bench_dir / "batch_out_1t";
            fs::create_directories(seq_out);

            ConversionRequest req;
            req.input_paths = batch_inputs;
            req.output_path = seq_out.string();
            req.output_format = Format::JPEG;

            auto r = run_benchmark("Batch (24 files, 1 Thread)", "Batch Concurrency & Scaling",
                quick ? 1 : 3, static_cast<double>(batch_file_count), [&]() {
                    seq_manager.convert(req);
                });
            seq_mean_ms = r.mean_ms;
            results.push_back(r);
        }

        // Multi-threaded (4 worker threads)
        {
            ConversionManager p4_manager(4);
            fs::path p4_out = bench_dir / "batch_out_4t";
            fs::create_directories(p4_out);

            ConversionRequest req;
            req.input_paths = batch_inputs;
            req.output_path = p4_out.string();
            req.output_format = Format::JPEG;

            results.push_back(run_benchmark("Batch (24 files, 4 Threads)", "Batch Concurrency & Scaling",
                quick ? 1 : 3, static_cast<double>(batch_file_count), [&]() {
                    p4_manager.convert(req);
                }));
        }

        // Multi-threaded (All available hardware threads)
        size_t hw_threads = std::thread::hardware_concurrency();
        double par_mean_ms = 0.0;
        {
            ConversionManager par_manager(hw_threads);
            fs::path par_out = bench_dir / "batch_out_hwt";
            fs::create_directories(par_out);

            ConversionRequest req;
            req.input_paths = batch_inputs;
            req.output_path = par_out.string();
            req.output_format = Format::JPEG;

            std::string title = "Batch (24 files, " + std::to_string(hw_threads) + " Threads)";
            auto r = run_benchmark(title, "Batch Concurrency & Scaling",
                quick ? 1 : 3, static_cast<double>(batch_file_count), [&]() {
                    par_manager.convert(req);
                });
            par_mean_ms = r.mean_ms;
            results.push_back(r);
        }

        // Print final table
        print_table(results);

        if (seq_mean_ms > 0.0 && par_mean_ms > 0.0) {
            double speedup = seq_mean_ms / par_mean_ms;
            std::cout << ">>> Concurrency Speedup on " << hw_threads << " threads: "
                      << std::fixed << std::setprecision(2) << speedup << "x ("
                      << static_cast<int>(seq_mean_ms) << " ms -> "
                      << static_cast<int>(par_mean_ms) << " ms)\n";
        }

        std::cout << ">>> Peak Process Resident Memory (RSS): " << (get_peak_rss_kb() / 1024) << " MB\n\n";

        // Cleanup temporary benchmark files
        fs::remove_all(bench_dir);

    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed with exception: " << e.what() << "\n";
        fs::remove_all(bench_dir);
        return 1;
    }

    return 0;
}
