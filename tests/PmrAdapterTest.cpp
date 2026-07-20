#include <gtest/gtest.h>

#include <MemCore/PmrAdapter.hpp>

#ifdef MEMCORE_HAS_PMR

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/FreeListAllocator.hpp>

#include <memory_resource>
#include <vector>
#include <string>
#include <unordered_map>
#include <numeric>

using namespace MemCore;

class PmrAdapterTest : public ::testing::Test
{
protected:
    MallocUpstream upstream;
    Block region{ nullptr, 0 };

    void SetUp() override
    {
        region = upstream.allocate(64 * 1024, alignof(std::max_align_t));
        ASSERT_NE(region.ptr, nullptr);
    }

    void TearDown() override
    {
        upstream.deallocate(region.ptr, region.size);
    }
};

TEST_F(PmrAdapterTest, PmrVectorAllocatesFromMemCore)
{
    LinearAllocator arena(region);
    PmrAdapter resource(arena);

    std::pmr::vector<int> v(&resource);
    for (int i = 0; i < 500; ++i)
        v.push_back(i);

    ASSERT_EQ(v.size(), 500u);
    EXPECT_EQ(v[499], 499);
    EXPECT_EQ(std::accumulate(v.begin(), v.end(), 0), 124750);

    // The container's buffer really came out of our arena.
    EXPECT_TRUE(arena.owns(v.data()));
}

TEST_F(PmrAdapterTest, PmrStringAndMapWork)
{
    FreeListAllocator fl(region);
    PmrAdapter resource(fl);

    std::pmr::string s("memcore pmr integration test string", &resource);
    EXPECT_EQ(s.size(), 35u);

    std::pmr::unordered_map<int, std::pmr::string> map(&resource);
    for (int i = 0; i < 50; ++i)
        map.emplace(i, std::pmr::string("value", &resource));

    EXPECT_EQ(map.size(), 50u);
    EXPECT_EQ(map.at(7), "value");
}

// A FreeListAllocator reclaims space, so a pmr container that grows and is then
// destroyed must return its memory -- exercised by repeated build/teardown.
TEST_F(PmrAdapterTest, MemoryIsReturnedOnContainerDestruction)
{
    FreeListAllocator fl(region);
    PmrAdapter resource(fl);

    for (int round = 0; round < 20; ++round)
    {
        std::pmr::vector<int> v(&resource);
        for (int i = 0; i < 1000; ++i)
            v.push_back(i);
        EXPECT_EQ(v.size(), 1000u);
    } // destroyed each round; without real reclamation this would exhaust

    // Still room for a large allocation after all that churn.
    std::pmr::vector<int> final_vec(&resource);
    for (int i = 0; i < 1000; ++i)
        final_vec.push_back(i);
    EXPECT_EQ(final_vec.size(), 1000u);
}

TEST_F(PmrAdapterTest, IsEqualComparesBackingAllocator)
{
    LinearAllocator a(region);
    Block other_region = upstream.allocate(1024, 16);
    LinearAllocator b(other_region);

    PmrAdapter ra1(a);
    PmrAdapter ra2(a); // same allocator
    PmrAdapter rb(b);  // different allocator

    EXPECT_TRUE(ra1.is_equal(ra2));
    EXPECT_FALSE(ra1.is_equal(rb));
    EXPECT_FALSE(ra1.is_equal(*std::pmr::new_delete_resource()));

    upstream.deallocate(other_region.ptr, other_region.size);
}

TEST_F(PmrAdapterTest, ThrowsBadAllocWhenExhausted)
{
    Block tiny = upstream.allocate(256, 16);
    LinearAllocator small(tiny);
    PmrAdapter resource(small);

    // pmr requires reporting failure by throwing, unlike MemCore's nullptr.
    EXPECT_THROW((void)resource.allocate(4096, 8), std::bad_alloc);

    upstream.deallocate(tiny.ptr, tiny.size);
}

TEST_F(PmrAdapterTest, ZeroByteRequestReturnsUsablePointer)
{
    LinearAllocator arena(region);
    PmrAdapter resource(arena);

    // MemCore returns nullptr for size 0, but pmr expects a real pointer.
    void* p = resource.allocate(0, alignof(std::max_align_t));
    EXPECT_NE(p, nullptr);
    resource.deallocate(p, 0, alignof(std::max_align_t));
}

#endif // MEMCORE_HAS_PMR
