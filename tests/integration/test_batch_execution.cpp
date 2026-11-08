#include "converter/job_executor.hpp"
#include "converter/conversion_manager.hpp"
#include "converter/format_registry.hpp"
#include "converter/error.hpp"
#include "converter/png_adapter.hpp"
#include "converter/jpeg_adapter.hpp"

#include <iostream>
#include <filesystem>
#include <cassert>
#include <vector>
#include <chrono>

namespace fs = std::filesystem;
using namespace converter;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    std::cout << "TEST: " << name << "... "; \
    tests_run++; \
    try

#define PASS() \
    std::cout << "PASS\n"; \
    tests_passed++

#define FAIL(msg) \
    std::cout << "FAIL: " << msg << "\n"

void test_job_executor_basic() {
    TEST("JobExecutor task submission and thread count") {
        JobExecutor executor(4);
        assert(executor.thread_count() == 4);

        auto fut1 = executor.submit([]() { return 42; });
        auto fut2 = executor.submit([](int a, int b) { return a * b; }, 6, 7);

        assert(fut1.get() == 42);
        assert(fut2.get() == 42);

        executor.set_thread_count(2);
        assert(executor.thread_count() == 2);

        auto fut3 = executor.submit([]() { return 100; });
        assert(fut3.get() == 100);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_batch_png_to_jpeg() {
    TEST("Multi-threaded batch PNG to JPEG conversion") {
        fs::path out_dir = "tests/output/batch_png_jpeg";
        fs::create_directories(out_dir);

        // Prepare 6 input files using fixtures
        std::vector<std::string> inputs;
        for (int i = 0; i < 6; ++i) {
            std::string src = (i % 2 == 0) ? "tests/fixtures/test_rgb.png" : "tests/fixtures/test_rgba.png";
            std::string copy_dest = (out_dir / ("input_" + std::to_string(i) + ".png")).string();
            fs::copy_file(src, copy_dest, fs::copy_options::overwrite_existing);
            inputs.push_back(copy_dest);
        }

        ConversionRequest request;
        request.input_paths = inputs;
        request.output_format = Format::JPEG;
        request.output_path = out_dir.string();
        request.jpeg_options.quality = 85;

        ConversionManager manager(4);
        auto result = manager.convert(request);

        assert(result.all_succeeded);
        assert(result.file_results.size() == 6);
        assert(result.success_count() == 6);
        assert(result.failure_count() == 0);

        for (int i = 0; i < 6; ++i) {
            std::string expected_out = (out_dir / ("input_" + std::to_string(i) + ".jpg")).string();
            assert(fs::exists(expected_out));
            assert(fs::file_size(expected_out) > 0);
            assert(result.file_results[static_cast<size_t>(i)].input_path == inputs[static_cast<size_t>(i)]);
            assert(result.file_results[static_cast<size_t>(i)].output_path == expected_out);

            // Verify decoded dimensions
            ImageData decoded = jpeg_decode(expected_out);
            assert(decoded.width == 100);
            assert(decoded.height == 100);
        }

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_batch_jpeg_to_png() {
    TEST("Multi-threaded batch JPEG to PNG conversion") {
        fs::path out_dir = "tests/output/batch_jpeg_png";
        fs::create_directories(out_dir);

        std::vector<std::string> inputs;
        for (int i = 0; i < 4; ++i) {
            std::string src = "tests/fixtures/test.jpg";
            std::string copy_dest = (out_dir / ("jpeg_in_" + std::to_string(i) + ".jpg")).string();
            fs::copy_file(src, copy_dest, fs::copy_options::overwrite_existing);
            inputs.push_back(copy_dest);
        }

        ConversionRequest request;
        request.input_paths = inputs;
        request.output_format = Format::PNG;
        request.output_path = out_dir.string();
        request.png_options.compression_level = 6;

        ConversionManager manager(4);
        auto result = manager.convert(request);

        assert(result.all_succeeded);
        assert(result.success_count() == 4);

        for (int i = 0; i < 4; ++i) {
            std::string expected_out = (out_dir / ("jpeg_in_" + std::to_string(i) + ".png")).string();
            assert(fs::exists(expected_out));
            ImageData decoded = png_decode(expected_out);
            assert(decoded.width == 100);
            assert(decoded.height == 100);
        }

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_batch_progress_reporting() {
    TEST("Batch conversion progress reporting") {
        fs::path out_dir = "tests/output/batch_progress";
        fs::create_directories(out_dir);

        std::vector<std::string> inputs;
        for (int i = 0; i < 5; ++i) {
            std::string copy_dest = (out_dir / ("prog_" + std::to_string(i) + ".png")).string();
            fs::copy_file("tests/fixtures/test_rgb.png", copy_dest, fs::copy_options::overwrite_existing);
            inputs.push_back(copy_dest);
        }

        ConversionRequest request;
        request.input_paths = inputs;
        request.output_format = Format::JPEG;
        request.output_path = out_dir.string();

        std::vector<int> progress_values;
        std::mutex p_mutex;

        request.progress_callback = [&](int pct, const std::string& /*msg*/) {
            std::lock_guard<std::mutex> lock(p_mutex);
            progress_values.push_back(pct);
        };

        ConversionManager manager(2);
        auto result = manager.convert(request);

        assert(result.all_succeeded);
        assert(!progress_values.empty());
        // Must end with 100%
        assert(progress_values.back() == 100);

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_batch_cancellation() {
    TEST("Batch conversion cancellation via CancellationToken") {
        fs::path out_dir = "tests/output/batch_cancel";
        fs::remove_all(out_dir);
        fs::create_directories(out_dir);

        std::vector<std::string> inputs;
        for (int i = 0; i < 10; ++i) {
            std::string copy_dest = (out_dir / ("cancel_in_" + std::to_string(i) + ".png")).string();
            fs::copy_file("tests/fixtures/test_rgb.png", copy_dest, fs::copy_options::overwrite_existing);
            inputs.push_back(copy_dest);
        }

        ConversionRequest request;
        request.input_paths = inputs;
        request.output_format = Format::JPEG;
        request.output_path = out_dir.string();

        auto token = std::make_shared<CancellationToken>();
        request.cancellation_token = token;

        // Cancel immediately before run
        token->cancel();

        ConversionManager manager(2);
        auto result = manager.convert(request);

        assert(!result.all_succeeded);
        assert(result.failure_count() > 0);
        assert(result.file_results.size() == 10);
        for (const auto& entry : fs::directory_iterator(out_dir)) {
            assert(entry.path().extension() != ".jpg");
        }

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

void test_batch_partial_failures() {
    TEST("Batch conversion error isolation on invalid files") {
        fs::path out_dir = "tests/output/batch_partial";
        fs::create_directories(out_dir);

        std::vector<std::string> inputs = {
            "tests/fixtures/test_rgb.png",
            "tests/fixtures/corrupt.png",
            "tests/fixtures/test_rgba.png"
        };

        ConversionRequest request;
        request.input_paths = inputs;
        request.output_format = Format::JPEG;
        request.output_path = out_dir.string();

        ConversionManager manager(2);
        auto result = manager.convert(request);

        assert(!result.all_succeeded);
        assert(result.success_count() == 2);
        assert(result.failure_count() == 1);
        assert(result.file_results[1].success == false);
        assert(!result.file_results[1].error_message.empty());

        PASS();
    } catch (const std::exception& e) {
        FAIL(e.what());
    }
}

int main() {
    std::cout << "\n=== Running Batch Execution Integration Tests ===\n\n";

    test_job_executor_basic();
    test_batch_png_to_jpeg();
    test_batch_jpeg_to_png();
    test_batch_progress_reporting();
    test_batch_cancellation();
    test_batch_partial_failures();

    std::cout << "\n================================================\n";
    std::cout << "Tests run: " << tests_run << ", Passed: " << tests_passed
              << ", Failed: " << (tests_run - tests_passed) << "\n";
    std::cout << "================================================\n\n";

    return (tests_run == tests_passed) ? 0 : 1;
}
