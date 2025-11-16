#include "threadpool.h"

ThreadPool::ThreadPool(int num_threads) {
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this]() {
            while (true) {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() { return !tasks.empty() || stop; });
                
                if (stop && tasks.empty()) break;
                
                auto task = tasks.front();
                tasks.pop();
                lock.unlock();
                
                task();
            }
        });
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push(task);
    }
    cv.notify_one();
}


ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop = true;
    }
    cv.notify_all();
    for (auto& t : threads) {
        t.join();
    }
}
