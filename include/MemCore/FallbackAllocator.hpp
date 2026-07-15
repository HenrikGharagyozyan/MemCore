#pragma once
#include "AllocatorConcept.hpp"

namespace MemCore 
{

    template <typename PrimaryAllocator, typename FallbackAllocatorT>
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

        // Support for chains of fallback allocators
        bool owns(const void* ptr) const noexcept 
        {
            return m_primary->owns(ptr) || m_fallback->owns(ptr);
        }
        
    };

}