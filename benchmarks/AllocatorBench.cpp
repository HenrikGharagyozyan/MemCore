// Benchmarks comparing MemCore allocators against malloc and std::pmr.
//
// Two workloads:
//   1. Transient burst  - allocate many objects, then release them all at once
//      (where bump/arena strategies shine over per-object free).
//   2. Fixed-size churn  - repeatedly allocate and free a single object
//      (the pool's home turf).
//
// Build:  cmake -B build -DCMAKE_BUILD_TYPE=Release -DMEMCORE_BUILD_BENCHMARKS=ON
// Run:    ./build/MemCore_Benchmarks

#include <benchmark/benchmark.h>

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/PoolAllocator.hpp>

#include <cstdlib>
#include <cstddef>
#include <memory_resource>
#include <vector>

namespace
{
    constexpr std::size_t kObjSize   = 64;
    constexpr std::size_t kObjAlign  = alignof(std::max_align_t);
    constexpr std::size_t kBurst     = 10'000;
    // Room for the burst plus alignment slack.
    constexpr std::size_t kArenaSize = kBurst * (kObjSize + kObjAlign);
}

// --- Workload 1: transient burst then bulk release -------------------------

static void BM_Burst_Linear(benchmark::State& state)
{
    std::vector<std::byte> buffer(kArenaSize);
    MemCore::LinearAllocator lin(MemCore::Block{ buffer.data(), buffer.size() });

    for (auto _ : state)
    {
        for (std::size_t i = 0; i < kBurst; ++i)
            benchmark::DoNotOptimize(lin.allocate(kObjSize, kObjAlign).ptr);
        lin.reset();
    }
    state.SetItemsProcessed(state.iterations() * kBurst);
}
BENCHMARK(BM_Burst_Linear);

static void BM_Burst_PmrMonotonic(benchmark::State& state)
{
    std::vector<std::byte> buffer(kArenaSize);

    for (auto _ : state)
    {
        std::pmr::monotonic_buffer_resource res(buffer.data(), buffer.size());
        for (std::size_t i = 0; i < kBurst; ++i)
            benchmark::DoNotOptimize(res.allocate(kObjSize, kObjAlign));
        // res destructor releases the whole buffer at once
    }
    state.SetItemsProcessed(state.iterations() * kBurst);
}
BENCHMARK(BM_Burst_PmrMonotonic);

static void BM_Burst_Malloc(benchmark::State& state)
{
    std::vector<void*> ptrs(kBurst);

    for (auto _ : state)
    {
        for (std::size_t i = 0; i < kBurst; ++i)
        {
            ptrs[i] = std::malloc(kObjSize);
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (std::size_t i = 0; i < kBurst; ++i)
            std::free(ptrs[i]);
    }
    state.SetItemsProcessed(state.iterations() * kBurst);
}
BENCHMARK(BM_Burst_Malloc);

// --- Workload 2: fixed-size allocate/free churn ----------------------------

static void BM_Churn_Pool(benchmark::State& state)
{
    std::vector<std::byte> buffer(kArenaSize);
    MemCore::PoolAllocator pool(MemCore::Block{ buffer.data(), buffer.size() }, kObjSize, kObjAlign);

    for (auto _ : state)
    {
        MemCore::Block b = pool.allocate(kObjSize, kObjAlign);
        benchmark::DoNotOptimize(b.ptr);
        pool.deallocate(b.ptr, b.size);
    }
}
BENCHMARK(BM_Churn_Pool);

static void BM_Churn_Malloc(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* p = std::malloc(kObjSize);
        benchmark::DoNotOptimize(p);
        std::free(p);
    }
}
BENCHMARK(BM_Churn_Malloc);

BENCHMARK_MAIN();
