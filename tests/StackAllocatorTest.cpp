#include <gtest/gtest.h>

#include <MemCore/StackAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>

class StackAllocatorTest : public ::testing::Test 
{
protected:
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk;

    void SetUp() override 
    {
        chunk = upstream.allocate(1024, 8);
    }

    void TearDown() override 
    {
        upstream.deallocate(chunk.ptr, chunk.size);
    }
};

TEST_F(StackAllocatorTest, LIFODeallocation) 
{
    MemCore::StackAllocator stack(chunk);

    MemCore::Block a = stack.allocate(16, 8);
    MemCore::Block b = stack.allocate(16, 8);

    stack.deallocate(b.ptr, b.size);

    MemCore::Block c = stack.allocate(16, 8);
    EXPECT_EQ(c.ptr, b.ptr);
}

// A5: an out-of-order (non-LIFO) deallocate must not corrupt the cursor.
// Debug builds trip the assert; release builds make it a safe no-op.
TEST_F(StackAllocatorTest, OutOfOrderDeallocateDoesNotCorruptCursor)
{
    MemCore::StackAllocator stack(chunk);

    MemCore::Block a = stack.allocate(16, 8);
    MemCore::Block b = stack.allocate(16, 8);
    ASSERT_NE(a.ptr, nullptr);
    ASSERT_NE(b.ptr, nullptr);

    // Freeing 'a' while 'b' is still on top violates LIFO.
#ifdef NDEBUG
    auto marker = stack.get_marker();
    stack.deallocate(a.ptr, a.size);          // no-op in release
    EXPECT_EQ(stack.get_marker(), marker);     // cursor untouched

    // The stack is still consistent: proper LIFO frees still work.
    stack.deallocate(b.ptr, b.size);
    stack.deallocate(a.ptr, a.size);
#else
    ASSERT_DEATH(stack.deallocate(a.ptr, a.size), "LIFO violated");
#endif
}

TEST_F(StackAllocatorTest, MarkerRollback)
{
    MemCore::StackAllocator stack(chunk);

    MemCore::Block a = stack.allocate(16, 8);
    auto marker = stack.get_marker(); 

    MemCore::Block b = stack.allocate(16, 8);
    MemCore::Block c = stack.allocate(16, 8);

    stack.free_to_marker(marker); 

    MemCore::Block d = stack.allocate(16, 8);
    EXPECT_EQ(d.ptr, b.ptr); 
}