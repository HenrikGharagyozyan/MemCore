#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"
#include <cstddef>
#include <algorithm>
#include <new>

namespace MemCore 
{

    template <typename Upstream>
    class ArenaAllocator 
    {
    private:
        // Intrusive list node stored at the beginning of each large block
        struct BlockNode 
        {
            BlockNode* next;
            std::size_t capacity;
            std::size_t offset;
        };
         
        Upstream& m_upstream;             // Raw memory source
        std::size_t m_default_block_size; // Default block size (for example, 4 KB)
        BlockNode* m_head;                // Pointer to the current active block

        // Internal helper to request a new block from Upstream
        BlockNode* allocate_new_block(std::size_t min_size) noexcept 
        {
            // We need to fit the object size plus our node header
            std::size_t required_size = min_size + sizeof(BlockNode);
            std::size_t size_to_alloc = std::max(required_size, m_default_block_size);

            // Request memory from Upstream aligned for our header
            Block upstream_block = m_upstream.allocate(size_to_alloc, alignof(BlockNode));
            if (!upstream_block.ptr) 
            {
                return nullptr;
            }

            // Place the node header at the start of the allocated memory (placement new)
            BlockNode* node = ::new (upstream_block.ptr) BlockNode{
                .next = m_head,
                .capacity = upstream_block.size,
                .offset = sizeof(BlockNode) // Occupied by the header itself
            };

            // Make this block the current active one
            m_head = node;
            return node;
        }

    public:
        explicit ArenaAllocator(Upstream& upstream, std::size_t default_block_size = 4096) noexcept
            : m_upstream(upstream)
            , m_default_block_size(default_block_size)
            , m_head(nullptr) 
        {
        }

        // When the arena is destroyed, return all blocks back to Upstream
        ~ArenaAllocator() noexcept 
        {
            reset();
        }

        // Disable copying so we don't accidentally duplicate pointers to the same blocks
        ArenaAllocator(const ArenaAllocator&) = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        // Allow moving
        ArenaAllocator(ArenaAllocator&& other) noexcept
            : m_upstream(other.m_upstream), 
              m_default_block_size(other.m_default_block_size), 
              m_head(other.m_head) 
        {
            other.m_head = nullptr;
        }

        Block allocate(std::size_t size, std::size_t alignment) noexcept 
        {
            if (size == 0) 
                return { nullptr, 0 };

            // If there are no blocks yet, create the first one
            if (!m_head) 
            {
                if (!allocate_new_block(size)) 
                    return { nullptr, 0 };
            }

            BlockNode* current = m_head;
            std::byte* base = reinterpret_cast<std::byte*>(current);
            std::byte* current_ptr = base + current->offset;

            // Compute alignment for the user pointer
            void* aligned_ptr = AlignForward(current_ptr, alignment);
            std::byte* payload_ptr = static_cast<std::byte*>(aligned_ptr);
            std::size_t shift = payload_ptr - current_ptr;

            // If current block lacks space, allocate a new block
            if (current->offset + shift + size > current->capacity) 
            {
                current = allocate_new_block(size);
                if (!current) 
                    return { nullptr, 0 };

                // Recompute pointers for the new block
                base = reinterpret_cast<std::byte*>(current);
                current_ptr = base + current->offset;
                aligned_ptr = AlignForward(current_ptr, alignment);
                payload_ptr = static_cast<std::byte*>(aligned_ptr);
                shift = payload_ptr - current_ptr;
            }

            // Move the current block cursor forward
            current->offset = (payload_ptr + size) - base;
            return { aligned_ptr, size };
        }

        // Like the linear allocator, the arena ignores fine-grained deallocation
        void deallocate(void* /*ptr*/, std::size_t /*size*/) noexcept {}

        // Full reset: traverse the intrusive list and free all blocks through Upstream
        void reset() noexcept 
        {
            BlockNode* current = m_head;
            while (current) 
            {
                BlockNode* next = current->next;
                
                void* ptr = static_cast<void*>(current);
                std::size_t cap = current->capacity;
                
                m_upstream.deallocate(ptr, cap);
                current = next;
            }
            m_head = nullptr;
        }

        // Проверяет, принадлежит ли указатель любому из блоков арены
        bool owns(const void* ptr) const noexcept 
        {
            if (!ptr) 
                return false;

            auto p = reinterpret_cast<std::uintptr_t>(ptr);

            // Проходим по всем выделенным чанкам через связный список
            BlockNode* current = m_head;
            while (current) 
            {
                auto start = reinterpret_cast<std::uintptr_t>(current);
                auto end = start + current->capacity;
                
                if (p >= start && p < end) 
                {
                    return true;
                }
                current = current->next;
            }
            
            return false;
        }
        
    };

}