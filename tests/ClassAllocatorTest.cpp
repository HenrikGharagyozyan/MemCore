#include <gtest/gtest.h>
#include <MemCore/ClassAllocator.hpp>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>

// 1. Создаем глобальный/доступный контекст для аллокатора (как в реальном движке)
class ParticleMemorySystem 
{
public:
    static MemCore::Block buffer;
    static MemCore::PoolAllocator* pool;
};

// Выделяем память под статические переменные теста
MemCore::Block ParticleMemorySystem::buffer = { nullptr, 0 };
MemCore::PoolAllocator* ParticleMemorySystem::pool = nullptr;


// 2. Тестовый класс, который будет использовать наш макрос
class Particle 
{
public:
    float position[3];
    float velocity[3];
    int lifetime;

    // Внедряем наш аллокатор!
    MEMCORE_ENABLE_CLASS_ALLOCATOR(Particle, *ParticleMemorySystem::pool)
};


TEST(ClassAllocatorTest, OverloadedNewUsesPool) 
{
    MemCore::MallocUpstream system_malloc;
    
    // Выделяем буфер на 10 частиц
    ParticleMemorySystem::buffer = system_malloc.allocate(sizeof(Particle) * 10, alignof(Particle));
    MemCore::PoolAllocator particle_pool(ParticleMemorySystem::buffer, sizeof(Particle), alignof(Particle));
    ParticleMemorySystem::pool = &particle_pool;

    {
        // 3. Вызываем ОБЫЧНЫЙ new. Никаких placement new или хелперов!
        Particle* p1 = new Particle();
        
        // Проверяем, что объект создался и память принадлежит нашему пулу
        ASSERT_NE(p1, nullptr);
        EXPECT_TRUE(particle_pool.owns(p1));

        Particle* p2 = new Particle();
        EXPECT_TRUE(particle_pool.owns(p2));
        EXPECT_NE(p1, p2); // Указатели должны быть разными

        // 4. Вызываем ОБЫЧНЫЙ delete.
        delete p1;
        delete p2;
    }

    system_malloc.deallocate(ParticleMemorySystem::buffer.ptr, ParticleMemorySystem::buffer.size);
    ParticleMemorySystem::pool = nullptr;
}