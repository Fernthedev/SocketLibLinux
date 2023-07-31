#pragma once

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <vector>
#include <thread>
#include <atomic>

// ChatGPT generated code
/// Essentially, if a thread wants exclusive access
/// no other thread will manage to get shared access

class ExclusivePrioritySharedMutex
{
public:
    void lock_shared()
    {
        // Fast path if no exclusive lock is requested and none is taken
        if (exclusiveRequests_.load(std::memory_order_acquire) == 0 &&
            sharedCount_.fetch_add(1, std::memory_order_acquire) >= 0)
        {
            return;
        }

        // Slow path: exclusive lock is requested or taken, or contention for shared access
        std::unique_lock<std::mutex> lock(mutex_);
        sharedCv_.wait(lock, [this]() { return exclusiveRequests_.load(std::memory_order_acquire) == 0 && exclusiveCount_.load(std::memory_order_acquire) == 0; });
        sharedCount_.fetch_add(1, std::memory_order_acquire);
    }

    void unlock_shared()
    {
        // if previous value was 1, no more shared locks
        if (sharedCount_.fetch_sub(1, std::memory_order_release) == 1)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (exclusiveWaiting_.load(std::memory_order_acquire) > 0)
            {
                exclusiveCv_.notify_one();
            }
            else
            {
                sharedCv_.notify_one();
            }
//            exclusiveCv_.notify_one();
        }
    }

    void lock()
    {
        exclusiveRequests_.fetch_add(1, std::memory_order_acquire);

        std::unique_lock<std::mutex> lock(mutex_);
        exclusiveCv_.wait(lock, [this]() { return sharedCount_.load(std::memory_order_acquire) == 0 && exclusiveCount_.load(std::memory_order_acquire) == 0; });
        exclusiveRequests_.fetch_sub(1, std::memory_order_release);
        exclusiveCount_.fetch_add(1, std::memory_order_acquire);
    }

    void unlock()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        exclusiveCount_.fetch_sub(1, std::memory_order_release);
        if (exclusiveWaiting_.load(std::memory_order_acquire) > 0)
        {
            exclusiveCv_.notify_one();
        }
        else
        {
            sharedCv_.notify_one();
        }
    }

private:
    std::atomic_int sharedCount_ = 0;          // Count of threads holding shared access
    std::atomic_int exclusiveRequests_ = 0;    // Count of threads requesting exclusive access
    std::atomic_int exclusiveWaiting_ = 0;     // Count of threads waiting for exclusive access
    std::atomic_int exclusiveCount_ = 0;       // Count of threads holding exclusive access

    std::mutex mutex_;               // For coordinating access to internal variables
    std::condition_variable sharedCv_;  // For signaling shared access
    std::condition_variable exclusiveCv_; // For signaling exclusive access
};



//
//class ExclusivePrioritySharedMutex
//{
//public:
//    void lock_shared()
//    {
//        std::unique_lock<std::mutex> lock(sharedMutex_);
//        sharedCv_.wait(lock, [this]() { return writers_ == 0; });
//        // Wait until there are no write requests pending
//        ++readers_;
//    }
//
//    void unlock_shared()
//    {
//        std::unique_lock<std::mutex> lock(sharedMutex_);
//        --readers_;
//        if (readers_ == 0 && writers_ > 0)
//        {
//            writeCv_.notify_one();
//        }
//    }
//
//    void lock()
//    {
//        std::unique_lock<std::mutex> lock(exclusiveMutex_);
//        writers_++; // Mark that exclusive access is requested
//        writeCv_.wait(lock, [this]() { return readers_ == 0; });
//        // Wait until there are no readers to proceed with exclusive access
//    }
//
//    void unlock()
//    {
//        std::unique_lock<std::mutex> lock(exclusiveMutex_);
//        writers_--; // Mark that exclusive access is no longer requested
//
//        // Check if there are waiting exclusive access requests
//        if (writeCv_.wait_for(lock, std::chrono::seconds(0), [this]() { return writers_ > 0; }))
//        {
//            // Notify the next waiting thread that exclusive access is granted
//            writeCv_.notify_one();
//        }
//        else
//        {
//            // If no exclusive access requests, notify all waiting threads that shared access is possible
//            sharedCv_.notify_all();
//        }
//    }
//
//private:
//    std::mutex sharedMutex_; // For coordinating shared access
//    std::mutex exclusiveMutex_; // For coordinating exclusive access
//    std::condition_variable sharedCv_; // For signaling shared access
//    std::condition_variable writeCv_; // For signaling exclusive access
//    int writers_ = 0; // Flag indicating if exclusive access is requested
//    int readers_ = 0; // Count of threads holding shared access
//};


//class ExclusiveSharedMutex
//{
//public:
//
//    void lock_shared()
//    {
//        sharedMutex_.lock_shared(); // Acquire shared lock for multiple readers
//    }
//
//    void unlock_shared()
//    {
//        sharedMutex_.unlock_shared(); // Release shared lock
//    }
//
//    void lock()
//    {
//        std::unique_lock<std::mutex> lock(exclusiveMutex_);
//        while (writeLockRequested_ || readers_ > 0)
//        {
//            // Wait until exclusive access can be granted (no writers or readers)
//            writeCv_.wait(lock);
//        }
//        writeLockRequested_ = true; // Mark that exclusive access is requested
//    }
//
//    void unlock()
//    {
//        std::unique_lock<std::mutex> lock(exclusiveMutex_);
//        writeLockRequested_ = false; // Mark that exclusive access is no longer requested
//        lock.unlock(); // Unlock before notifying to prevent waking up and blocking ourselves
//        writeCv_.notify_one(); // Notify waiting threads that exclusive access is released
//    }
//
////    // Locks the mutex for shared (read) access.
////    void lock_shared()
////    {
////        sharedMutex_.lock_shared(); // Acquire shared lock for multiple readers
////        std::unique_lock<std::mutex> lock(exclusiveMutex_);
////        while (writeLockRequested_)
////        {
////            // Wait for exclusive access to be granted (while another thread holds it)
////            writeCv_.wait(lock);
////        }
////    }
////
////    // Unlocks the mutex from shared (read) access.
////    void unlock_shared()
////    {
////        sharedMutex_.unlock_shared(); // Release shared lock
////    }
////
////    // Locks the mutex for exclusive (write) access.
////    void lock()
////    {
////        std::unique_lock<std::mutex> lock(exclusiveMutex_);
////        writeLockRequested_ = true; // Mark that exclusive access is requested
////        writeCv_.wait(lock, [this]() { return readers_ == 0; });
////        // Wait until there are no readers to proceed with exclusive access
////        sharedMutex_.lock(); // Acquire exclusive access once shared access is released
////    }
////
////    // Unlocks the mutex from exclusive (write) access.
////    void unlock()
////    {
////        sharedMutex_.unlock(); // Release exclusive access
////        std::unique_lock<std::mutex> lock(exclusiveMutex_);
////        writeLockRequested_ = false; // Mark that exclusive access is no longer requested
////        if (readers_ == 0)
////        {
////            // Notify waiting threads that shared access is possible
////            writeCv_.notify_one();
////        }
////    }
//
//private:
//    std::shared_mutex sharedMutex_; // For shared read access (multiple threads allowed)
//    std::mutex exclusiveMutex_; // For coordinating exclusive access
//    std::condition_variable writeCv_; // For signaling exclusive access
//    bool writeLockRequested_ = false; // Flag indicating if exclusive access is requested
//    int readers_ = 0; // Count of threads holding shared access
//};
