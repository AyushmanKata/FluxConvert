#pragma once

#include <functional>
#include <string>
#include <optional>
#include <cstdint>
#include <atomic>

namespace converter {

// Progress callback: reports progress percentage (0-100) and optional status message
using ProgressCallback = std::function<void(int percentage, const std::string& message)>;

// Cancellation token: thread-safe check if operation should be cancelled
class CancellationToken {
public:
    CancellationToken() : cancelled_(false) {}

    void cancel() { cancelled_.store(true, std::memory_order_release); }
    bool is_cancelled() const { return cancelled_.load(std::memory_order_acquire); }
    void reset() { cancelled_.store(false, std::memory_order_release); }

private:
    std::atomic<bool> cancelled_;
};

} // namespace converter
