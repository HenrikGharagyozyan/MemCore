#pragma once
#include "AllocatorConcept.hpp"
#include "MemoryTags.hpp"
#include "Align.hpp"

#include <cstddef>
#include <algorithm>
#include <iostream>
#include <array>
#include <atomic>
#include <iomanip>

namespace MemCore 
{
    template <Allocator UpstreamAllocator>
    class TrackerAllocator
    {
    private:
        UpstreamAllocator* m_upstream;

        // Hidden header that stores the size and tag so deallocate knows what to
        // subtract from. The payload is aligned by shifting it forward from the
        // upstream base, so the header no longer needs an over-alignment of its
        // own; base_offset lets deallocate recover the original upstream pointer.
        struct Header
        {
            std::size_t size;
            MemoryTag tag;
            std::size_t base_offset; // bytes from the upstream base ptr to the payload
        };

        // Atomic arrays for tracking statistics separately by tag
        mutable std::array<std::atomic<std::size_t>, static_cast<std::size_t>(MemoryTag::Count)> m_allocated_per_tag{};
        mutable std::array<std::atomic<std::size_t>, static_cast<std::size_t>(MemoryTag::Count)> m_peak_per_tag{};
        
        std::atomic<std::size_t> m_total_allocations{0};
        std::atomic<std::size_t> m_total_deallocations{0};

    public:
        explicit TrackerAllocator(UpstreamAllocator& upstream) noexcept
            : m_upstream(&upstream) 
        {
        }

        ~TrackerAllocator() 
        {
            // Uncomment to print statistics when the tracker is destroyed
            // print_stats(); 
        }

        [[nodiscard]] Block allocate(std::size_t size, std::size_t alignment) noexcept
        {
            // Zero-size requests yield no object.
            if (size == 0)
                return { nullptr, 0 };

            // The upstream base must be aligned for the Header AND the caller's
            // request; the payload is then aligned by shifting forward.
            std::size_t actual_align = std::max(alignment, alignof(Header));

            // Reserve a whole number of aligned slots for the header, so
            // base + header_space is guaranteed to stay aligned.
            std::size_t header_space = AlignUp(sizeof(Header), actual_align);
            std::size_t total_size = header_space + size;

            Block block = m_upstream->allocate(total_size, actual_align);
            if (!block.ptr)
                return { nullptr, 0 };

            std::byte* base = static_cast<std::byte*>(block.ptr);
            std::byte* payload = base + header_space; // aligned to actual_align

            // Write metadata directly before the (aligned) payload
            Header* header = reinterpret_cast<Header*>(payload - sizeof(Header));
            header->size = size;
            header->tag = g_current_tag; // Take the current thread tag from MemoryTags.hpp
            header->base_offset = header_space;

            // Update metrics for the specific tag
            std::size_t tag_idx = static_cast<std::size_t>(g_current_tag);
            std::size_t current = m_allocated_per_tag[tag_idx].fetch_add(size) + size;

            // Atomically update the peak value
            std::size_t peak = m_peak_per_tag[tag_idx].load();
            while (current > peak && !m_peak_per_tag[tag_idx].compare_exchange_weak(peak, current));

            m_total_allocations.fetch_add(1, std::memory_order_relaxed);

            return { payload, size };
        }

        void deallocate(void* ptr, std::size_t /*size*/) noexcept 
        {
            if (!ptr) 
                return;

            // Step back to read the header
            std::byte* payload = static_cast<std::byte*>(ptr);
            Header* header = reinterpret_cast<Header*>(payload - sizeof(Header));

            // Read the tag and subtract the size from the appropriate category
            std::size_t tag_idx = static_cast<std::size_t>(header->tag);
            m_allocated_per_tag[tag_idx].fetch_sub(header->size);

            m_total_deallocations.fetch_add(1, std::memory_order_relaxed);

            // Recover the original upstream pointer via the stored offset
            std::byte* base = payload - header->base_offset;
            std::size_t total_size = header->base_offset + header->size;
            m_upstream->deallocate(base, total_size);
        }

        [[nodiscard]] std::size_t get_allocated(MemoryTag tag) const noexcept 
        {
            return m_allocated_per_tag[static_cast<std::size_t>(tag)].load();
        }

        [[nodiscard]] std::size_t get_peak(MemoryTag tag) const noexcept 
        {
            return m_peak_per_tag[static_cast<std::size_t>(tag)].load();
        }

        // Only exposed when the wrapped layer can answer ownership, so the
        // tracker does not falsely advertise OwningAllocator over a non-owning
        // upstream (e.g. MallocUpstream).
        bool owns(const void* ptr) const noexcept
            requires OwningAllocator<UpstreamAllocator>
        {
            if (!ptr)
                return false;

            const std::byte* payload = static_cast<const std::byte*>(ptr);
            const Header* header = reinterpret_cast<const Header*>(payload - sizeof(Header));
            const std::byte* base = payload - header->base_offset;
            return m_upstream->owns(base); // Check the original upstream pointer
        }

        // Updated professional statistics with categories
        void print_stats() const 
        {
            std::cout << "\n======================================\n";
            std::cout << " [MemCore] Memory Tracker Statistics\n";
            std::cout << "======================================\n";
            
            std::size_t total_current = 0;

            // Iterate over all tags and print statistics for active ones
            for (std::size_t i = 0; i < static_cast<std::size_t>(MemoryTag::Count); ++i) 
            {
                MemoryTag tag = static_cast<MemoryTag>(i);
                std::size_t current = m_allocated_per_tag[i].load();
                std::size_t peak = m_peak_per_tag[i].load();
                
                total_current += current;

                if (peak > 0) // Print only categories that were allocated
                { 
                    std::cout << " [" << ToString(tag) << "]\n";
                    std::cout << "   Current : " << current << " bytes\n";
                    std::cout << "   Peak    : " << peak << " bytes\n";
                }
            }

            std::cout << "--------------------------------------\n";
            std::cout << " Total Allocations   : " << m_total_allocations.load() << "\n";
            std::cout << " Total Deallocations : " << m_total_deallocations.load() << "\n";
            std::cout << " Total Current Mem   : " << total_current << " bytes\n";
            std::cout << "--------------------------------------\n";
            
            if (total_current > 0) 
            {
                std::cout << " [!] WARNING: " << total_current << " bytes leaked!\n";
            } 
            else 
            {
                std::cout << " [OK] No memory leaks detected.\n";
            }
            std::cout << "======================================\n\n";
        }
    };

}