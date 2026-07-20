#pragma once
#include "AllocatorConcept.hpp"
#include <cstddef>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
    #define MEMCORE_OS_WINDOWS
#else
    #define MEMCORE_OS_LINUX
#endif

namespace MemCore 
{
    class VirtualUpstream 
    {
    private:
        std::size_t m_page_size;

        // Helper method to round the size up to the page size
        [[nodiscard]] std::size_t align_to_page(std::size_t size) const noexcept 
        {
            return (size + m_page_size - 1) & ~(m_page_size - 1);
        }

        // Internal method to query the OS page size
        static std::size_t query_page_size() noexcept;

    public:
        VirtualUpstream() noexcept;
        ~VirtualUpstream() = default;

        // Disable copying because this is a system resource
        VirtualUpstream(const VirtualUpstream&) = delete;
        VirtualUpstream& operator=(const VirtualUpstream&) = delete;
        VirtualUpstream(VirtualUpstream&&) noexcept = default;
        VirtualUpstream& operator=(VirtualUpstream&&) noexcept = default;

        [[nodiscard]] Block allocate(std::size_t size, std::size_t alignment) noexcept;
        void deallocate(void* ptr, std::size_t size) noexcept;

        // NOTE: VirtualUpstream deliberately provides NO owns().
        //
        // Each allocation is an independent OS mapping, so there is no
        // contiguous range to test a pointer against; answering correctly would
        // require tracking every mapping in a side table -- overhead that
        // defeats a thin OS passthrough. The previous implementation returned
        // `ptr != nullptr`, which claimed ownership of *every* non-null pointer
        // and would have made FallbackAllocator route every deallocation here.
        //
        // Like MallocUpstream, it therefore models Allocator but not
        // OwningAllocator, and may only be a FallbackAllocator's fallback leg.

        [[nodiscard]] std::size_t get_page_size() const noexcept { return m_page_size; }
    };

    static_assert(Allocator<VirtualUpstream>);
    static_assert(!OwningAllocator<VirtualUpstream>);
}