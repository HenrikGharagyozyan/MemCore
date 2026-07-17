#include <gtest/gtest.h>

#include <MemCore/CanaryAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/Align.hpp>


TEST(CanaryAllocatorTest, NormalUseDoesNotCrash)
{
    MemCore::MallocUpstream malloc_up;
    MemCore::CanaryAllocator<MemCore::MallocUpstream> canary(malloc_up);

    MemCore::Block b = canary.allocate(10, 8);
    ASSERT_NE(b.ptr, nullptr);
    
    // Write exactly 10 bytes. This is legal.
    std::memset(b.ptr, 0xAA, 10);
    
    // This should succeed without crashing
    canary.deallocate(b.ptr, b.size);
}

TEST(CanaryAllocatorTest, OverAlignedPayloadIsAligned)
{
    MemCore::MallocUpstream malloc_up;
    MemCore::CanaryAllocator<MemCore::MallocUpstream> canary(malloc_up);

    constexpr std::size_t kAlign = 64;
    MemCore::Block b = canary.allocate(100, kAlign);

    ASSERT_NE(b.ptr, nullptr);
    EXPECT_TRUE(MemCore::IsAligned(b.ptr, kAlign));

    // Fill exactly the requested size; both canaries must survive.
    std::memset(b.ptr, 0xAA, 100);
    canary.deallocate(b.ptr, b.size);
}

TEST(CanaryAllocatorTest, DetectsBufferOverflow)
{
    MemCore::MallocUpstream malloc_up;
    MemCore::CanaryAllocator<MemCore::MallocUpstream> canary(malloc_up);

    // Expect the code inside ASSERT_DEATH to terminate the program with the message "Back Canary corrupted"
    ASSERT_DEATH(
        {
            MemCore::Block b = canary.allocate(10, 8);
            
            // INTENTIONAL BUG: Write 11 bytes to an array of size 10!
            // This will overwrite one byte of the back canary.
            std::memset(b.ptr, 0xAA, 11);
            
            canary.deallocate(b.ptr, b.size);
        }, "Back Canary corrupted");
}

TEST(CanaryAllocatorTest, DetectsBufferUnderflow) 
{
    MemCore::MallocUpstream malloc_up;
    MemCore::CanaryAllocator<MemCore::MallocUpstream> canary(malloc_up);

    // Expect termination with "Front Canary corrupted"
    ASSERT_DEATH(
        {
            MemCore::Block b = canary.allocate(10, 8);
            
            // INTENTIONAL BUG: Write via a negative index (before the start of the array)
            std::byte* payload = static_cast<std::byte*>(b.ptr);
            payload[-1] = std::byte{0xFF};
            
            canary.deallocate(b.ptr, b.size);
        }, "Front Canary corrupted");
}