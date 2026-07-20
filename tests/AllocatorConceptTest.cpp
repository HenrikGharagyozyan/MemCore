#include <gtest/gtest.h>

#include <MemCore/AllocatorConcept.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/VirtualUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/StackAllocator.hpp>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/CanaryAllocator.hpp>
#include <MemCore/TrackerAllocator.hpp>
#include <MemCore/FallbackAllocator.hpp>
#include <MemCore/ThreadSafeAllocator.hpp>
#include <MemCore/StlAdapter.hpp>

#include <vector>

using namespace MemCore;

// --- Compile-time contract checks -----------------------------------------

// Leaf allocators backed by a single contiguous region model OwningAllocator.
static_assert(OwningAllocator<LinearAllocator>);
static_assert(OwningAllocator<StackAllocator>);
static_assert(OwningAllocator<PoolAllocator>);
static_assert(OwningAllocator<ArenaAllocator<MallocUpstream>>);

// OwningAllocator refines Allocator, so every owning allocator is an allocator.
static_assert(Allocator<LinearAllocator>);

// The thin OS passthroughs are valid Allocators but deliberately NOT owning:
// they own no contiguous range, so they may only be the fallback leg of a
// FallbackAllocator, never the primary.
static_assert(Allocator<MallocUpstream>);
static_assert(!OwningAllocator<MallocUpstream>);
static_assert(Allocator<VirtualUpstream>);
static_assert(!OwningAllocator<VirtualUpstream>);

// reset() is a refinement, not part of the base contract. Allocators over a
// region (and the malloc passthrough, via a no-op) can reset everything.
static_assert(ResettableAllocator<LinearAllocator>);
static_assert(ResettableAllocator<StackAllocator>);
static_assert(ResettableAllocator<PoolAllocator>);
static_assert(ResettableAllocator<ArenaAllocator<MallocUpstream>>);
static_assert(ResettableAllocator<MallocUpstream>);

// The point of shrinking the base contract: decorators are now valid
// Allocators, even though they own no memory to reset themselves.
static_assert(Allocator<CanaryAllocator<MallocUpstream>>);
static_assert(Allocator<TrackerAllocator<MallocUpstream>>);
static_assert(Allocator<FallbackAllocator<LinearAllocator, MallocUpstream>>);

// ...and they are correctly NOT Resettable (reset stays with the wrapped layer).
static_assert(!ResettableAllocator<CanaryAllocator<MallocUpstream>>);
static_assert(!ResettableAllocator<TrackerAllocator<MallocUpstream>>);

// Ownership honesty: a decorator's guarded owns() makes it model OwningAllocator
// ONLY when the layer it wraps does -- not a bare signature that fails to compile.
static_assert(OwningAllocator<CanaryAllocator<LinearAllocator>>);
static_assert(!OwningAllocator<CanaryAllocator<MallocUpstream>>);
static_assert(OwningAllocator<TrackerAllocator<LinearAllocator>>);
static_assert(!OwningAllocator<TrackerAllocator<MallocUpstream>>);

// FallbackAllocator requires an owning primary (enforced by its constraint); it
// is itself owning only when the fallback leg is owning too.
static_assert(OwningAllocator<FallbackAllocator<LinearAllocator, StackAllocator>>);
static_assert(!OwningAllocator<FallbackAllocator<LinearAllocator, MallocUpstream>>);

// ThreadSafeAllocator must model the contract too, otherwise it cannot appear
// at any composition point (StlAdapter, Arena upstream, inside a decorator).
// Its operations are noexcept because a mutex failure is unrecoverable here.
static_assert(Allocator<ThreadSafeAllocator<LinearAllocator>>);
static_assert(ResettableAllocator<ThreadSafeAllocator<LinearAllocator>>);
static_assert(OwningAllocator<ThreadSafeAllocator<LinearAllocator>>);

// It must also compose: wrapping it in a decorator and using it as an Arena
// upstream are both constrained on Allocator.
static_assert(Allocator<TrackerAllocator<ThreadSafeAllocator<LinearAllocator>>>);
static_assert(Allocator<ArenaAllocator<ThreadSafeAllocator<MallocUpstream>>>);

// --- Runtime sanity: owns() actually routes by range ----------------------

// Regression for the API freeze: a ThreadSafeAllocator can be handed to an STL
// container through StlAdapter. This did not compile before its operations were
// made noexcept, because StlAdapter is constrained on the Allocator concept.
TEST(AllocatorConcept, ThreadSafeAllocatorComposesWithStlAdapter)
{
    MallocUpstream up;
    Block chunk = up.allocate(4096, 16);
    LinearAllocator lin(chunk);
    ThreadSafeAllocator<LinearAllocator> safe(lin);

    using Alloc = StlAdapter<int, ThreadSafeAllocator<LinearAllocator>>;
    std::vector<int, Alloc> v{ Alloc(safe) };

    for (int i = 0; i < 32; ++i)
        v.push_back(i);

    EXPECT_EQ(v.size(), 32u);
    EXPECT_EQ(v[31], 31);
    EXPECT_TRUE(safe.owns(v.data()));

    up.deallocate(chunk.ptr, chunk.size);
}

TEST(AllocatorConcept, OwnsAnswersByRange)
{
    MallocUpstream up;
    Block chunk = up.allocate(256, 16);
    LinearAllocator lin(chunk);

    Block b = lin.allocate(32, 8);
    ASSERT_NE(b.ptr, nullptr);

    EXPECT_TRUE(lin.owns(b.ptr));    // inside its region
    EXPECT_FALSE(lin.owns(nullptr)); // null is never owned

    int stack_var = 0;
    EXPECT_FALSE(lin.owns(&stack_var)); // unrelated pointer

    up.deallocate(chunk.ptr, chunk.size);
}
