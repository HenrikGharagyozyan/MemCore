#include <gtest/gtest.h>

#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/Align.hpp>

#include <cstring>


// The A4 fix: a large-alignment request must not be shifted past the end of a
// freshly grown block. With a tiny default block size the alignment padding
// would previously overrun the block.
TEST(ArenaAllocatorTest, LargeAlignmentFitsWithinBlock)
{
    MemCore::MallocUpstream upstream;
    MemCore::ArenaAllocator arena(upstream, 64); // tiny default block

    constexpr std::size_t kAlign = 128;
    MemCore::Block b = arena.allocate(32, kAlign);

    ASSERT_NE(b.ptr, nullptr);
    EXPECT_TRUE(MemCore::IsAligned(b.ptr, kAlign));
    EXPECT_TRUE(arena.owns(b.ptr));

    // Writing the full payload must stay within the block (ASan-verified).
    std::memset(b.ptr, 0xCD, 32);

    // A second aligned allocation should also succeed.
    MemCore::Block b2 = arena.allocate(16, kAlign);
    ASSERT_NE(b2.ptr, nullptr);
    EXPECT_TRUE(MemCore::IsAligned(b2.ptr, kAlign));
    std::memset(b2.ptr, 0xEF, 16);
}

TEST(ArenaAllocatorTest, GrowthAndReset)
{
    MemCore::MallocUpstream upstream;
    
    // Create an arena with a tiny default block size (64 bytes), 
    // to easily force it to grow
    MemCore::ArenaAllocator arena(upstream, 64);

    // 1. First allocation
    MemCore::Block a = arena.allocate(32, 8);
    EXPECT_NE(a.ptr, nullptr);

    // 2. Second allocation: 32 bytes (A) + intrusive list header size 
    // will no longer fit in the initial 64-byte block. The arena must grow automatically!
    MemCore::Block b = arena.allocate(40, 8);
    EXPECT_NE(b.ptr, nullptr);

    // Since the blocks live in different OS blocks, their addresses should not be adjacent
    EXPECT_NE(static_cast<std::byte*>(a.ptr) + 32, static_cast<std::byte*>(b.ptr));

    // 3. Check reset — it should correctly return all memory without leaks
    arena.reset();
    
    // After reset, a new allocation should still work normally
    MemCore::Block c = arena.allocate(16, 8);
    EXPECT_NE(c.ptr, nullptr);
}