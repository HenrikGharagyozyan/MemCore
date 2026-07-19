#include <gtest/gtest.h>

#include <MemCore/FreeListAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/Align.hpp>

#include <cstring>
#include <vector>

using namespace MemCore;

// --- Compile-time contract ------------------------------------------------

static_assert(Allocator<FreeListAllocator>);
static_assert(OwningAllocator<FreeListAllocator>);
static_assert(ResettableAllocator<FreeListAllocator>);

// --- Fixture: a 64 KiB region carved from malloc --------------------------

class FreeListAllocatorTest : public ::testing::Test
{
protected:
    MallocUpstream upstream;
    Block region{ nullptr, 0 };

    static constexpr std::size_t kRegionSize = 64 * 1024;

    void SetUp() override
    {
        region = upstream.allocate(kRegionSize, alignof(std::max_align_t));
        ASSERT_NE(region.ptr, nullptr);
    }

    void TearDown() override
    {
        upstream.deallocate(region.ptr, region.size);
    }
};

TEST_F(FreeListAllocatorTest, BasicAllocateOwnsAndUse)
{
    FreeListAllocator fl(region);

    Block a = fl.allocate(100, 8);
    ASSERT_NE(a.ptr, nullptr);
    EXPECT_EQ(a.size, 100u);
    EXPECT_TRUE(fl.owns(a.ptr));
    EXPECT_FALSE(fl.owns(nullptr));

    int stack_var = 0;
    EXPECT_FALSE(fl.owns(&stack_var));

    // Writing the whole payload must stay in bounds (ASan-verified).
    std::memset(a.ptr, 0xAB, 100);

    fl.deallocate(a.ptr, a.size);
}

TEST_F(FreeListAllocatorTest, ZeroSizeReturnsNull)
{
    FreeListAllocator fl(region);
    Block b = fl.allocate(0, 8);
    EXPECT_EQ(b.ptr, nullptr);
    EXPECT_EQ(b.size, 0u);
}

TEST_F(FreeListAllocatorTest, RespectsOverAlignment)
{
    FreeListAllocator fl(region);

    for (std::size_t align : { std::size_t{16}, std::size_t{32}, std::size_t{64}, std::size_t{128} })
    {
        Block b = fl.allocate(200, align);
        ASSERT_NE(b.ptr, nullptr) << "alignment " << align;
        EXPECT_TRUE(IsAligned(b.ptr, align)) << "alignment " << align;
        std::memset(b.ptr, 0xCD, 200);
        fl.deallocate(b.ptr, b.size);
    }
}

TEST_F(FreeListAllocatorTest, AllocationsDoNotOverlap)
{
    FreeListAllocator fl(region);

    Block a = fl.allocate(300, 8);
    Block b = fl.allocate(300, 8);
    ASSERT_NE(a.ptr, nullptr);
    ASSERT_NE(b.ptr, nullptr);

    auto pa = reinterpret_cast<std::uintptr_t>(a.ptr);
    auto pb = reinterpret_cast<std::uintptr_t>(b.ptr);

    // Disjoint [pa, pa+300) and [pb, pb+300).
    EXPECT_TRUE(pa + 300 <= pb || pb + 300 <= pa);

    fl.deallocate(a.ptr, a.size);
    fl.deallocate(b.ptr, b.size);
}

TEST_F(FreeListAllocatorTest, FreedBlockIsReused)
{
    FreeListAllocator fl(region);

    Block a = fl.allocate(500, 8);
    Block b = fl.allocate(500, 8);
    Block c = fl.allocate(500, 8);
    ASSERT_NE(a.ptr, nullptr);
    ASSERT_NE(b.ptr, nullptr);
    ASSERT_NE(c.ptr, nullptr);

    // Free the middle block; a same-size request should reclaim that slot.
    fl.deallocate(b.ptr, b.size);
    Block d = fl.allocate(500, 8);
    ASSERT_NE(d.ptr, nullptr);
    EXPECT_EQ(d.ptr, b.ptr);

    fl.deallocate(a.ptr, a.size);
    fl.deallocate(c.ptr, c.size);
    fl.deallocate(d.ptr, d.size);
}

TEST_F(FreeListAllocatorTest, CoalescesAdjacentFreeBlocks)
{
    FreeListAllocator fl(region);

    // Three large adjacent blocks nearly fill the region.
    Block c1 = fl.allocate(18000, 8);
    Block c2 = fl.allocate(18000, 8);
    Block c3 = fl.allocate(18000, 8);
    ASSERT_NE(c1.ptr, nullptr);
    ASSERT_NE(c2.ptr, nullptr);
    ASSERT_NE(c3.ptr, nullptr);

    // A block this large cannot fit until the three are merged back together.
    EXPECT_EQ(fl.allocate(50000, 8).ptr, nullptr);

    fl.deallocate(c2.ptr, c2.size); // middle
    fl.deallocate(c1.ptr, c1.size); // coalesce backward
    fl.deallocate(c3.ptr, c3.size); // coalesce forward -> one big block

    Block big = fl.allocate(50000, 8);
    ASSERT_NE(big.ptr, nullptr) << "coalescing did not reclaim contiguous space";
    std::memset(big.ptr, 0x11, 50000);
    fl.deallocate(big.ptr, big.size);
}

TEST_F(FreeListAllocatorTest, ManyMixedSizeAllocationsAnyOrder)
{
    FreeListAllocator fl(region);

    std::vector<Block> live;
    for (int i = 0; i < 400; ++i)
    {
        std::size_t sz = 16 + static_cast<std::size_t>((i * 37) % 400);
        std::size_t align = std::size_t{8} << (i % 4); // 8, 16, 32, 64

        Block x = fl.allocate(sz, align);
        if (x.ptr)
        {
            EXPECT_TRUE(IsAligned(x.ptr, align));
            std::memset(x.ptr, i & 0xFF, sz);
            live.push_back(x);
        }

        // Periodically release the oldest to exercise coalescing paths.
        if (live.size() > 30)
        {
            fl.deallocate(live.front().ptr, live.front().size);
            live.erase(live.begin());
        }
    }

    for (auto& x : live)
        fl.deallocate(x.ptr, x.size);

    // Everything is free again: a near-region-size allocation must succeed.
    Block full = fl.allocate(50000, 8);
    EXPECT_NE(full.ptr, nullptr);
    if (full.ptr)
        fl.deallocate(full.ptr, full.size);
}

TEST_F(FreeListAllocatorTest, ResetReclaimsWholeRegion)
{
    FreeListAllocator fl(region);

    // Fragment the region, then reset.
    std::vector<Block> blocks;
    for (int i = 0; i < 20; ++i)
    {
        Block b = fl.allocate(1000, 8);
        if (b.ptr)
            blocks.push_back(b);
    }
    ASSERT_FALSE(blocks.empty());

    fl.reset(); // drop everything without individual frees

    Block big = fl.allocate(50000, 8);
    ASSERT_NE(big.ptr, nullptr) << "reset did not reclaim the region";
    fl.deallocate(big.ptr, big.size);
}

TEST_F(FreeListAllocatorTest, OutOfMemoryReturnsNull)
{
    FreeListAllocator fl(region);
    EXPECT_EQ(fl.allocate(1u << 20, 8).ptr, nullptr); // 1 MiB into a 64 KiB region
}
