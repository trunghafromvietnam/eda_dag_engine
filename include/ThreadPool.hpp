#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>

// --- TASKQUEUE VÀ WORKERSTATE ---
class TaskQueue {
public:
    using Task = std::function<void()>;
private:
    std::queue<Task> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_{false};
public:
    void push(Task task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }
    bool pop(Task& task) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return !tasks_.empty() || stop_; });
        if (stop_ && tasks_.empty()) return false;
        task = std::move(tasks_.front());
        tasks_.pop();
        return true;
    }
    void stop() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
    }
};

struct alignas(64) WorkerState {
    std::atomic<bool> is_busy{false};
    std::atomic<uint64_t> tasks_completed{0};
};

// --- THREADPOOL ---
class ThreadPool {
private:
    TaskQueue queue_;
    std::vector<WorkerState> worker_states_;
    std::vector<std::thread> threads_;

public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
        : worker_states_(num_threads) {
        threads_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back([this, i]() {
                TaskQueue::Task task;
                while (queue_.pop(task)) {
                    worker_states_[i].is_busy.store(true, std::memory_order_relaxed);
                    task(); // Chạy packaged_task thông qua lambda
                    worker_states_[i].tasks_completed.fetch_add(1, std::memory_order_relaxed);
                    worker_states_[i].is_busy.store(false, std::memory_order_relaxed);
                }
            });
        }
    }

    // ENHANCED ENQUEUE FUNCTION: SUPPORT FOR RETURNING VALUES VIA STD::FUTURE
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> 
    {
        // 1. Trích xuất kiểu dữ liệu trả về của hàm F (Ví dụ: int, double, std::string)
        using return_type = typename std::invoke_result<F, Args...>::type;

        // 2. Đóng gói hàm f và tham số args thành một packaged_task
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        // 3. Rút lấy tấm phiếu std::future để trả về cho người gọi
        std::future<return_type> res = task->get_future();

        // 4. Biến packaged_task thành một lambda void() để nhét vào TaskQueue
        queue_.push([task]() { 
            (*task)(); 
        });

        return res; // Trả về tấm phiếu
    }

    ~ThreadPool() {
        queue_.stop();
        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }
};