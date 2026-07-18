#include <gtest/gtest.h>

#include <MemCore/AllocatorConcept.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/StackAllocator.hpp>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/CanaryAllocator.hpp>
#include <MemCore/TrackerAllocator.hpp>
#include <MemCore/FallbackAllocator.hpp>

using namespace MemCore;

// --- Compile-time contract checks -----------------------------------------

// Leaf allocators backed by a single contiguous region model OwningAllocator.
static_assert(OwningAllocator<LinearAllocator>);
static_assert(OwningAllocator<StackAllocator>);
static_assert(OwningAllocator<PoolAllocator>);
static_assert(OwningAllocator<ArenaAllocator<MallocUpstream>>);

// OwningAllocator refines Allocator, so every owning allocator is an allocator.
static_assert(Allocator<LinearAllocator>);

// The thin OS passthrough is a valid Allocator but deliberately NOT owning:
// it may only be the fallback leg of a FallbackAllocator, never the primary.
static_assert(Allocator<MallocUpstream>);
static_assert(!OwningAllocator<MallocUpstream>);

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

// --- Runtime sanity: owns() actually routes by range ----------------------

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
