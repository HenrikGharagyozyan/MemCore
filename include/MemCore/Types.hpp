#pragma once
#include <cstddef> // Для std::size_t

namespace MemCore 
{

    // Structure describing a memory block that has been allocated
    struct Block 
    {
        void* ptr;
        std::size_t size;
    };

}