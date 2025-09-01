#pragma once
#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <type_traits>

namespace voxelvk {

// Thread-safe task queue for the thread pool
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Submit a task and get a future for the result
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // Submit a task without expecting a result
    template<typename F, typename... Args>
    void SubmitDetached(F&& f, Args&&... args);
    
    // Wait for all queued tasks to complete
    void WaitForAll();
    
    // Get the number of threads
    size_t GetNumThreads() const { return m_threads.size(); }
    
    // Get the number of queued tasks
    size_t GetQueueSize() const;
    
    // Get the number of active (running) tasks
    size_t GetActiveTaskCount() const { return m_active_tasks.load(); }
    
    // Shutdown the thread pool (finish current tasks, reject new ones)
    void Shutdown();
    
    // Check if the thread pool is shutting down
    bool IsShuttingDown() const { return m_shutdown.load(); }
    
private:
    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_shutdown{false};
    std::atomic<size_t> m_active_tasks{0};
    
    void WorkerThread();
};

// Template implementation
template<typename F, typename... Args>
auto ThreadPool::Submit(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    if (m_shutdown.load()) {
        throw std::runtime_error("ThreadPool is shutting down, cannot submit new tasks");
    }
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_tasks.emplace([task]() { (*task)(); });
    }
    
    m_condition.notify_one();
    return result;
}

template<typename F, typename... Args>
void ThreadPool::SubmitDetached(F&& f, Args&&... args) {
    if (m_shutdown.load()) {
        return; // Silently ignore if shutting down
    }
    
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_tasks.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }
    
    m_condition.notify_one();
}

// Utility class for parallel execution patterns
class ParallelFor {
public:
    // Execute a function in parallel over a range
    template<typename Func>
    static void Execute(size_t start, size_t end, Func&& func, 
                       ThreadPool& pool, size_t chunk_size = 0);
    
    // Execute a function in parallel over a range with automatic chunking
    template<typename Func>
    static void Execute(size_t start, size_t end, Func&& func, 
                       ThreadPool& pool);
                       
    // Execute a function in parallel over a container
    template<typename Container, typename Func>
    static void Execute(Container& container, Func&& func, ThreadPool& pool);
    
    template<typename Container, typename Func>
    static void Execute(const Container& container, Func&& func, ThreadPool& pool);
};

// Template implementations for ParallelFor
template<typename Func>
void ParallelFor::Execute(size_t start, size_t end, Func&& func, 
                         ThreadPool& pool, size_t chunk_size) {
    if (start >= end) return;
    
    const size_t total_work = end - start;
    const size_t num_threads = pool.GetNumThreads();
    
    if (chunk_size == 0) {
        // Automatic chunking: aim for 2-4 chunks per thread
        chunk_size = std::max(size_t(1), total_work / (num_threads * 2));
    }
    
    std::vector<std::future<void>> futures;
    futures.reserve((total_work + chunk_size - 1) / chunk_size);
    
    for (size_t i = start; i < end; i += chunk_size) {
        size_t chunk_end = std::min(i + chunk_size, end);
        
        futures.emplace_back(pool.Submit([&func, i, chunk_end]() {
            for (size_t j = i; j < chunk_end; ++j) {
                func(j);
            }
        }));
    }
    
    // Wait for all chunks to complete
    for (auto& future : futures) {
        future.wait();
    }
}

template<typename Func>
void ParallelFor::Execute(size_t start, size_t end, Func&& func, ThreadPool& pool) {
    Execute(start, end, std::forward<Func>(func), pool, 0);
}

template<typename Container, typename Func>
void ParallelFor::Execute(Container& container, Func&& func, ThreadPool& pool) {
    Execute(size_t(0), container.size(), 
           [&container, &func](size_t i) { func(container[i]); }, pool);
}

template<typename Container, typename Func>
void ParallelFor::Execute(const Container& container, Func&& func, ThreadPool& pool) {
    Execute(size_t(0), container.size(), 
           [&container, &func](size_t i) { func(container[i]); }, pool);
}

// Global thread pool instance
class GlobalThreadPool {
public:
    static ThreadPool& Instance();
    static void Initialize(size_t num_threads = std::thread::hardware_concurrency());
    static void Shutdown();
    
private:
    static std::unique_ptr<ThreadPool> s_instance;
    static std::once_flag s_init_flag;
};

// Convenience functions using global thread pool
template<typename F, typename... Args>
auto SubmitTask(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    return GlobalThreadPool::Instance().Submit(std::forward<F>(f), std::forward<Args>(args)...);
}

template<typename F, typename... Args>
void SubmitTaskDetached(F&& f, Args&&... args) {
    GlobalThreadPool::Instance().SubmitDetached(std::forward<F>(f), std::forward<Args>(args)...);
}

// RAII helper for temporarily changing thread pool size
class ThreadPoolScope {
public:
    explicit ThreadPoolScope(size_t num_threads);
    ~ThreadPoolScope();
    
private:
    bool m_should_restore;
};

} // namespace voxelvk