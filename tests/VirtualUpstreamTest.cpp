#include <gtest/gtest.h>

#include <MemCore/VirtualUpstream.hpp>


TEST(VirtualUpstreamTest, AllocatesAndDeallocatesPages) 
{
    MemCore::VirtualUpstream vm_up;
    
    std::size_t pageSize = vm_up.get_page_size();
    EXPECT_GT(pageSize, 0); // The page size should be reasonable (usually 4096)

    // Request 100 bytes — we should receive a block sized to one page
    MemCore::Block block = vm_up.allocate(100, 8);
    
    ASSERT_NE(block.ptr, nullptr);
    EXPECT_EQ(block.size, pageSize);

    // Verify that the memory is writable
    int* intArray = reinterpret_cast<int*>(block.ptr);
    intArray[0] = 42;
    intArray[pageSize / sizeof(int) - 1] = 99;
    
    EXPECT_EQ(intArray[0], 42);

    // Release it
    vm_up.deallocate(block.ptr, block.size);
}