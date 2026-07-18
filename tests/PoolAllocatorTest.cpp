#include <gtest/gtest.h>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/Align.hpp>
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

// Regression (found via UBSan): when the user's alignment is smaller than
// alignof(FreeNode), chunks must still be aligned for the intrusive free-list
// node. chunk_size 28 / alignment 4 previously placed odd chunks at 4-aligned
// (non-8) addresses, making the free-list pointer write misaligned UB.
TEST_F(PoolAllocatorTest, ChunksAlignedForFreeNodeUnderSmallAlignment)
{
    MemCore::PoolAllocator pool(chunk, 28, 4);

    std::vector<void*> ptrs;
    for (int i = 0; i < 6; ++i)
    {
        MemCore::Block b = pool.allocate(28, 4);
        ASSERT_NE(b.ptr, nullptr);
        // Must satisfy the internal FreeNode alignment (alignof a pointer),
        // not just the user's 4.
        EXPECT_TRUE(MemCore::IsAligned(b.ptr, alignof(void*)));
        ptrs.push_back(b.ptr);
    }
    for (void* p : ptrs)
        pool.deallocate(p, 28);
}

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