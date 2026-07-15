#pragma once
#include "AllocatorConcept.hpp"
#include <mutex>
#include <utility>

namespace MemCore 
{

    /**
     * @brief A wrapper that makes any allocator thread-safe.
     * 
     * @tparam Allocator The wrapped allocator type.
     * @tparam Mutex The mutex type (std::mutex by default).
     *               A SpinLock can be passed for higher performance.
     */
    template <typename Allocator, typename Mutex = std::mutex>
    class ThreadSafeAllocator 
    {
    private:
        Allocator& m_allocator; // Reference to the underlying allocator
        mutable Mutex m_mutex;  // Mutex protecting access (mutable so it can be locked in const methods)

    public:
        // Constructor takes a reference to an existing allocator
        explicit ThreadSafeAllocator(Allocator& allocator) noexcept
            : m_allocator(allocator) 
        {
        }

        // Disable copying so the mutex and references are not duplicated
        ThreadSafeAllocator(const ThreadSafeAllocator&) = delete;
        ThreadSafeAllocator& operator=(const ThreadSafeAllocator&) = delete;

        /**
         * @brief Allocates memory while holding the mutex.
         */
        Block allocate(std::size_t size, std::size_t alignment) 
        {
            std::lock_guard<Mutex> lock(m_mutex);
            return m_allocator.allocate(size, alignment);
        }

        /**
         * @brief Deallocates memory while holding the mutex.
         */
        void deallocate(void* ptr, std::size_t size) 
        {
            std::lock_guard<Mutex> lock(m_mutex);
            m_allocator.deallocate(ptr, size);
        }

        /**
         * @brief Delegates ownership checks if the underlying allocator supports them.
         */
        bool owns(const void* ptr) const 
        {
            std::lock_guard<Mutex> lock(m_mutex);
            // If the underlying allocator has an owns method, call it.
            // We use constexpr if (C++17) to check for the method at compile time
            // (although in our architecture it is now present for all allocators).
            return m_allocator.owns(ptr);
        }
        
        /**
         * @brief Delegates allocator reset if supported.
         */
        void reset() 
        {
            std::lock_guard<Mutex> lock(m_mutex);
            // reset() exists for Stack, Linear, and Arena, but not for Malloc/Pool in the current design.
            // Keep the call direct, assuming use with arenas or stacks.
            m_allocator.reset();
        }
    };

} // namespace MemCore