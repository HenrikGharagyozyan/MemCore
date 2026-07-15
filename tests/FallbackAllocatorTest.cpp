#include <gtest/gtest.h>

#include <MemCore/FallbackAllocator.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>


TEST(FallbackAllocatorTest, SwitchesToFallbackOnOOM) 
{
    MemCore::MallocUpstream system_malloc;
    
    // Create a tiny 128-byte buffer
    MemCore::Block chunk = system_malloc.allocate(128, 8);
    
    {
        MemCore::LinearAllocator primary(chunk);
        
        // Combine: Linear (fast) + Malloc (slower, but unlimited)
        MemCore::FallbackAllocator<MemCore::LinearAllocator, MemCore::MallocUpstream> fallback_alloc(primary, system_malloc);

        // 1. Request 100 bytes. It should fit in primary.
        MemCore::Block a = fallback_alloc.allocate(100, 8);
        EXPECT_NE(a.ptr, nullptr);
        EXPECT_TRUE(primary.owns(a.ptr)); // Verify that the memory came from the Linear allocator

        // 2. Request another 50 bytes. Only 28 bytes remain in primary!
        // It will return nullptr, and the FallbackAllocator should intercept the call
        // and forward it to system_malloc.
        MemCore::Block b = fallback_alloc.allocate(50, 8);
        EXPECT_NE(b.ptr, nullptr);
        EXPECT_FALSE(primary.owns(b.ptr)); // Verify that this is NO LONGER from the Linear allocator

        // Release the memory (FallbackAllocator will decide where to route deallocate)
        fallback_alloc.deallocate(a.ptr, a.size);
        fallback_alloc.deallocate(b.ptr, b.size);
    }

    system_malloc.deallocate(chunk.ptr, chunk.size);
}