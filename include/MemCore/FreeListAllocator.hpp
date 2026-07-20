#pragma once
#include "AllocatorConcept.hpp"
#include "Align.hpp"

#include <cstddef>
#include <cstdint>
#include <cassert>

namespace MemCore
{

    // A general-purpose allocator over a single fixed region of memory.
    //
    // Unlike Linear/Stack (which only bump) or Pool (fixed-size slots), the
    // FreeListAllocator supports variable-size allocation AND freeing in any
    // order, reclaiming space by keeping a free list and coalescing physically
    // adjacent free blocks on deallocation.
    //
    // Block layout (every block, free or used):
    //
    //     block_start                                            block_end
    //     | Header |  ... payload / free-list links ...  | Footer |
    //
    //   - Header (at block_start): total block size + free flag.
    //   - Footer (at block_end - sizeof(Footer)): mirrors the size, so the
    //     block *before* another block can be found for backward coalescing.
    //   - While a block is FREE, its payload area holds an intrusive FreeNode
    //     (next/prev) threading it into the free list. That space is returned
    //     to the user once the block is allocated.
    //
    // Allocation returns an aligned pointer. Because a block starts at a fixed
    // boundary but the user may request a larger alignment, the payload is
    // shifted forward inside the block and an "anchor" word storing the block
    // start is written immediately before the returned pointer, so deallocate()
    // can recover the owning block. (Same idea as CanaryAllocator/Tracker.)
    class FreeListAllocator
    {
    private:
        struct Header
        {
            std::size_t size;   // total block size, including Header and Footer
            bool is_free;
        };

        struct Footer
        {
            std::size_t size;   // mirror of the owning block's size
        };

        // Overlaid in the payload of a FREE block; gone once allocated.
        struct FreeNode
        {
            FreeNode* next;
            FreeNode* prev;
        };

        // Word written just before the returned payload, pointing back to the
        // block start so deallocate() can find the Header regardless of shift.
        using Anchor = std::uintptr_t;

        // All block boundaries are kept on this alignment so every Header,
        // Footer and FreeNode access is naturally aligned.
        static constexpr std::size_t BLOCK_ALIGN = alignof(std::max_align_t);

        // Smallest block that can still hold the bookkeeping for a free block.
        static std::size_t min_block_size() noexcept
        {
            return AlignUp(sizeof(Header) + sizeof(FreeNode) + sizeof(Footer), BLOCK_ALIGN);
        }

        std::byte* m_region_begin; // first usable (aligned) byte
        std::byte* m_region_end;   // one past the last usable byte
        FreeNode*  m_free_head;    // head of the doubly-linked free list

        // --- small helpers ------------------------------------------------

        static Header* header_of(std::byte* block) noexcept
        {
            return reinterpret_cast<Header*>(block);
        }

        static FreeNode* node_of(std::byte* block) noexcept
        {
            return reinterpret_cast<FreeNode*>(block + sizeof(Header));
        }

        // Write the Header + matching Footer for a block.
        static void stamp_block(std::byte* block, std::size_t size, bool is_free) noexcept
        {
            Header* h = header_of(block);
            h->size = size;
            h->is_free = is_free;

            Footer* f = reinterpret_cast<Footer*>(block + size - sizeof(Footer));
            f->size = size;
        }

        void freelist_insert(std::byte* block) noexcept
        {
            FreeNode* node = node_of(block);
            node->prev = nullptr;
            node->next = m_free_head;
            if (m_free_head)
                m_free_head->prev = node;
            m_free_head = node;
        }

        void freelist_remove(std::byte* block) noexcept
        {
            FreeNode* node = node_of(block);
            if (node->prev)
                node->prev->next = node->next;
            else
                m_free_head = node->next;

            if (node->next)
                node->next->prev = node->prev;
        }

        void init_single_free_block() noexcept
        {
            std::size_t total = static_cast<std::size_t>(m_region_end - m_region_begin);
            m_free_head = nullptr;

            if (total < min_block_size())
                return; // region too small to hold even one block

            stamp_block(m_region_begin, total, /*is_free=*/true);
            freelist_insert(m_region_begin);
        }

    public:
        explicit FreeListAllocator(Block memory) noexcept
        {
            std::byte* raw = static_cast<std::byte*>(memory.ptr);
            std::byte* aligned = static_cast<std::byte*>(AlignForward(raw, BLOCK_ALIGN));

            m_region_begin = aligned;

            // Trim the end down to a BLOCK_ALIGN boundary so split remainders
            // stay aligned too.
            std::size_t usable = 0;
            if (memory.ptr && memory.size > static_cast<std::size_t>(aligned - raw))
            {
                usable = memory.size - static_cast<std::size_t>(aligned - raw);
                usable -= (usable % BLOCK_ALIGN);
            }
            m_region_end = aligned + usable;

            init_single_free_block();
        }

        // Copying would duplicate the free list over the SAME region, so two
        // allocators would hand out overlapping blocks. Move instead, which
        // leaves the source empty (its allocate() then returns nullptr).
        FreeListAllocator(const FreeListAllocator&) = delete;
        FreeListAllocator& operator=(const FreeListAllocator&) = delete;

        FreeListAllocator(FreeListAllocator&& other) noexcept
            : m_region_begin(other.m_region_begin)
            , m_region_end(other.m_region_end)
            , m_free_head(other.m_free_head)
        {
            other.m_region_begin = nullptr;
            other.m_region_end = nullptr;
            other.m_free_head = nullptr;
        }

        FreeListAllocator& operator=(FreeListAllocator&& other) noexcept
        {
            if (this != &other)
            {
                m_region_begin = other.m_region_begin;
                m_region_end = other.m_region_end;
                m_free_head = other.m_free_head;
                other.m_region_begin = nullptr;
                other.m_region_end = nullptr;
                other.m_free_head = nullptr;
            }
            return *this;
        }

        [[nodiscard]] Block allocate(std::size_t size, std::size_t alignment) noexcept
        {
            if (size == 0)
                return { nullptr, 0 };

            assert(std::has_single_bit(alignment) && "Alignment must be a power of two");

            // First-fit: walk the free list looking for a block that can host
            // an aligned payload plus its footer.
            for (FreeNode* node = m_free_head; node != nullptr; node = node->next)
            {
                std::byte* block = reinterpret_cast<std::byte*>(node) - sizeof(Header);
                std::size_t block_size = header_of(block)->size;
                std::byte* block_end = block + block_size;

                // Reserve room for the Header and the anchor word, then align
                // the payload forward. This guarantees the anchor at
                // (payload - sizeof(Anchor)) never overlaps the Header.
                std::byte* earliest = block + sizeof(Header) + sizeof(Anchor);
                std::byte* payload = static_cast<std::byte*>(AlignForward(earliest, alignment));

                std::byte* needed_end = payload + size + sizeof(Footer);
                if (needed_end > block_end)
                    continue; // does not fit here

                freelist_remove(block);

                // Try to split off the tail as a new free block. The next block
                // must start on a BLOCK_ALIGN boundary.
                std::byte* split = static_cast<std::byte*>(
                    AlignForward(needed_end, BLOCK_ALIGN));

                if (split < block_end &&
                    static_cast<std::size_t>(block_end - split) >= min_block_size())
                {
                    std::size_t alloc_size = static_cast<std::size_t>(split - block);
                    std::size_t rem_size = static_cast<std::size_t>(block_end - split);

                    stamp_block(block, alloc_size, /*is_free=*/false);
                    stamp_block(split, rem_size, /*is_free=*/true);
                    freelist_insert(split);
                }
                else
                {
                    // Not worth splitting: hand out the whole block.
                    stamp_block(block, block_size, /*is_free=*/false);
                }

                // Anchor the payload back to its block start.
                *reinterpret_cast<Anchor*>(payload - sizeof(Anchor)) =
                    reinterpret_cast<Anchor>(block);

                return { payload, size };
            }

            return { nullptr, 0 }; // no fitting block
        }

        void deallocate(void* ptr, std::size_t /*size*/) noexcept
        {
            if (!ptr)
                return;

            std::byte* payload = static_cast<std::byte*>(ptr);
            Anchor anchor = *reinterpret_cast<Anchor*>(payload - sizeof(Anchor));
            std::byte* block = reinterpret_cast<std::byte*>(anchor);

            Header* h = header_of(block);
            h->is_free = true;

            // Coalesce with the following physical block if it is free.
            std::byte* next = block + h->size;
            if (next < m_region_end && header_of(next)->is_free)
            {
                freelist_remove(next);
                h->size += header_of(next)->size;
            }

            // Coalesce with the preceding physical block if it is free.
            if (block > m_region_begin)
            {
                Footer* prev_footer = reinterpret_cast<Footer*>(block - sizeof(Footer));
                std::byte* prev = block - prev_footer->size;
                if (prev >= m_region_begin && header_of(prev)->is_free)
                {
                    freelist_remove(prev);
                    header_of(prev)->size += h->size;
                    block = prev;
                    h = header_of(block);
                }
            }

            // Re-stamp the (possibly merged) block and put it back on the list.
            stamp_block(block, h->size, /*is_free=*/true);
            freelist_insert(block);
        }

        // Release everything at once by rebuilding the single free block.
        void reset() noexcept
        {
            init_single_free_block();
        }

        bool owns(const void* ptr) const noexcept
        {
            if (!ptr)
                return false;

            auto p = reinterpret_cast<std::uintptr_t>(ptr);
            auto start = reinterpret_cast<std::uintptr_t>(m_region_begin);
            auto end = reinterpret_cast<std::uintptr_t>(m_region_end);
            return p >= start && p < end;
        }
    };

    static_assert(Allocator<FreeListAllocator>);
    static_assert(OwningAllocator<FreeListAllocator>);

}