#include "converter/job_executor.hpp"
#include "converter/format.hpp"
#include "converter/error.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace converter {

namespace {

std::string get_default_extension(Format format) {
    switch (format) {
        case Format::PNG:  return ".png";
        case Format::JPEG: return ".jpg";
        case Format::PDF:  return ".pdf";
        default:           return ".out";
    }
}

size_t determine_default_threads() {
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    return static_cast<size_t>(std::clamp(hw, 1u, 16u));
}

} // anonymous namespace

class JobExecutor::Impl {
public:
    explicit Impl(size_t num_threads) {
        start_workers(num_threads == 0 ? determine_default_threads() : num_threads);
    }

    ~Impl() {
        stop_workers();
    }

    size_t thread_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return workers_.size();
    }

    void set_thread_count(size_t threads) {
        if (threads == 0) {
            threads = determine_default_threads();
        }
        stop_workers();
        start_workers(threads);
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                throw ConversionError("JobExecutor is stopping, cannot enqueue task");
            }
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    void stop_workers() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();

        // Clear remaining tasks
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<std::function<void()>> empty;
        std::swap(tasks_, empty);
        stopping_ = false;
    }

private:
    void start_workers(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = false;
        workers_.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            workers_.emplace_back([this]() {
                worker_loop();
            });
        }
    }

    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() {
                    return stopping_ || !tasks_.empty();
                });

                if (stopping_ && tasks_.empty()) {
                    return;
                }

                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
            }

            if (task) {
                try {
                    task();
                } catch (...) {
                    // Prevent uncaught exceptions from terminating worker threads
                }
            }
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    bool stopping_ = false;
};

// ── JobExecutor Public Interface ────────────────────────────────────────────

JobExecutor::JobExecutor(size_t num_threads)
    : impl_(std::make_unique<Impl>(num_threads)) {}

JobExecutor::~JobExecutor() = default;

size_t JobExecutor::thread_count() const {
    return impl_->thread_count();
}

void JobExecutor::set_thread_count(size_t threads) {
    impl_->set_thread_count(threads);
}

void JobExecutor::enqueue_raw_task(std::function<void()> task) {
    impl_->enqueue(std::move(task));
}

void JobExecutor::shutdown() {
    impl_->stop_workers();
}

ConversionResult JobExecutor::execute_batch(
    const ConversionRequest& request,
    const std::function<FileConversionResult(
        const std::string& in_path,
        const std::string& out_path,
        const ConversionRequest& file_req)>& convert_fn
) {
    ConversionResult result;
    auto start_time = std::chrono::steady_clock::now();

    const size_t total_files = request.input_paths.size();
    if (total_files == 0) {
        result.all_succeeded = true;
        return result;
    }

    // Determine target directory
    fs::path base_out_dir;
    if (!request.output_path.empty()) {
        fs::path p(request.output_path);
        // If output_path is an existing directory or has no extension or ends with separator
        if (fs::is_directory(p) || !p.has_extension() || request.output_path.back() == '/' || request.output_path.back() == '\\') {
            base_out_dir = p;
        } else if (total_files > 1) {
            // Specified a single file path for multiple files -> use its parent directory
            base_out_dir = p.parent_path();
            if (base_out_dir.empty()) base_out_dir = fs::current_path();
        } else {
            // Single file output
            base_out_dir = p.parent_path();
        }
    } else {
        base_out_dir = fs::current_path();
    }

    std::error_code ec;
    if (!base_out_dir.empty()) {
        fs::create_directories(base_out_dir, ec);
    }

    std::string out_ext = get_default_extension(request.output_format);

    // Pre-allocate indexed results to maintain ordering
    std::vector<FileConversionResult> indexed_results(total_files);
    std::vector<std::future<void>> futures;
    futures.reserve(total_files);

    std::atomic<size_t> completed_count{0};
    std::mutex progress_mutex;

    // Report initial progress
    if (request.progress_callback) {
        request.progress_callback(0, "Starting batch conversion (0/" + std::to_string(total_files) + ")...");
    }

    // Prepare a base request template once to avoid copying the full input_paths vector in every iteration
    ConversionRequest base_req = request;
    base_req.input_paths.clear();
    base_req.output_path.clear();

    for (size_t idx = 0; idx < total_files; ++idx) {
        const std::string in_path = request.input_paths[idx];

        // Determine destination file path
        std::string out_path;
        if (total_files == 1 && !request.output_path.empty() && fs::path(request.output_path).has_extension()) {
            out_path = request.output_path;
        } else {
            std::string stem = fs::path(in_path).stem().string();
            out_path = (base_out_dir / (stem + out_ext)).string();
        }

        // Prepare minimal per-file request
        ConversionRequest file_req = base_req;
        file_req.input_paths.push_back(in_path);
        file_req.output_path = out_path;

        auto future = submit([idx, in_path, out_path, file_req = std::move(file_req), &convert_fn, &indexed_results,
                              &completed_count, total_files, &request, &progress_mutex]() {
            // Check cancellation before starting
            if (request.cancellation_token && request.cancellation_token->is_cancelled()) {
                FileConversionResult res;
                res.input_path = in_path;
                res.output_path = out_path;
                res.success = false;
                res.error_message = "Conversion cancelled by user";
                indexed_results[idx] = res;
                return;
            }

            FileConversionResult res;
            try {
                res = convert_fn(in_path, out_path, file_req);
            } catch (const std::exception& e) {
                res.input_path = in_path;
                res.output_path = out_path;
                res.success = false;
                res.error_message = e.what();
            } catch (...) {
                res.input_path = in_path;
                res.output_path = out_path;
                res.success = false;
                res.error_message = "Unknown error occurred during conversion";
            }

            indexed_results[idx] = std::move(res);

            size_t done = ++completed_count;
            if (request.progress_callback) {
                int pct = static_cast<int>((done * 100) / total_files);
                std::string msg = "Converted " + fs::path(in_path).filename().string() +
                                  " (" + std::to_string(done) + "/" + std::to_string(total_files) + ")";
                std::lock_guard<std::mutex> p_lock(progress_mutex);
                request.progress_callback(pct, msg);
            }
        });

        futures.push_back(std::move(future));
    }

    // Wait for all tasks to finish
    for (auto& fut : futures) {
        fut.get();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.file_results = std::move(indexed_results);
    result.all_succeeded = (result.failure_count() == 0);

    return result;
}

} // namespace converter
