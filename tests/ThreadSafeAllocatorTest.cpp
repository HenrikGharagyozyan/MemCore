#include <gtest/gtest.h>

#include <MemCore/ThreadSafeAllocator.hpp>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>

#include <thread>
#include <vector>
#include <mutex>


TEST(ThreadSafeAllocatorTest, ConcurrentAllocationsFromPool) 
{
    MemCore::MallocUpstream system_malloc;
    
    // Allocate a buffer for 1000 chunks of 32 bytes each
    const std::size_t chunk_size = 32;
    const std::size_t num_chunks = 1000;
    MemCore::Block buffer = system_malloc.allocate(chunk_size * num_chunks, 8);
    
    {
        // Create a pool and wrap it in a ThreadSafeAllocator
        MemCore::PoolAllocator pool(buffer, chunk_size, 8);
        MemCore::ThreadSafeAllocator<MemCore::PoolAllocator, std::mutex> safe_pool(pool);

        const int num_threads = 10;
        const int allocs_per_thread = 50;
        std::vector<std::thread> threads;
        
        // A vector of vectors to store the pointers allocated by each thread
        std::vector<std::vector<MemCore::Block>> thread_allocs(num_threads);

        // 1. Allocate memory concurrently
        for (int i = 0; i < num_threads; ++i) 
        {
            threads.emplace_back([&safe_pool, &thread_allocs, i, allocs_per_thread]() 
                {
                    for (int j = 0; j < allocs_per_thread; ++j) 
                    {
                        MemCore::Block b = safe_pool.allocate(32, 8);
                        EXPECT_NE(b.ptr, nullptr); // The pool should not be exhausted
                        thread_allocs[i].push_back(b);
                    }
                });
        }

        // Wait for all threads to finish
        for (auto& t : threads) 
        {
            t.join();
        }
        threads.clear();

        // 2. Release memory concurrently
        for (int i = 0; i < num_threads; ++i) 
        {
            threads.emplace_back([&safe_pool, &thread_allocs, i]() 
                {
                    for (auto& b : thread_allocs[i]) 
                    {
                        safe_pool.deallocate(b.ptr, b.size);
                    }
                });
        }

        // Wait for completion
        for (auto& t : threads) 
        {
            t.join();
        }
        
        // If the test did not crash with a Segmentation Fault,
        // then the pool's internal linked list stayed intact thanks to the mutex!
    }

    system_malloc.deallocate(buffer.ptr, buffer.size);
}