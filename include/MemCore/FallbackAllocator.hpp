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
            // 1. Сначала пытаемся выделить память из быстрого основного аллокатора
            Block block = m_primary->allocate(size, alignment);
            
            // 2. Если основной не справился (вернул nullptr), просим у запасного
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

            // Спрашиваем основной аллокатор: "Это твой указатель?"
            if (m_primary->owns(ptr)) 
            {
                m_primary->deallocate(ptr, size);
            } 
            else 
            {
                // Если не его, значит память выделял запасной
                m_fallback->deallocate(ptr, size);
            }
        }

        // Поддержка цепочек Fallback-аллокаторов
        bool owns(const void* ptr) const noexcept 
        {
            return m_primary->owns(ptr) || m_fallback->owns(ptr);
        }
        
    };

}