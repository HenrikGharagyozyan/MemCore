#pragma once
#include "AllocatorConcept.hpp"

// <memory_resource> is C++17, but some standard libraries shipped it late.
// Guard so MemCore still builds where it is unavailable.
#if defined(__has_include)
#  if __has_include(<memory_resource>)
#    define MEMCORE_HAS_PMR 1
#  endif
#endif

#ifdef MEMCORE_HAS_PMR

#include <memory_resource>
#include <cstddef>
#include <new>

namespace MemCore
{

    // Adapts any MemCore Allocator into a std::pmr::memory_resource, so every
    // std::pmr container (vector, string, unordered_map, ...) can allocate from
    // it without being templated on the allocator type:
    //
    //     MemCore::LinearAllocator arena(chunk);
    //     MemCore::PmrAdapter resource(arena);
    //     std::pmr::vector<int> v(&resource);
    //
    // This is the boundary where MemCore's "return nullptr on exhaustion" rule
    // meets the pmr contract, which requires throwing std::bad_alloc instead.
    template <Allocator Alloc>
    class PmrAdapter final : public std::pmr::memory_resource
    {
    public:
        explicit PmrAdapter(Alloc& allocator) noexcept
            : m_allocator(&allocator)
        {
        }

        [[nodiscard]] Alloc& allocator() const noexcept { return *m_allocator; }

    private:
        Alloc* m_allocator;

        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            // pmr permits a zero-byte request and expects a usable pointer back,
            // whereas MemCore allocators return nullptr for size 0. Ask for one
            // byte so the distinct-pointer expectation still holds.
            if (bytes == 0)
                bytes = 1;

            Block block = m_allocator->allocate(bytes, alignment);
            if (!block.ptr)
                throw std::bad_alloc(); // required by the pmr contract

            return block.ptr;
        }

        void do_deallocate(void* ptr, std::size_t bytes, std::size_t /*alignment*/) override
        {
            // pmr guarantees `bytes` matches the original request, which is
            // exactly what MemCore's sized-deallocation contract needs.
            m_allocator->deallocate(ptr, bytes == 0 ? 1 : bytes);
        }

        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
        {
            // Memory from this resource can only be returned to a resource
            // backed by the very same allocator instance.
            const auto* rhs = dynamic_cast<const PmrAdapter*>(&other);
            return rhs != nullptr && rhs->m_allocator == m_allocator;
        }
    };

} // namespace MemCore

#endif // MEMCORE_HAS_PMR
