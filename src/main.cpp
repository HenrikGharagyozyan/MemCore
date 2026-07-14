#include <MemCore/AllocatorConcept.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/StackAllocator.hpp>
#include <iostream>

int main() {
    std::cout << "--- Stack Allocator Test ---" << std::endl;

    MemCore::MallocUpstream upstream;
    MemCore::Block big_chunk = upstream.allocate(1024 * 1024, 8);

    if (big_chunk.ptr) {
        MemCore::StackAllocator stack_alloc(big_chunk);

        // 1. Allocate a persistent object
        MemCore::Block a = stack_alloc.allocate(16, 8);
        std::cout << "Allocated A at: " << a.ptr << std::endl;
        
        // 2. Set a marker before temporary objects
        MemCore::StackAllocator::Marker marker = stack_alloc.get_marker();
        
        // 3. Allocate temporary objects
        MemCore::Block b = stack_alloc.allocate(16, 8);
        MemCore::Block c = stack_alloc.allocate(16, 8);
        std::cout << "Allocated B at: " << b.ptr << std::endl;
        std::cout << "Allocated C at: " << c.ptr << std::endl;

        // Deallocate C and allocate D in its place to verify LIFO
        std::cout << "Deallocating C..." << std::endl;
        stack_alloc.deallocate(c.ptr, c.size);
        
        MemCore::Block d = stack_alloc.allocate(16, 8);
        std::cout << "Allocated D (after C freed) at: " << d.ptr << " (Should match C)" << std::endl;
        
        // 4. Marker magic: free both D and B at once by rolling back to the marker
        std::cout << "Rolling back to marker (frees D and B)..." << std::endl;
        stack_alloc.free_to_marker(marker);

        // To prove the memory was freed correctly, allocate a new block E.
        // It should end up at the same location where block B was originally.
        MemCore::Block e = stack_alloc.allocate(16, 8);
        std::cout << "Allocated E (after rollback) at: " << e.ptr << " (Should match B)" << std::endl;

        // Clean up
        stack_alloc.reset();
        upstream.deallocate(big_chunk.ptr, big_chunk.size);
        std::cout << "All clean!" << std::endl;
    }

    return 0;
}