#include <MemCore/AllocatorConcept.hpp>
#include <MemCore/MallocUpstream.hpp>
#include <MemCore/Align.hpp>
#include <iostream>

int main() 
{
    std::cout << "--- MemCore Sandbox ---" << std::endl;

    MemCore::MallocUpstream upstream;

    MemCore::Block block = upstream.allocate(100, 32);

    if (block.ptr) 
    {
        std::cout << "Allocated " << block.size << " bytes at address: " << block.ptr << std::endl;
        
        if (MemCore::IsAligned(block.ptr, 32)) 
        {
            std::cout << "SUCCESS: Pointer is perfectly aligned to 32 bytes!" << std::endl;
        } 
        else 
        {
            std::cout << "ERROR: Pointer alignment failed!" << std::endl;
        }

        upstream.deallocate(block.ptr, block.size);
    } 
    else 
    {
        std::cout << "Allocation failed!" << std::endl;
    }

    return 0;
}