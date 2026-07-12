#pragma once
#include "AllocatorConcept.hpp"
#include <cstdlib> // For std::aligned_alloc and std::free
#include <new>     // For std::bad_alloc (if we want to throw it, but we return nullptr)

namespace MemCore 
{

    // The most basic allocator, which requests memory directly from the OS.
    // It acts as an upstream source for our future fast allocators.
    class MallocUpstream 
    {
    public:
        // Allocate memory while taking alignment into account
        Block allocate(std::size_t size, std::size_t alignment) noexcept 
        {
            // For std::aligned_alloc, the size must be a multiple of the alignment.
            // Round the size up.
            std::size_t remainder = size % alignment;
            std::size_t aligned_size = (remainder == 0) ? size : (size + alignment - remainder);

            void* ptr = std::aligned_alloc(alignment, aligned_size);
            
            return { ptr, aligned_size };
        }

        void deallocate(void* ptr, std::size_t /*size*/) noexcept 
        {
            std::free(ptr);
        }

        // For a system allocator, it is not possible to free all memory at once, so this method is empty.
        void reset() noexcept 
        {
            // No-op
        }
    };

    // Strict check: if we break the AllocatorConcept interface somewhere,
    static_assert(Allocator<MallocUpstream>);

}