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