#include <gtest/gtest.h>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <vector>

class PoolAllocatorTest : public ::testing::Test 
{
protected:
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk;

    void SetUp() override 
    {
        chunk = upstream.allocate(1024, 8); // Allocate 1 KB for the pool
    }

    void TearDown() override 
    {
        upstream.deallocate(chunk.ptr, chunk.size);
    }
};

TEST_F(PoolAllocatorTest, AllocateAndDeallocateAnyOrder) 
{
    // Create a pool for 32-byte chunks with 8-byte alignment
    MemCore::PoolAllocator pool(chunk, 32, 8);

    MemCore::Block a = pool.allocate(32, 8);
    MemCore::Block b = pool.allocate(32, 8);
    MemCore::Block c = pool.allocate(32, 8);

    EXPECT_NE(a.ptr, nullptr);
    EXPECT_NE(b.ptr, nullptr);
    EXPECT_NE(c.ptr, nullptr);

    // Remove from the middle (Block B)
    pool.deallocate(b.ptr, b.size);

    // The next allocation must immediately reuse Block B's slot
    MemCore::Block d = pool.allocate(32, 8);
    EXPECT_EQ(d.ptr, b.ptr);
}

TEST_F(PoolAllocatorTest, OutOfMemoryHandling) 
{
    // 1024 bytes / 128 bytes = exactly 8 chunks
    MemCore::PoolAllocator pool(chunk, 128, 8);

    std::vector<MemCore::Block> blocks;
    // Exhaust the entire pool
    for (int i = 0; i < 8; ++i) 
    {
        blocks.push_back(pool.allocate(128, 8));
        EXPECT_NE(blocks.back().ptr, nullptr);
    }

    // The 9th attempt should return nullptr (pool is empty)
    MemCore::Block overflow = pool.allocate(128, 8);
    EXPECT_EQ(overflow.ptr, nullptr);
}