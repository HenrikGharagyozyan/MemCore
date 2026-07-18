#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"

#include <cstddef>
#include <cstdlib>   // std::free, posix_memalign (POSIX)
#include <algorithm> // std::max
#include <bit>       // std::has_single_bit
#include <cassert>

#if defined(_WIN32)
    #include <malloc.h> // _aligned_malloc / _aligned_free
#endif

namespace MemCore
{

    // The most basic allocator, which requests memory directly from the OS.
    // It acts as an upstream source for our faster, specialized allocators.
    class MallocUpstream
    {
    private:
        // Platform-matched aligned allocation.
        // The free path MUST mirror this: on Windows a pointer from
        // _aligned_malloc cannot be released with std::free (heap corruption),
        // so allocate() and deallocate() both route through this pair.
        static void* raw_aligned_alloc(std::size_t size, std::size_t alignment) noexcept
        {
#if defined(_WIN32)
            // NOTE: MSVC's argument order is (size, alignment) — reversed vs POSIX.
            return _aligned_malloc(size, alignment);
#else
            void* ptr = nullptr;
            // posix_memalign returns an errno-style code, not the pointer.
            if (::posix_memalign(&ptr, alignment, size) != 0)
                return nullptr;
            return ptr;
#endif
        }

        static void raw_aligned_free(void* ptr) noexcept
        {
#if defined(_WIN32)
            _aligned_free(ptr);
#else
            std::free(ptr);
#endif
        }

    public:
        // Allocate memory honoring the requested alignment.
        Block allocate(std::size_t size, std::size_t alignment) noexcept
        {
            if (size == 0)
                return { nullptr, 0 };

            // posix_memalign requires alignment to be a power of two AND a
            // multiple of sizeof(void*). Floor it to the platform minimum so
            // small requests (e.g. alignof(int) == 4) stay legal. Over-aligning
            // is always safe.
            alignment = std::max(alignment, alignof(std::max_align_t));
            assert(std::has_single_bit(alignment) && "Alignment must be a power of two");

            void* ptr = raw_aligned_alloc(size, alignment);
            if (!ptr)
                return { nullptr, 0 };

            return { ptr, size };
        }

        void deallocate(void* ptr, std::size_t /*size*/) noexcept
        {
            raw_aligned_free(ptr);
        }

        // A system allocator cannot free everything at once, so reset is a no-op.
        void reset() noexcept
        {
            // No-op
        }
    };

    // Strict check: if we break the AllocatorConcept interface somewhere,
    // this fails to compile.
    static_assert(Allocator<MallocUpstream>);

    // Design intent, enforced: MallocUpstream is a valid Allocator but NOT an
    // OwningAllocator. malloc returns scattered blocks with no contiguous range
    // to test a pointer against, so implementing owns() would require tracking
    // every allocation — overhead that defeats a thin OS passthrough. It may
    // therefore only serve as the *fallback* leg of a FallbackAllocator, never
    // the primary. This assert makes that contract impossible to regress silently.
    static_assert(!OwningAllocator<MallocUpstream>);

}
