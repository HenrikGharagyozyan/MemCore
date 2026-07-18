#include <gtest/gtest.h>

#include <MemCore/LinearAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/Align.hpp>

class LinearAllocatorTest : public ::testing::Test 
{
protected:
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk;

    void SetUp() override
    {
        // Request a 16-aligned base so sequential-packing offsets are
        // deterministic across platforms. alignof(std::max_align_t) is 16 on
        // Linux but 8 on MSVC, so an 8-aligned request could yield a base that
        // is not 16-aligned, shifting a later 16-aligned allocation.
        chunk = upstream.allocate(1024, 16);
    }

    void TearDown() override 
    {
        upstream.deallocate(chunk.ptr, chunk.size);
    }
};

TEST_F(LinearAllocatorTest, SequentialAllocations) 
{
    MemCore::LinearAllocator linear(chunk);

    MemCore::Block a = linear.allocate(16, 8);
    MemCore::Block b = linear.allocate(32, 16);

    EXPECT_NE(a.ptr, nullptr);
    EXPECT_NE(b.ptr, nullptr);
    
    // b must be correctly 16-aligned...
    EXPECT_TRUE(MemCore::IsAligned(b.ptr, 16));

    // ...and packed tightly after a's 16 bytes: with a 16-aligned base,
    // a sits at offset 0 and b at the next 16-byte boundary.
    std::uintptr_t addr_a = reinterpret_cast<std::uintptr_t>(a.ptr);
    std::uintptr_t addr_b = reinterpret_cast<std::uintptr_t>(b.ptr);
    EXPECT_EQ(addr_b - addr_a, 16u);
}

TEST_F(LinearAllocatorTest, OutOfMemoryHandling) 
{
    MemCore::LinearAllocator linear(chunk);

    MemCore::Block huge = linear.allocate(2048, 8);
    EXPECT_EQ(huge.ptr, nullptr); 
    EXPECT_EQ(huge.size, 0);
}