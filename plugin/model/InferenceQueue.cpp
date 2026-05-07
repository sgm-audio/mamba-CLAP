#include "model/InferenceQueue.hpp"
#include <stdexcept>

namespace mamba::model {

InferenceQueue::InferenceQueue(uint32_t num_workers)
    : num_workers_(num_workers)
{
    workers_.reserve(num_workers_);
    for (uint32_t i = 0; i < num_workers_; ++i)
        workers_.emplace_back(&InferenceQueue::worker_loop, this);
}

InferenceQueue::~InferenceQueue() {
    {
        std::lock_guard lock(mutex_);
        shutdown_.store(true, std::memory_order_release);
    }
    cv_.notify_all();
    for (auto& t : workers_)
        if (t.joinable()) t.join();
}

std::future<void> InferenceQueue::submit(Job job) {
    std::packaged_task<void()> task(std::move(job));
    auto future = task.get_future();
    {
        std::lock_guard lock(mutex_);
        if (shutdown_.load(std::memory_order_relaxed))
            throw std::runtime_error("InferenceQueue: submit after shutdown");
        pending_.push(std::move(task));
    }
    cv_.notify_one();
    return future;
}

void InferenceQueue::cancel_pending() {
    std::lock_guard lock(mutex_);
    while (!pending_.empty()) pending_.pop();
}

uint32_t InferenceQueue::pending_count() const noexcept {
    std::lock_guard lock(mutex_);
    return static_cast<uint32_t>(pending_.size());
}

void InferenceQueue::worker_loop() {
    while (true) {
        std::packaged_task<void()> task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] {
                return !pending_.empty() || shutdown_.load(std::memory_order_relaxed);
            });
            if (shutdown_.load(std::memory_order_relaxed) && pending_.empty())
                return;
            task = std::move(pending_.front());
            pending_.pop();
        }
        task();
    }
}

} // namespace mamba::model
