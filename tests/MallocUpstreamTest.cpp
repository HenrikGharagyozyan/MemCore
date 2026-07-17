#include <gtest/gtest.h>

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/Align.hpp>

#include <vector>

using namespace MemCore;

// A small alignment (below the platform minimum) must still be honored and legal.
TEST(MallocUpstream, SmallAlignmentIsHonored)
{
    MallocUpstream up;
    Block b = up.allocate(sizeof(int), alignof(int));

    ASSERT_NE(b.ptr, nullptr);
    EXPECT_GE(b.size, sizeof(int));
    EXPECT_TRUE(IsAligned(b.ptr, alignof(int)));

    up.deallocate(b.ptr, b.size);
}

// The core A1 fix: over-aligned requests (> alignof(max_align_t)) must return
// a correctly aligned pointer.
TEST(MallocUpstream, OverAlignedRequestIsAligned)
{
    MallocUpstream up;
    constexpr std::size_t kAlign = 64;

    Block b = up.allocate(200, kAlign);

    ASSERT_NE(b.ptr, nullptr);
    EXPECT_TRUE(IsAligned(b.ptr, kAlign));

    up.deallocate(b.ptr, b.size);
}

TEST(MallocUpstream, ZeroSizeReturnsNull)
{
    MallocUpstream up;
    Block b = up.allocate(0, 8);

    EXPECT_EQ(b.ptr, nullptr);
    EXPECT_EQ(b.size, 0u);
}

// Round-trip many allocations to shake out alloc/free path mismatches
// (this is what catches a Windows _aligned_malloc / std::free mistake under a
// heap-checking build).
TEST(MallocUpstream, ManyAllocationsRoundTrip)
{
    MallocUpstream up;
    std::vector<Block> blocks;

    for (int i = 1; i <= 100; ++i)
    {
        Block b = up.allocate(static_cast<std::size_t>(i) * 8, 16);
        ASSERT_NE(b.ptr, nullptr);
        EXPECT_TRUE(IsAligned(b.ptr, 16));
        blocks.push_back(b);
    }

    for (auto& b : blocks)
        up.deallocate(b.ptr, b.size);
}
