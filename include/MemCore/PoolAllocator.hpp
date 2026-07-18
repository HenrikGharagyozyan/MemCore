#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"
#include <cstddef>
#include <cassert>
#include <algorithm>

namespace MemCore 
{

    class PoolAllocator 
    {
    private:
        // Internal node stored inside FREE memory blocks.
        // Once a block is given to the user, this structure is overwritten by user data.
        struct FreeNode 
        {
            FreeNode* next;
        };

        Block m_memory;
        std::size_t m_chunk_size;
        std::size_t m_alignment;
        FreeNode* m_free_list;

        // Internal function to split raw memory into equal chunks
        void initialize_free_list() noexcept 
        {
            m_free_list = nullptr;

            std::byte* base = static_cast<std::byte*>(m_memory.ptr);
            std::size_t memory_size = m_memory.size;

            // Chunks hold an intrusive FreeNode while free, so every chunk must
            // be aligned for BOTH the caller's requirement and FreeNode itself.
            // If the user asks for less than alignof(FreeNode) (e.g. 4 for a
            // struct of floats), threading the free list through a 4-aligned
            // chunk would be a misaligned pointer write (UB). Widen internally.
            const std::size_t effective_align = std::max(m_alignment, alignof(FreeNode));

            // 1. Align the start address
            void* aligned_base = AlignForward(base, effective_align);
            std::byte* current = static_cast<std::byte*>(aligned_base);

            // Count how many bytes were lost due to initial alignment
            std::size_t shift = current - base;
            if (shift >= memory_size)
                return;

            std::size_t available_size = memory_size - shift;

            // 2. Chunk size must be at least as large as a FreeNode pointer!
            std::size_t actual_chunk_size = m_chunk_size;
            if (actual_chunk_size < sizeof(FreeNode))
            {
                actual_chunk_size = sizeof(FreeNode);
            }

            // 3. Chunk size must be a multiple of the effective alignment so
            // EVERY chunk (not just the first) stays aligned for the FreeNode.
            actual_chunk_size = AlignUp(actual_chunk_size, effective_align);

            // 4. Count how many chunks fit in the available memory
            std::size_t num_chunks = available_size / actual_chunk_size;
            if (num_chunks == 0) 
                return;

            // 5. Slice the memory and build a free list from the chunks
            // Iterate from the end so the first chunk in memory becomes the first head of the list
            for (std::size_t i = num_chunks; i > 0; --i) 
            {
                std::byte* chunk_ptr = current + ((i - 1) * actual_chunk_size);
                FreeNode* node = reinterpret_cast<FreeNode*>(chunk_ptr);
                
                // Add the current node to the front of the list
                node->next = m_free_list;
                m_free_list = node;
            }
        }

    public:
        // The pool takes a raw memory chunk, an item size, and required alignment
        PoolAllocator(Block memory, std::size_t chunk_size, std::size_t alignment) noexcept
            : m_memory(memory), m_chunk_size(chunk_size), m_alignment(alignment), m_free_list(nullptr) 
        {
            // Alignment must be a power of two (checked with bit operations)
            assert(alignment != 0 && (alignment & (alignment - 1)) == 0);
            initialize_free_list();
        }

        Block allocate(std::size_t size, std::size_t alignment) noexcept 
        {
            // The pool is strict: it can only return its fixed size and alignment
            if (size > m_chunk_size || alignment > m_alignment || !m_free_list) 
            {
                return { nullptr, 0 };
            }

            // Take the first free block (O(1))
            FreeNode* node = m_free_list;
            m_free_list = m_free_list->next;

            return { static_cast<void*>(node), m_chunk_size };
        }

        // Unlike Linear and Arena, the pool can free blocks in any order!
        void deallocate(void* ptr, std::size_t /*size*/) noexcept 
        {
            if (!ptr) 
                return;

            // Simply return the block to the free list (O(1))
            FreeNode* node = static_cast<FreeNode*>(ptr);
            node->next = m_free_list;
            m_free_list = node;
        }

        // Fast reset of all memory at once
        void reset() noexcept 
        {
            initialize_free_list();
        }

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

    static_assert(Allocator<PoolAllocator>);

}