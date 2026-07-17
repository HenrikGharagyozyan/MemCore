#include <gtest/gtest.h>

#include <MemCore/TrackerAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/Align.hpp>

#include <cstring>


// The A3 fix: over-aligned requests must return a correctly aligned payload,
// and the stats must still balance back to zero after deallocation.
TEST(TrackerAllocatorTest, OverAlignedPayloadIsAligned)
{
    MemCore::MallocUpstream malloc_up;
    MemCore::TrackerAllocator<MemCore::MallocUpstream> tracker(malloc_up);

    constexpr std::size_t kAlign = 64;
    MemCore::Block b = tracker.allocate(100, kAlign);

    ASSERT_NE(b.ptr, nullptr);
    EXPECT_TRUE(MemCore::IsAligned(b.ptr, kAlign));
    EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Unknown), 100u);

    // Writing the full payload must not disturb the hidden header behind it.
    std::memset(b.ptr, 0xAA, 100);

    tracker.deallocate(b.ptr, b.size);
    EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Unknown), 0u);
}

TEST(TrackerAllocatorTest, TracksCategoriesSeparately)
{
    MemCore::MallocUpstream malloc_up;
    MemCore::TrackerAllocator<MemCore::MallocUpstream> tracker(malloc_up);

    // 1. Allocation without a tag (it will go to Unknown)
    MemCore::Block b_unk = tracker.allocate(100, 8);
    EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Unknown), 100);

    {
        // 2. Simulate the work of the engine's graphics subsystem
        MemCore::TagScope graphics_zone(MemCore::MemoryTag::Graphics);
        
        MemCore::Block b_gfx1 = tracker.allocate(250, 8);
        MemCore::Block b_gfx2 = tracker.allocate(150, 8);
        
        EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Graphics), 400);
        EXPECT_EQ(tracker.get_peak(MemCore::MemoryTag::Graphics), 400);

        {
            // Nested scope: for example, a sound effect is triggered inside rendering
            MemCore::TagScope audio_zone(MemCore::MemoryTag::Audio);
            MemCore::Block b_audio = tracker.allocate(50, 8);
            
            EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Audio), 50);
            // Graphics did not change in this case
            EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Graphics), 400);
            
            tracker.deallocate(b_audio.ptr, b_audio.size);
        }
        
        // Release part of the graphics allocation
        tracker.deallocate(b_gfx1.ptr, b_gfx1.size);
        EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Graphics), 150);
        EXPECT_EQ(tracker.get_peak(MemCore::MemoryTag::Graphics), 400); // The peak remembers the maximum!
        
        tracker.deallocate(b_gfx2.ptr, b_gfx2.size);
    }

    tracker.deallocate(b_unk.ptr, b_unk.size);
    
    // At the end everything should be back to zero
    EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Unknown), 0);
    EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Graphics), 0);
    EXPECT_EQ(tracker.get_allocated(MemCore::MemoryTag::Audio), 0);
}