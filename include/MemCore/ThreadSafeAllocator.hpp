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
    template <Allocator Alloc, typename Mutex = std::mutex>
    class ThreadSafeAllocator
    {
    private:
        Alloc& m_allocator;    // Reference to the underlying allocator
        mutable Mutex m_mutex; // Mutex protecting access (mutable so it can be locked in const methods)

    public:
        // Constructor takes a reference to an existing allocator
        explicit ThreadSafeAllocator(Alloc& allocator) noexcept
            : m_allocator(allocator) 
        {
        }

        // Disable copying so the mutex and references are not duplicated
        ThreadSafeAllocator(const ThreadSafeAllocator&) = delete;
        ThreadSafeAllocator& operator=(const ThreadSafeAllocator&) = delete;

        // NOTE on noexcept: locking is the only operation here that can fail,
        // and a mutex error (std::system_error) is unrecoverable for an
        // allocator -- there is no sensible way to report it through this API.
        // Declaring these noexcept turns such a failure into a terminate, and
        // is what lets ThreadSafeAllocator model the Allocator concept at all.
        // Without it the decorator cannot be used with StlAdapter, as an Arena
        // upstream, or inside any other decorator.

        /**
         * @brief Allocates memory while holding the mutex.
         */
        [[nodiscard]] Block allocate(std::size_t size, std::size_t alignment) noexcept
        {
            std::lock_guard<Mutex> lock(m_mutex);
            return m_allocator.allocate(size, alignment);
        }

        /**
         * @brief Deallocates memory while holding the mutex.
         */
        void deallocate(void* ptr, std::size_t size) noexcept
        {
            std::lock_guard<Mutex> lock(m_mutex);
            m_allocator.deallocate(ptr, size);
        }

        /**
         * @brief Delegates ownership checks if the underlying allocator supports them.
         */
        bool owns(const void* ptr) const noexcept
            requires OwningAllocator<Alloc>
        {
            std::lock_guard<Mutex> lock(m_mutex);
            return m_allocator.owns(ptr);
        }

        /**
         * @brief Delegates allocator reset if the wrapped allocator supports it.
         */
        void reset() noexcept
            requires ResettableAllocator<Alloc>
        {
            std::lock_guard<Mutex> lock(m_mutex);
            m_allocator.reset();
        }
    };

} // namespace MemCore