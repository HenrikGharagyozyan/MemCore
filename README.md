[![CMake Build and Test](https://github.com/YOUR_USERNAME/MemCore/actions/workflows/cmake.yml/badge.svg)](https://github.com/YOUR_USERNAME/MemCore/actions/workflows/cmake.yml)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

# 🚀 MemCore

**MemCore** is a lightweight, high-performance **header-only C++20 memory allocation library** designed for performance-critical applications such as:

- 🎮 Game engines
- ⚡ Real-time systems
- 🖥️ Embedded applications
- 📊 Simulation software
- 🔥 High-performance C++ applications

The goal of MemCore is to provide specialized allocators that reduce allocation overhead, minimize fragmentation, and give developers explicit control over memory lifetime.

Instead of relying on frequent `malloc/free` or `new/delete` calls, MemCore allows allocating memory using predictable and optimized strategies.

---

## ✨ Features

### Header-only

No compilation step required.

Simply include the headers:

```cpp
#include <MemCore/LinearAllocator.hpp>
```

and start using the allocators.

### STL Compatible

MemCore provides `StlAdapter`, allowing custom allocators to be used with standard containers:

```cpp
std::vector<int, MemCore::StlAdapter<int, Allocator>>
```

Compatible with:

- `std::vector`
- `std::list`
- `std::deque`
- `std::unordered_map`
- and other STL containers

### Memory Safety

All allocators handle:

- alignment requirements
- correct pointer arithmetic
- lifetime management
- bounds checking where applicable

### Memory Profiling

`TrackerAllocator` provides allocation statistics:

- total allocated memory
- allocation count
- peak memory usage
- leak detection support

Example:

```cpp
tracker.print_stats();
```

## 📦 Allocators

| Allocator | Allocation | Deallocation | Fragmentation | Use Case |
|---|---|---|---|---|
| LinearAllocator | O(1) | O(1) reset | None | Frame memory, temporary data |
| StackAllocator | O(1) | O(1) LIFO | None | Scoped allocations |
| PoolAllocator | O(1) | O(1) | None | Objects with identical size |
| ArenaAllocator | O(1) amortized | O(1) reset | None | Growing collections |
| TrackerAllocator | Depends on upstream | Depends on upstream | Depends on upstream | Debugging and profiling |

## 🏗️ Architecture

MemCore follows a layered allocator design:

```
                User Code
                    |
                    |
             STL Adapter Layer
                    |
                    |
            Specialized Allocators
                    |
        +-----------+-----------+
        |           |           |
     Linear      Pool       Arena
        |
        |
   Memory Source
        |
        |
   MallocUpstream
```

Allocators are independent and can be composed together.

Example:

```
MallocUpstream
        |
LinearAllocator
        |
TrackerAllocator
        |
Application
```

## 📥 Installation

### Option 1: CMake FetchContent (Recommended)

Add:

```cmake
include(FetchContent)

FetchContent_Declare(
    MemCore
    GIT_REPOSITORY https://github.com/YOUR_USERNAME/MemCore.git
    GIT_TAG main
)

FetchContent_MakeAvailable(MemCore)

target_link_libraries(MyProject PRIVATE MemCore)
```

### Option 2: Git Submodule

```bash
git submodule add https://github.com/YOUR_USERNAME/MemCore.git third_party/MemCore
```

Then:

```cmake
add_subdirectory(third_party/MemCore)

target_link_libraries(MyProject PRIVATE MemCore)
```

## ⚡ Quick Start

### Linear Allocator

```cpp
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/MallocUpstream.hpp>

int main()
{
    MemCore::MallocUpstream upstream;

    auto block = upstream.allocate(1024, 8);

    MemCore::LinearAllocator allocator(block);

    auto memory = allocator.allocate(sizeof(int), alignof(int));

    int* value = static_cast<int*>(memory.ptr);

    *value = 42;

    allocator.reset();

    upstream.deallocate(block.ptr, block.size);
}
```

## 📊 Tracking Allocations

```cpp
#include <MemCore/TrackerAllocator.hpp>

MemCore::TrackerAllocator tracker(allocator);

auto block = tracker.allocate(256, 8);

tracker.print_stats();
```

Example output:

```
===== Memory Statistics =====

Allocations : 128
Current     : 4096 bytes
Peak        : 8192 bytes

=============================
```

## 🔥 STL Integration

Example using `std::vector`:

```cpp
#include <vector>

#include <MemCore/LinearAllocator.hpp>
#include <MemCore/StlAdapter.hpp>


void process_frame(MemCore::Block memory)
{
    MemCore::LinearAllocator allocator(memory);

    using Alloc = MemCore::StlAdapter<int,
                                      MemCore::LinearAllocator>;

    std::vector<int, Alloc> data(
        Alloc(allocator)
    );


    data.push_back(10);
    data.push_back(20);
    data.push_back(30);
}
```

The container now allocates memory from the custom allocator.

## 🎯 Design Goals

MemCore focuses on:

- ✅ predictable allocation time
- ✅ minimal fragmentation
- ✅ explicit memory lifetime control
- ✅ composable allocator architecture
- ✅ modern C++20 design

## 🧪 Testing

Build:

```bash
cmake -B build
cmake --build build
```

Run tests:

```bash
ctest --test-dir build
```

## 📁 Project Structure

```
MemCore/
|
├── include/
│   └── MemCore/
│       ├── LinearAllocator.hpp
│       ├── StackAllocator.hpp
│       ├── PoolAllocator.hpp
│       ├── ArenaAllocator.hpp
│       ├── TrackerAllocator.hpp
│       └── StlAdapter.hpp
|
├── tests/
|
├── examples/
|
├── CMakeLists.txt
|
└── LICENSE
```

## 📚 Requirements

C++20 compatible compiler

Supported:

- GCC 11+
- Clang 14+
- MSVC 2022+

## 📜 License

MemCore is licensed under the MIT License.

See [LICENSE](LICENSE) for details.