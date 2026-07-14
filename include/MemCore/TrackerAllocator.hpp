#pragma once
#include "AllocatorConcept.hpp"

#include <cstddef>
#include <algorithm>
#include <iostream>

namespace MemCore 
{

    // Template accepts ANY allocator (Linear, Stack, Pool, Arena)
    template <typename UpstreamAllocator>
    class TrackerAllocator 
    {
    private:
        UpstreamAllocator* m_upstream;
        std::size_t m_current_allocated;
        std::size_t m_peak_allocated;
        std::size_t m_total_allocations;
        std::size_t m_total_deallocations;

    public:
        explicit TrackerAllocator(UpstreamAllocator& upstream) noexcept
            : m_upstream(&upstream)
            , m_current_allocated(0)
            , m_peak_allocated(0)
            , m_total_allocations(0)
            , m_total_deallocations(0) 
        {
        }

        ~TrackerAllocator() 
        {
            // Ideally, we could assert(m_current_allocated == 0);
            // so the program fails on exit if there are leaks.
        }

        Block allocate(std::size_t size, std::size_t alignment) 
        {
            // Proxy the call to the real allocator
            Block block = m_upstream->allocate(size, alignment);
            
            // If allocation succeeds, collect metrics
            if (block.ptr) 
            {
                m_current_allocated += size;
                if (m_current_allocated > m_peak_allocated) 
                {
                    m_peak_allocated = m_current_allocated;
                }
                m_total_allocations++;
            }
            return block;
        }

        void deallocate(void* ptr, std::size_t size) noexcept 
        {
            if (!ptr) 
                return;
            
            // Proxy the deallocation
            m_upstream->deallocate(ptr, size);
            
            // Protect against underflow if someone frees more than allocated
            if (m_current_allocated >= size) 
            {
                m_current_allocated -= size;
            }
            m_total_deallocations++;
        }

        // Getters for collecting metrics (for example, for plotting graphs in a game)
        std::size_t get_current_usage() const noexcept { return m_current_allocated; }
        std::size_t get_peak_usage() const noexcept { return m_peak_allocated; }
        std::size_t get_allocation_count() const noexcept { return m_total_allocations; }
        std::size_t get_deallocation_count() const noexcept { return m_total_deallocations; }

        // Professional console visualization of statistics
        void print_stats() const 
        {
            std::cout << "\n======================================\n";
            std::cout << " [MemCore] Memory Tracker Statistics\n";
            std::cout << "======================================\n";
            std::cout << " Current Usage : " << m_current_allocated << " bytes\n";
            std::cout << " Peak Usage    : " << m_peak_allocated << " bytes\n";
            std::cout << " Allocations   : " << m_total_allocations << "\n";
            std::cout << " Deallocations : " << m_total_deallocations << "\n";
            std::cout << "--------------------------------------\n";
            
            if (m_current_allocated > 0) 
            {
                std::cout << " [!] WARNING: " << m_current_allocated << " bytes leaked!\n";
            } 
            else 
            {
                std::cout << " [OK] No memory leaks detected.\n";
            }
            std::cout << "======================================\n\n";
        }
    };

}