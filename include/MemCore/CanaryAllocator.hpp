#pragma once

#include "AllocatorConcept.hpp"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>


namespace MemCore 
{

    template <typename Allocator>
    class CanaryAllocator 
    {
    private:
        Allocator& m_allocator;

        // Canary values (magic numbers)
        static constexpr std::uint32_t FRONT_MAGIC = 0xDEADBEEF;
        static constexpr std::uint32_t BACK_MAGIC  = 0xBAADF00D;

        // Align the header to 16 bytes. This ensures that the user data that follows it
        // keeps the correct alignment.
        struct alignas(16) Header 
        {
            std::size_t user_size;

            // Explicitly fill the padding so that magic is always at the end of the header.
            // On 64-bit systems this will be 4 bytes; on 32-bit systems it will be 8 bytes.
            std::byte padding[16 - sizeof(std::size_t) - sizeof(std::uint32_t)]; // Padding to ensure alignment

            std::uint32_t magic;
        };

    public:
        explicit CanaryAllocator(Allocator& allocator) noexcept
            : m_allocator(allocator) 
        {
        }

        Block allocate(std::size_t size, std::size_t alignment) 
        {
            // Request memory for: Header + Data + Back Canary
            std::size_t total_size = sizeof(Header) + size + sizeof(std::uint32_t);
            
            // Ensure the underlying allocator aligns at least to the header boundary
            std::size_t actual_align = (alignment > alignof(Header)) ? alignment : alignof(Header);

            Block block = m_allocator.allocate(total_size, actual_align);
            if (!block.ptr) 
                return { nullptr, 0 };

            // 1. Place the front canary
            Header* header = static_cast<Header*>(block.ptr);
            header->user_size = size;
            header->magic = FRONT_MAGIC;

            // 2. Compute the start of the user data
            std::byte* payload = reinterpret_cast<std::byte*>(block.ptr) + sizeof(Header);

            // 3. Place the back canary at the very end.
            // Use memcpy instead of a direct cast to avoid crashes on ARM processors
            // when accessing unaligned memory.
            std::memcpy(payload + size, &BACK_MAGIC, sizeof(BACK_MAGIC));

            return { payload, size };
        }

        void deallocate(void* ptr, std::size_t size) 
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

            // If everything is intact, return the memory to the underlying allocator
            std::size_t total_size = sizeof(Header) + header->user_size + sizeof(std::uint32_t);
            m_allocator.deallocate(header, total_size);
        }
        
        bool owns(const void* ptr) const noexcept 
        {
            if (!ptr) 
                return false;

            const std::byte* payload = static_cast<const std::byte*>(ptr);
            const Header* header = reinterpret_cast<const Header*>(payload - sizeof(Header));
            return m_allocator.owns(header);
        }
    };

} // namespace MemCore