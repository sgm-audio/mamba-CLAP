#pragma once
// InferenceQueue — fixed-size thread pool for ARA offline inference jobs.
//
// For real-time inference, the MambaInferenceEngine uses its own dedicated
// single worker thread instead of this pool (lower scheduling jitter).
// InferenceQueue is exclusively for ARA region analysis tasks which have
// no real-time deadline and may be long-running.
//
// Jobs are posted via submit() and executed FIFO.
// The pool is torn down gracefully in ~InferenceQueue().

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>
#include <future>

namespace mamba::model {

class InferenceQueue {
public:
    using Job = std::function<void()>;

    explicit InferenceQueue(uint32_t num_workers = 2);
    ~InferenceQueue();

    // Post a job; returns a future that resolves when the job completes.
    // Throws std::runtime_error if the queue is shutting down.
    [[nodiscard]] std::future<void> submit(Job job);

    // Cancel all pending (not yet started) jobs.
    void cancel_pending();

    uint32_t pending_count() const noexcept;

private:
    void worker_loop();

    uint32_t                  num_workers_;
    std::vector<std::thread>  workers_;
    std::queue<std::packaged_task<void()>> pending_;
    mutable std::mutex        mutex_;
    std::condition_variable   cv_;
    std::atomic<bool>         shutdown_{false};
};

} // namespace mamba::model
