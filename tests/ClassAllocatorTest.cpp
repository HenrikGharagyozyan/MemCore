#include <gtest/gtest.h>
#include <MemCore/ClassAllocator.hpp>
#include <MemCore/PoolAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>

// 1. Create a global, accessible allocator context (like in a real engine)
class ParticleMemorySystem 
{
public:
    static MemCore::Block buffer;
    static MemCore::PoolAllocator* pool;
};

// Allocate storage for the test's static variables
MemCore::Block ParticleMemorySystem::buffer = { nullptr, 0 };
MemCore::PoolAllocator* ParticleMemorySystem::pool = nullptr;


// 2. Test class that will use our macro
class Particle 
{
public:
    float position[3];
    float velocity[3];
    int lifetime;

    // Inject our allocator!
    MEMCORE_ENABLE_CLASS_ALLOCATOR(Particle, *ParticleMemorySystem::pool)
};


TEST(ClassAllocatorTest, OverloadedNewUsesPool) 
{
    MemCore::MallocUpstream system_malloc;
    
    // Allocate a buffer for 10 particles
    ParticleMemorySystem::buffer = system_malloc.allocate(sizeof(Particle) * 10, alignof(Particle));
    MemCore::PoolAllocator particle_pool(ParticleMemorySystem::buffer, sizeof(Particle), alignof(Particle));
    ParticleMemorySystem::pool = &particle_pool;

    {
        // 3. Call regular new. No placement new or helpers!
        Particle* p1 = new Particle();
        
        // Verify that the object was created and that the memory belongs to our pool
        ASSERT_NE(p1, nullptr);
        EXPECT_TRUE(particle_pool.owns(p1));

        Particle* p2 = new Particle();
        EXPECT_TRUE(particle_pool.owns(p2));
        EXPECT_NE(p1, p2); // The pointers should be different

        // 4. Call regular delete.
        delete p1;
        delete p2;
    }

    system_malloc.deallocate(ParticleMemorySystem::buffer.ptr, ParticleMemorySystem::buffer.size);
    ParticleMemorySystem::pool = nullptr;
}