#include <gtest/gtest.h>

#include <MemCore/TrackerAllocator.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>


TEST(TrackerAllocatorTest, TracksAllocationsAndPeak) 
{
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk = upstream.allocate(1024, 8);

    {
        MemCore::LinearAllocator linear(chunk);
        
        // Wrap the linear allocator with the tracker
        MemCore::TrackerAllocator<MemCore::LinearAllocator> tracker(linear);

        EXPECT_EQ(tracker.get_current_usage(), 0);
        EXPECT_EQ(tracker.get_peak_usage(), 0);

        // Allocate 100 bytes
        MemCore::Block a = tracker.allocate(100, 8);
        EXPECT_EQ(tracker.get_current_usage(), 100);
        EXPECT_EQ(tracker.get_peak_usage(), 100);
        EXPECT_EQ(tracker.get_allocation_count(), 1);

        // Allocate another 50 bytes
        MemCore::Block b = tracker.allocate(50, 8);
        EXPECT_EQ(tracker.get_current_usage(), 150);
        EXPECT_EQ(tracker.get_peak_usage(), 150);
        EXPECT_EQ(tracker.get_allocation_count(), 2);

        // Simulate deallocation (Linear itself does not free, but the tracker updates its counters)
        tracker.deallocate(b.ptr, b.size);
        
        // Current usage dropped, but peak stayed at 150!
        EXPECT_EQ(tracker.get_current_usage(), 100); 
        EXPECT_EQ(tracker.get_peak_usage(), 150);    
        EXPECT_EQ(tracker.get_deallocation_count(), 1);
        
        // Print statistics to the console for inspection
        tracker.print_stats();
    }

    upstream.deallocate(chunk.ptr, chunk.size);
}