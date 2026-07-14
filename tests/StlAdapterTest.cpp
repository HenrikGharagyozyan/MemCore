#include <gtest/gtest.h>

#include <MemCore/StlAdapter.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>

#include <vector>
#include <list>


TEST(StlAdapterTest, VectorIntegration) 
{
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk = upstream.allocate(1024, 8);

    {
        MemCore::LinearAllocator linear(chunk);
        
        // 1. Create the adapter by passing in our allocator
        MemCore::StlAdapter<int, MemCore::LinearAllocator> adapter(linear);
        
        // 2. Pass the ADAPTER to the vector constructor
        std::vector<int, MemCore::StlAdapter<int, MemCore::LinearAllocator>> vec(adapter);

        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        EXPECT_EQ(vec.size(), 3);
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 20);
        EXPECT_EQ(vec[2], 30);
    }

    upstream.deallocate(chunk.ptr, chunk.size);
}

TEST(StlAdapterTest, ListIntegration) 
{
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk = upstream.allocate(2048, 8);

    {
        MemCore::LinearAllocator linear(chunk);
        
        // 1. Create the adapter
        MemCore::StlAdapter<int, MemCore::LinearAllocator> adapter(linear);
        
        // 2. Pass the ADAPTER to the list constructor
        std::list<int, MemCore::StlAdapter<int, MemCore::LinearAllocator>> list(adapter);

        list.push_back(100);
        list.push_back(200);

        EXPECT_EQ(list.front(), 100);
        EXPECT_EQ(list.back(), 200);
    }

    upstream.deallocate(chunk.ptr, chunk.size);
}