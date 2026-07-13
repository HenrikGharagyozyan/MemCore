#include <gtest/gtest.h>

#include <MemCore/LinearAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>

class LinearAllocatorTest : public ::testing::Test 
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

TEST_F(LinearAllocatorTest, SequentialAllocations) 
{
    MemCore::LinearAllocator linear(chunk);

    MemCore::Block a = linear.allocate(16, 8);
    MemCore::Block b = linear.allocate(32, 16);

    EXPECT_NE(a.ptr, nullptr);
    EXPECT_NE(b.ptr, nullptr);
    
    std::uintptr_t addr_a = reinterpret_cast<std::uintptr_t>(a.ptr);
    std::uintptr_t addr_b = reinterpret_cast<std::uintptr_t>(b.ptr);
    EXPECT_EQ(addr_b - addr_a, 16);
}

TEST_F(LinearAllocatorTest, OutOfMemoryHandling) 
{
    MemCore::LinearAllocator linear(chunk);

    MemCore::Block huge = linear.allocate(2048, 8);
    EXPECT_EQ(huge.ptr, nullptr); 
    EXPECT_EQ(huge.size, 0);
}