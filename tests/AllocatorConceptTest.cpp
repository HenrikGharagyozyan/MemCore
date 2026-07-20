#include <gtest/gtest.h>

#include <MemCore/AllocatorConcept.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/VirtualUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/StackAllocator.hpp>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/FreeListAllocator.hpp>
#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/CanaryAllocator.hpp>
#include <MemCore/TrackerAllocator.hpp>
#include <MemCore/FallbackAllocator.hpp>
#include <MemCore/ThreadSafeAllocator.hpp>
#include <MemCore/StlAdapter.hpp>

#include <vector>
#include <type_traits>
#include <utility>

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

// --- Copy/move semantics (API freeze) -------------------------------------
// Stateful region allocators must NOT be copyable: a copy would duplicate the
// cursor/free list over the same region and hand out overlapping memory.
// They are movable instead, leaving the source empty.
static_assert(!std::is_copy_constructible_v<LinearAllocator>);
static_assert(!std::is_copy_constructible_v<StackAllocator>);
static_assert(!std::is_copy_constructible_v<PoolAllocator>);
static_assert(!std::is_copy_constructible_v<FreeListAllocator>);

static_assert(std::is_move_constructible_v<LinearAllocator>);
static_assert(std::is_move_constructible_v<StackAllocator>);
static_assert(std::is_move_constructible_v<PoolAllocator>);
static_assert(std::is_move_constructible_v<FreeListAllocator>);

// The contract says a zero-size request yields { nullptr, 0 } for EVERY
// allocator. This was inconsistent before the API freeze (half returned a
// non-null pointer), so it is pinned down here.
TEST(AllocatorConcept, ZeroSizeAllocationReturnsNullEverywhere)
{
    MallocUpstream up;
    Block r1 = up.allocate(4096, 16);
    Block r2 = up.allocate(4096, 16);
    Block r3 = up.allocate(4096, 16);
    Block r4 = up.allocate(4096, 16);

    LinearAllocator lin(r1);
    StackAllocator stk(r2);
    PoolAllocator pool(r3, 64, 16);
    FreeListAllocator fl(r4);
    ArenaAllocator<MallocUpstream> arena(up);
    CanaryAllocator<LinearAllocator> canary(lin);
    TrackerAllocator<MallocUpstream> tracker(up);

    EXPECT_EQ(up.allocate(0, 8).ptr, nullptr)      << "MallocUpstream";
    EXPECT_EQ(lin.allocate(0, 8).ptr, nullptr)     << "LinearAllocator";
    EXPECT_EQ(stk.allocate(0, 8).ptr, nullptr)     << "StackAllocator";
    EXPECT_EQ(pool.allocate(0, 8).ptr, nullptr)    << "PoolAllocator";
    EXPECT_EQ(fl.allocate(0, 8).ptr, nullptr)      << "FreeListAllocator";
    EXPECT_EQ(arena.allocate(0, 8).ptr, nullptr)   << "ArenaAllocator";
    EXPECT_EQ(canary.allocate(0, 8).ptr, nullptr)  << "CanaryAllocator";
    EXPECT_EQ(tracker.allocate(0, 8).ptr, nullptr) << "TrackerAllocator";

    up.deallocate(r1.ptr, r1.size);
    up.deallocate(r2.ptr, r2.size);
    up.deallocate(r3.ptr, r3.size);
    up.deallocate(r4.ptr, r4.size);
}

// A moved-from region allocator is empty and safe to touch: it must not hand
// out memory, and must not claim to own anything.
TEST(AllocatorConcept, MovedFromRegionAllocatorIsEmpty)
{
    MallocUpstream up;
    Block chunk = up.allocate(1024, 16);

    LinearAllocator src(chunk);
    Block before = src.allocate(64, 8);
    ASSERT_NE(before.ptr, nullptr);

    LinearAllocator dst(std::move(src));

    // The moved-to allocator keeps serving from the region...
    Block after = dst.allocate(64, 8);
    EXPECT_NE(after.ptr, nullptr);
    EXPECT_TRUE(dst.owns(after.ptr));

    // ...and the moved-from one is inert rather than aliasing the same memory.
    EXPECT_EQ(src.allocate(64, 8).ptr, nullptr);
    EXPECT_FALSE(src.owns(after.ptr));

    up.deallocate(chunk.ptr, chunk.size);
}

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
