#pragma once

#include "conversion_request.hpp"
#include "conversion_result.hpp"

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace converter {

struct JobConfig {
    size_t num_threads = 0; // 0 = auto-detect hardware concurrency
};

class JobExecutor {
public:
    explicit JobExecutor(size_t num_threads = 0);
    ~JobExecutor();

    JobExecutor(const JobExecutor&) = delete;
    JobExecutor& operator=(const JobExecutor&) = delete;

    // Number of active worker threads in the pool
    size_t thread_count() const;

    // Reconfigure thread count (restarts pool)
    void set_thread_count(size_t threads);

    // Submit a generic callable task to the worker pool
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        enqueue_raw_task([task]() { (*task)(); });
        return res;
    }

    // Execute batch conversion across multiple files concurrently
    ConversionResult execute_batch(
        const ConversionRequest& request,
        const std::function<FileConversionResult(
            const std::string& in_path,
            const std::string& out_path,
            const ConversionRequest& file_req)>& convert_fn
    );

    // Shutdown all worker threads
    void shutdown();

private:
    void enqueue_raw_task(std::function<void()> task);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace converter
