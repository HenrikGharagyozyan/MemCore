#pragma once

#include "AllocatorConcept.hpp"
#include "Align.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <iostream>


namespace MemCore
{

    template <Allocator Alloc>
    class CanaryAllocator
    {
    private:
        Alloc& m_allocator;

        // Canary values (magic numbers)
        static constexpr std::uint32_t FRONT_MAGIC = 0xDEADBEEF;
        static constexpr std::uint32_t BACK_MAGIC  = 0xBAADF00D;

        // Bookkeeping written immediately before the user payload.
        //
        // The payload is aligned by shifting it FORWARD from the upstream base,
        // so the header no longer needs an over-alignment of its own. We only
        // require that `magic` remains the LAST member (with no trailing
        // padding) so it sits directly before the payload and a one-byte
        // underflow corrupts it.
        struct Header
        {
            std::size_t user_size;     // payload size the caller asked for
            std::uint32_t base_offset; // bytes from the upstream base ptr to the payload
            std::uint32_t magic;       // FRONT canary — MUST stay the last member
        };

        static_assert(offsetof(Header, magic) + sizeof(std::uint32_t) == sizeof(Header),
            "magic must be the last bytes of Header so it sits immediately before the payload");

    public:
        explicit CanaryAllocator(Alloc& allocator) noexcept
            : m_allocator(allocator)
        {
        }

        [[nodiscard]] Block allocate(std::size_t size, std::size_t alignment) noexcept
        {
            if (size == 0)
                return { nullptr, 0 };

            // The upstream base must be aligned at least for the Header AND for
            // the caller's request, so the payload can be aligned by shifting.
            std::size_t actual_align = std::max(alignment, alignof(Header));

            // Reserve a whole number of aligned slots for the header. Because
            // the base is aligned to actual_align and header_space is a multiple
            // of it, base + header_space is guaranteed to be aligned too.
            std::size_t header_space = AlignUp(sizeof(Header), actual_align);
            std::size_t total_size = header_space + size + sizeof(std::uint32_t);

            Block block = m_allocator.allocate(total_size, actual_align);
            if (!block.ptr)
                return { nullptr, 0 };

            std::byte* base = static_cast<std::byte*>(block.ptr);
            std::byte* payload = base + header_space; // aligned to actual_align

            // The header lives directly before the (aligned) payload.
            Header* header = reinterpret_cast<Header*>(payload - sizeof(Header));
            header->user_size = size;
            header->base_offset = static_cast<std::uint32_t>(header_space);
            header->magic = FRONT_MAGIC;

            // Place the back canary just past the user data.
            // Use memcpy to stay safe on architectures that fault on unaligned access.
            std::memcpy(payload + size, &BACK_MAGIC, sizeof(BACK_MAGIC));

            return { payload, size };
        }

        // noexcept: on corruption we std::abort (never throw), and the wrapped
        // deallocate is itself noexcept, so this satisfies the Allocator contract.
        void deallocate(void* ptr, std::size_t /*size*/) noexcept
        {
            if (!ptr)
                return;

            std::byte* payload = static_cast<std::byte*>(ptr);
            Header* header = reinterpret_cast<Header*>(payload - sizeof(Header));

            // Check 1: Front Canary (protection against writes before the array)
            if (header->magic != FRONT_MAGIC)
            {
                std::cerr << "[MemCore FATAL] Front Canary corrupted! Buffer Underflow at " << ptr << "\n";
                std::abort(); // Terminate the program immediately!
            }

            // Check 2: Back Canary (protection against buffer overflow)
            std::uint32_t back_magic;
            std::memcpy(&back_magic, payload + header->user_size, sizeof(BACK_MAGIC));

            if (back_magic != BACK_MAGIC)
            {
                std::cerr << "[MemCore FATAL] Back Canary corrupted! Buffer Overflow at " << ptr << "\n";
                std::abort(); // Terminate the program immediately!
            }

            // Recover the original upstream pointer via the stored offset.
            std::byte* base = payload - header->base_offset;
            std::size_t total_size = header->base_offset + header->user_size + sizeof(std::uint32_t);
            m_allocator.deallocate(base, total_size);
        }

        // Only exposed when the wrapped layer can answer ownership, so a
        // Canary over a non-owning allocator does not falsely model OwningAllocator.
        bool owns(const void* ptr) const noexcept
            requires OwningAllocator<Alloc>
        {
            if (!ptr)
                return false;

            const std::byte* payload = static_cast<const std::byte*>(ptr);
            const Header* header = reinterpret_cast<const Header*>(payload - sizeof(Header));
            const std::byte* base = payload - header->base_offset;
            return m_allocator.owns(base);
        }
    };

} // namespace MemCore
