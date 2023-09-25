#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

class ThreadPool{
public:
    ThreadPool(int thread_number = 8);
    ThreadPool(ThreadPool&&) = default;
    ~ThreadPool();
    template<class F, class... Args>
    auto AddTask(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

private:
    std::vector<std::thread> pool_;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_lock;
    std::condition_variable condition;
    bool stop; 
};

inline ThreadPool::ThreadPool(int thread_number) {
    stop = false;
    for (int i = 0; i < thread_number; i++) {
        pool_.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                std::unique_lock<std::mutex> lock(this->queue_lock);
                // 等待队列非空或停止标志位为true
                this->condition.wait(lock, [this] {return this->stop || !this->tasks.empty();});
                if (this->stop && this->tasks.empty()) {
                    return ;
                }
                
                task = std::move(this->tasks.front());  // 取出任务
                this->tasks.pop();                      // 从任务队列中移除

                task(); // 执行任务
            }
        }
        );
    }
}

inline ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_lock);
        stop = true;
    }
    condition.notify_all(); // 唤醒所有线程
    for (std::thread &thread : pool_) {
        thread.join();  // 等待线程结束
    }
}

template<class F, class... Args>
auto ThreadPool::AddTask(F&& f, Args&&... args) -> std::future<decltype(f(args...))>{
    std::function<void()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(task);
    std::function<void()> warrper_task = [task_ptr]() {
        (*task_ptr)();
    };
    {
        std::unique_lock<std::mutex> lock(queue_lock);
        if (stop)
            throw std::runtime_error("submit on stopped ThreadPool");

        tasks.emplace(warrper_task);
    }
    condition.notify_one();

    return task_ptr->get_future();
}


#endif //THREADPOOL_H