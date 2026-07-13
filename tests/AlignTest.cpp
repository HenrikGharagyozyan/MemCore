#include <gtest/gtest.h>

#include <MemCore/Align.hpp>

TEST(AlignTests, PointerAlignment) 
{
    void* ptr = reinterpret_cast<void*>(1003);
    
    void* aligned_8 = MemCore::AlignForward(ptr, 8);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned_8), 1008);
    EXPECT_TRUE(MemCore::IsAligned(aligned_8, 8));

    void* aligned_32 = MemCore::AlignForward(ptr, 32);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned_32), 1024);
    EXPECT_TRUE(MemCore::IsAligned(aligned_32, 32));
}