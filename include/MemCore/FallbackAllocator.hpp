#pragma once
#include "AllocatorConcept.hpp"

namespace MemCore 
{

    // The primary MUST be owning: deallocate() routes a pointer by asking the
    // primary whether it owns it. The fallback only needs to allocate/free, so a
    // non-owning source like MallocUpstream is valid there (and only there).
    template <OwningAllocator PrimaryAllocator, Allocator FallbackAllocatorT>
    class FallbackAllocator
    {
    private:
        PrimaryAllocator* m_primary;
        FallbackAllocatorT* m_fallback;

    public:
        FallbackAllocator(PrimaryAllocator& primary, FallbackAllocatorT& fallback) noexcept
            : m_primary(&primary), m_fallback(&fallback) 
        {
        }

        Block allocate(std::size_t size, std::size_t alignment) 
        {
            // 1. First, try to allocate memory from the fast primary allocator
            Block block = m_primary->allocate(size, alignment);
            
            // 2. If the primary allocator failed (returned nullptr), ask the fallback allocator
            if (!block.ptr) 
            {
                block = m_fallback->allocate(size, alignment);
            }
            
            return block;
        }

        void deallocate(void* ptr, std::size_t size) noexcept 
        {
            if (!ptr) 
                return;

            // Ask the primary allocator whether this pointer belongs to it
            if (m_primary->owns(ptr)) 
            {
                m_primary->deallocate(ptr, size);
            } 
            else 
            {
                // If not, then the fallback allocator allocated the memory
                m_fallback->deallocate(ptr, size);
            }
        }

        // Support for chains of fallback allocators. Only available when the
        // fallback leg is also owning (the primary always is, by constraint),
        // so this composite only models OwningAllocator when both legs do.
        bool owns(const void* ptr) const noexcept
            requires OwningAllocator<FallbackAllocatorT>
        {
            return m_primary->owns(ptr) || m_fallback->owns(ptr);
        }
        
    };

}