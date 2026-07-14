#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"
#include <cstddef> // For std::byte

namespace MemCore 
{

    class LinearAllocator 
    {
    private:
        Block m_memory;        // The biggest memory chunk from Upstream
        std::size_t m_offset;  // Our "cursor" (how many bytes are already used)

    public:
        // The allocator does not allocate memory itself; it takes an existing chunk from outside
        explicit LinearAllocator(Block memory) noexcept 
            : m_memory(memory)
            , m_offset(0) 
        {
        }

        Block allocate(std::size_t size, std::size_t alignment) noexcept 
        {
            // std::byte* lets us do byte-wise pointer arithmetic
            std::byte* base = static_cast<std::byte*>(m_memory.ptr);
            std::byte* current_ptr = base + m_offset;
            
            // Align the current address
            void* aligned_ptr = AlignForward(current_ptr, alignment);
            
            // Count how many "empty" bytes were skipped for alignment
            std::size_t shift = static_cast<std::byte*>(aligned_ptr) - current_ptr;
            
            // Check whether we've run past the end of our big memory chunk
            if (m_offset + shift + size > m_memory.size) 
            {
                return { nullptr, 0 }; // Memory is exhausted
            }
            
            // Move the cursor forward
            m_offset += shift + size;
            
            return { aligned_ptr, size };
        }

        // A linear allocator ignores fine-grained deallocation.
        // This is not a bug; it is a feature — that is exactly why it works in O(1) CPU cycles.
        void deallocate(void* /*ptr*/, std::size_t /*size*/) noexcept {}

        // Release all memory at once
        void reset() noexcept 
        {
            m_offset = 0;
        }

        // Проверяет, принадлежит ли указатель этому аллокатору
        bool owns(const void* ptr) const noexcept 
        {
            if (!ptr) 
                return false;
                
            auto p = reinterpret_cast<std::uintptr_t>(ptr);
            auto start = reinterpret_cast<std::uintptr_t>(m_memory.ptr);
            auto end = start + m_memory.size;
            return p >= start && p < end;
        }

    };

    // Ensure that our class matches the contract
    static_assert(Allocator<LinearAllocator>);

}