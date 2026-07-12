#include <MemCore/AllocatorConcept.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <iostream>

int main() 
{
    std::cout << "--- Linear Allocator Test ---" << std::endl;

    // 1. Create the upstream source
    MemCore::MallocUpstream upstream;

    // 2. Ask the system for one large chunk of 1 megabyte
    MemCore::Block big_chunk = upstream.allocate(1024 * 1024, 8);

    if (big_chunk.ptr) 
    {
        // 3. Give this chunk to our fast linear allocator
        MemCore::LinearAllocator linear(big_chunk);

        // 4. Allocate memory instantly by just moving the cursor
        MemCore::Block a = linear.allocate(16, 8);
        MemCore::Block b = linear.allocate(32, 16);
        MemCore::Block c = linear.allocate(128, 32);

        std::cout << "Allocated block A at: " << a.ptr << std::endl;
        std::cout << "Allocated block B at: " << b.ptr << std::endl;
        std::cout << "Allocated block C at: " << c.ptr << std::endl;

        // 5. Release everything immediately
        linear.reset();
        std::cout << "Linear allocator reset." << std::endl;

        // 6. Return the megabyte to the system
        upstream.deallocate(big_chunk.ptr, big_chunk.size);
    }

    return 0;
}