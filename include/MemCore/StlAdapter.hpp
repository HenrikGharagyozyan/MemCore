#pragma once
#include "Types.hpp"
#include "AllocatorConcept.hpp"

#include <cstddef>
#include <new>


namespace MemCore 
{

    // Template adapter that wraps ANY of our allocators
    // into the standard C++ allocator interface.
    template <typename T, Allocator AllocatorType>
    class StlAdapter
    {
        // Every StlAdapter instantiation over the same allocator is one family:
        // rebinding (StlAdapter<U, A>) and comparison need to read each other's
        // pointer, which is why they are friends rather than the member public.
        template <typename U, Allocator A>
        friend class StlAdapter;

        AllocatorType* m_allocator;

    public:
        using value_type = T;

        // Constructor takes a reference to our allocator
        explicit StlAdapter(AllocatorType& allocator) noexcept
            : m_allocator(&allocator)
        {
        }

        // Template copy constructor. It is required for the STL.
        // When std::list wants to allocate memory for its internal Node<T>,
        // it will ask the adapter to rebuild itself for another type (rebind).
        template <typename U>
        StlAdapter(const StlAdapter<U, AllocatorType>& other) noexcept 
            : m_allocator(other.m_allocator) 
        {
        }

        // Memory allocation method according to the STL standard
        T* allocate(std::size_t n) 
        {
            if (n == 0) 
                return nullptr;

            // Convert element count to bytes and request alignment for type T
            Block block = m_allocator->allocate(n * sizeof(T), alignof(T));
            if (!block.ptr) 
            {
                throw std::bad_alloc();
            }

            return static_cast<T*>(block.ptr);
        }

        // Memory deallocation method according to the STL standard
        void deallocate(T* p, std::size_t n) noexcept 
        {
            if (!p) 
                return;
            m_allocator->deallocate(p, n * sizeof(T));
        }

        // Allocator comparison. In C++, two stateful allocators are equal
        // only if they manage the same memory.
        template <typename U>
        bool operator==(const StlAdapter<U, AllocatorType>& other) const noexcept 
        {
            return m_allocator == other.m_allocator;
        }

        template <typename U>
        bool operator!=(const StlAdapter<U, AllocatorType>& other) const noexcept 
        {
            return m_allocator != other.m_allocator;
        }
    };

}