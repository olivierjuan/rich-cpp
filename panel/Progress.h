#pragma once
#include <rich_core.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

namespace rich::live {

class Progress {
public:
    Progress(size_t total) : total_(total), current_(0), running_(true) {
        thread_ = std::jthread([this]() {
            while (running_) {
                std::lock_guard<std::mutex> lock(mutex_);
                std::cout << "\rProgress: " << (current_ * 100 / total_) << "%" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    ~Progress() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

    void update(size_t progress) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_ = progress;
    }

private:
    size_t total_;
    std::atomic<size_t> current_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::jthread thread_;
};

} // namespace rich::live
