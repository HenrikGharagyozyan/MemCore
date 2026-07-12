#include <MemCore/AllocatorConcept.hpp>
#include <MemCore/Align.hpp>
#include <iostream>

int main() 
{
    // Take a misaligned address (for example, 1003)
    void* bad_ptr = reinterpret_cast<void*>(1003);
    
    // Try to align it to an 8-byte boundary
    void* aligned_ptr = MemCore::AlignForward(bad_ptr, 8);

    std::cout << "MemCore Sandbox Running!" << std::endl;
    std::cout << "Original address: " << bad_ptr << std::endl;
    std::cout << "Aligned (8 bytes): " << aligned_ptr << std::endl;

    // Verify it with our own function
    if (MemCore::IsAligned(aligned_ptr, 8)) 
    {
        std::cout << "Alignment check: SUCCESS!" << std::endl;
    } 
    else 
    {
        std::cout << "Alignment check: FAILED!" << std::endl;
    }

    return 0;
}