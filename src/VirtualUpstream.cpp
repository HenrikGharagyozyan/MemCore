#include "MemCore/VirtualUpstream.hpp"

#if defined(MEMCORE_OS_WINDOWS)
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

namespace MemCore 
{

    std::size_t VirtualUpstream::query_page_size() noexcept 
    {
#if defined(MEMCORE_OS_WINDOWS)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return static_cast<std::size_t>(si.dwPageSize);
#else
        return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
    }

    VirtualUpstream::VirtualUpstream() noexcept 
        : m_page_size(query_page_size()) 
    {
    }

    Block VirtualUpstream::allocate(std::size_t size, std::size_t /*alignment*/) noexcept 
    {
        if (size == 0) return { nullptr, 0 };

        // The OS always allocates memory in pages, so we round the size up
        std::size_t allocation_size = align_to_page(size);
        void* ptr = nullptr;

#if defined(MEMCORE_OS_WINDOWS)
        // Reserve and immediately commit physical memory (Read/Write)
        ptr = VirtualAlloc(nullptr, allocation_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        // mmap without an associated file (anonymous) to allocate memory in RAM
        ptr = ::mmap(nullptr, allocation_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) 
        {
            return { nullptr, 0 };
        }
#endif

        return { static_cast<std::byte*>(ptr), allocation_size };
    }

    void VirtualUpstream::deallocate(void* ptr, std::size_t size) noexcept 
    {
        if (!ptr || size == 0) return;

        std::size_t allocation_size = align_to_page(size);

#if defined(MEMCORE_OS_WINDOWS)
        // On Windows, when using MEM_RELEASE the size must be 0, and the OS will free the whole region
        VirtualFree(ptr, 0, MEM_RELEASE);
#else
        ::munmap(ptr, allocation_size);
#endif
    }

}