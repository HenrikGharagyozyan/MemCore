#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"
#include <cstddef>
#include <cstdint>
#include <cassert>

namespace MemCore 
{

    class StackAllocator 
    {
    private:
        // Internal structure that lives invisibly before user memory
        struct Header 
        {
            std::size_t previous_offset;
        };

        Block m_memory;        // Backing store (raw memory from Upstream)
        std::size_t m_offset;  // Current cursor position

    public:
        using Marker = std::size_t;

        explicit StackAllocator(Block memory) noexcept 
            : m_memory(memory)
            , m_offset(0) 
        {
        }

        Block allocate(std::size_t size, std::size_t alignment) noexcept 
        {
            std::byte* base = static_cast<std::byte*>(m_memory.ptr);
            
            // 1. Calculate where user data can minimally begin.
            // It must start at least after the current offset plus the header size.
            std::byte* earliest_payload_ptr = base + m_offset + sizeof(Header);
            
            // 2. Align the user pointer to its requirements
            void* aligned_payload_ptr = AlignForward(earliest_payload_ptr, alignment);
            std::byte* payload_ptr = static_cast<std::byte*>(aligned_payload_ptr);
            
            // 3. The header must reside immediately before the aligned user pointer
            std::byte* header_ptr = payload_ptr - sizeof(Header);
            
            // 4. Check whether there is enough memory in the block
            std::size_t new_offset = (payload_ptr + size) - base;
            if (new_offset > m_memory.size) 
            {
                return { nullptr, 0 }; // Out of memory
            }

            // 5. Store information in the header.
            // Use placement new to construct the structure in raw memory.
            Header* header = ::new (static_cast<void*>(header_ptr)) Header();
            header->previous_offset = m_offset;

            // 6. Move the cursor to the new location
            m_offset = new_offset;

            return { aligned_payload_ptr, size };
        }

        // Fine-grained memory deallocation (must occur strictly in reverse order!)
        void deallocate(void* ptr, std::size_t size) noexcept 
        {
            if (!ptr) 
                return;

            std::byte* base = static_cast<std::byte*>(m_memory.ptr);
            std::byte* payload_ptr = static_cast<std::byte*>(ptr);

            // LIFO check: the block being freed must be the topmost block!
            // In other words, current m_offset should exactly equal the end of this block.
            assert((payload_ptr + size) - base == m_offset && "Out-of-order deallocation in StackAllocator! LIFO violated.");

            // Step back to read the header
            std::byte* header_ptr = payload_ptr - sizeof(Header);
            Header* header = reinterpret_cast<Header*>(header_ptr);

            // Simply roll the cursor back to its state before this allocation
            m_offset = header->previous_offset;
        }

        // Additional professional API: lets you create a marker
        // and roll back to it all at once, freeing a batch of objects.
        Marker get_marker() const noexcept 
        {
            return m_offset;
        }

        void free_to_marker(Marker marker) noexcept 
        {
            assert(marker <= m_offset && "Cannot rollback to a forward marker!");
            m_offset = marker;
        }

        void reset() noexcept 
        {
            m_offset = 0;
        }
    };

    static_assert(Allocator<StackAllocator>);

}