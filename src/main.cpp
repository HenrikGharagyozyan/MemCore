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

        // 1. Выделяем постоянный объект
        MemCore::Block a = stack_alloc.allocate(16, 8);
        std::cout << "Allocated A at: " << a.ptr << std::endl;
        
        // 2. Ставим маркер ПЕРЕД временными объектами
        MemCore::StackAllocator::Marker marker = stack_alloc.get_marker();
        
        // 3. Выделяем временные объекты
        MemCore::Block b = stack_alloc.allocate(16, 8);
        MemCore::Block c = stack_alloc.allocate(16, 8);
        std::cout << "Allocated B at: " << b.ptr << std::endl;
        std::cout << "Allocated C at: " << c.ptr << std::endl;

        // Точечно удалим C и выделим D на его месте, чтобы проверить LIFO
        std::cout << "Deallocating C..." << std::endl;
        stack_alloc.deallocate(c.ptr, c.size);
        
        MemCore::Block d = stack_alloc.allocate(16, 8);
        std::cout << "Allocated D (after C freed) at: " << d.ptr << " (Should match C)" << std::endl;
        
        // 4. Магия маркера: удаляем разом и D, и B, просто откатываясь к закладке
        std::cout << "Rolling back to marker (frees D and B)..." << std::endl;
        stack_alloc.free_to_marker(marker);

        // Чтобы доказать, что память освободилась корректно, выделим новый блок E.
        // Он должен оказаться на том же месте, где изначально был блок B.
        MemCore::Block e = stack_alloc.allocate(16, 8);
        std::cout << "Allocated E (after rollback) at: " << e.ptr << " (Should match B)" << std::endl;

        // Чистим за собой
        stack_alloc.reset();
        upstream.deallocate(big_chunk.ptr, big_chunk.size);
        std::cout << "All clean!" << std::endl;
    }

    return 0;
}