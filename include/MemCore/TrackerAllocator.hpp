#pragma once
#include "AllocatorConcept.hpp"
#include "MemoryTags.hpp"

#include <cstddef>
#include <algorithm>
#include <iostream>
#include <array>
#include <atomic>
#include <iomanip>

namespace MemCore 
{
    template <typename UpstreamAllocator>
    class TrackerAllocator 
    {
    private:
        UpstreamAllocator* m_upstream;

        // Hidden header that stores the size and tag so deallocate knows what to subtract from
        struct alignas(16) Header 
        {
            std::size_t size;
            MemoryTag tag;
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

        Block allocate(std::size_t size, std::size_t alignment) 
        {
            // Increase the size by the header size
            std::size_t total_size = sizeof(Header) + size;
            std::size_t actual_align = (alignment > alignof(Header)) ? alignment : alignof(Header);

            Block block = m_upstream->allocate(total_size, actual_align);
            if (!block.ptr) 
                return { nullptr, 0 };

            // Write metadata before the user pointer
            Header* header = static_cast<Header*>(block.ptr);
            header->size = size;
            header->tag = g_current_tag; // Take the current thread tag from MemoryTags.hpp

            // Update metrics for the specific tag
            std::size_t tag_idx = static_cast<std::size_t>(g_current_tag);
            std::size_t current = m_allocated_per_tag[tag_idx].fetch_add(size) + size;

            // Atomically update the peak value
            std::size_t peak = m_peak_per_tag[tag_idx].load();
            while (current > peak && !m_peak_per_tag[tag_idx].compare_exchange_weak(peak, current));

            m_total_allocations.fetch_add(1, std::memory_order_relaxed);

            // Return a pointer to the data after the header
            std::byte* payload = reinterpret_cast<std::byte*>(block.ptr) + sizeof(Header);
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

            // Pass the original pointer to the upstream allocator
            std::size_t total_size = sizeof(Header) + header->size;
            m_upstream->deallocate(header, total_size);
        }

        [[nodiscard]] std::size_t get_allocated(MemoryTag tag) const noexcept 
        {
            return m_allocated_per_tag[static_cast<std::size_t>(tag)].load();
        }

        [[nodiscard]] std::size_t get_peak(MemoryTag tag) const noexcept 
        {
            return m_peak_per_tag[static_cast<std::size_t>(tag)].load();
        }

        bool owns(const void* ptr) const noexcept 
        {
            if (!ptr) 
                return false;

            const std::byte* payload = static_cast<const std::byte*>(ptr);
            const Header* header = reinterpret_cast<const Header*>(payload - sizeof(Header));
            return m_upstream->owns(header); // Check the original pointer
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