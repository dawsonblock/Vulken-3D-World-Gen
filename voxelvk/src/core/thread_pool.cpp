#include "thread_pool.hpp"
#include "logger.hpp"
#include <algorithm>

namespace voxelvk {

// ThreadPool implementation
ThreadPool::ThreadPool(size_t num_threads) {
    // Ensure we have at least one thread
    num_threads = std::max(size_t(1), num_threads);
    
    VXL_INFO("Creating ThreadPool with {} threads", num_threads);
    
    m_threads.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        m_threads.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    Shutdown();
}

void ThreadPool::WorkerThread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            
            // Wait for a task or shutdown signal
            m_condition.wait(lock, [this] {
                return !m_tasks.empty() || m_shutdown.load();
            });
            
            // If shutting down and no tasks remain, exit
            if (m_shutdown.load() && m_tasks.empty()) {
                break;
            }
            
            // Get the next task
            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
                m_active_tasks.fetch_add(1);
            }
        }
        
        // Execute the task outside the lock
        if (task) {
            try {
                task();
            }
            catch (const std::exception& e) {
                VXL_ERROR("Task execution failed: {}", e.what());
            }
            catch (...) {
                VXL_ERROR("Task execution failed with unknown exception");
            }
            
            m_active_tasks.fetch_sub(1);
        }
    }
}

void ThreadPool::WaitForAll() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            if (m_tasks.empty() && m_active_tasks.load() == 0) {
                break;
            }
        }
        
        // Small sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

size_t ThreadPool::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    return m_tasks.size();
}

void ThreadPool::Shutdown() {
    if (m_shutdown.load()) {
        return; // Already shutting down
    }
    
    VXL_INFO("Shutting down ThreadPool...");
    
    // Signal shutdown
    m_shutdown.store(true);
    
    // Wake up all threads
    m_condition.notify_all();
    
    // Wait for all threads to finish
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // Clear any remaining tasks
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        std::queue<std::function<void()>> empty;
        m_tasks.swap(empty);
    }
    
    VXL_INFO("ThreadPool shutdown complete");
}

// GlobalThreadPool implementation
std::unique_ptr<ThreadPool> GlobalThreadPool::s_instance = nullptr;
std::once_flag GlobalThreadPool::s_init_flag;

ThreadPool& GlobalThreadPool::Instance() {
    std::call_once(s_init_flag, []() {
        Initialize();
    });
    return *s_instance;
}

void GlobalThreadPool::Initialize(size_t num_threads) {
    if (s_instance) {
        VXL_WARN("GlobalThreadPool already initialized");
        return;
    }
    
    s_instance = std::make_unique<ThreadPool>(num_threads);
    VXL_INFO("GlobalThreadPool initialized with {} threads", num_threads);
}

void GlobalThreadPool::Shutdown() {
    if (s_instance) {
        s_instance->Shutdown();
        s_instance.reset();
        VXL_INFO("GlobalThreadPool shutdown");
    }
}

// ThreadPoolScope implementation
ThreadPoolScope::ThreadPoolScope(size_t num_threads) : m_should_restore(false) {
    // For this simple implementation, we just log the change
    // In a more sophisticated version, you might dynamically resize the pool
    VXL_DEBUG("ThreadPoolScope: Requesting {} threads", num_threads);
    
    // TODO: Implement dynamic thread pool resizing if needed
    // For now, this is just a placeholder that ensures the pool is initialized
    GlobalThreadPool::Instance();
}

ThreadPoolScope::~ThreadPoolScope() {
    if (m_should_restore) {
        VXL_DEBUG("ThreadPoolScope: Restoring original thread count");
        // TODO: Restore original thread count if dynamic resizing is implemented
    }
}

} // namespace voxelvk