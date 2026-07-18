cmake_minimum_required(VERSION 3.21)
project(MemCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(MemCore STATIC
    src/VirtualUpstream.cpp
)

target_include_directories(MemCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ==========================================
# TESTS
# ==========================================
# Off by default when MemCore is consumed as a subproject (add_subdirectory /
# FetchContent) so downstreams don't build googletest or our test suite.
option(MEMCORE_BUILD_TESTS "Build the MemCore unit tests" ${PROJECT_IS_TOP_LEVEL})

if(MEMCORE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    # List EVERY test file here
    add_executable(MemCore_UnitTests
        tests/AlignTest.cpp
        tests/AllocatorConceptTest.cpp
        tests/MallocUpstreamTest.cpp
        tests/LinearAllocatorTest.cpp
        tests/StackAllocatorTest.cpp
        tests/ArenaAllocatorTest.cpp
        tests/PoolAllocatorTest.cpp
        tests/StlAdapterTest.cpp
        tests/TrackerAllocatorTest.cpp
        tests/FallbackAllocatorTest.cpp
        tests/MemoryHelpersTest.cpp
        tests/ThreadSafeAllocatorTest.cpp
        tests/ClassAllocatorTest.cpp
        tests/CanaryAllocatorTest.cpp
        tests/VirtualUpstreamTest.cpp
    )

    find_package(Threads REQUIRED)
    target_link_libraries(MemCore_UnitTests PRIVATE MemCore GTest::gtest_main Threads::Threads)

    include(GoogleTest)
    gtest_discover_tests(MemCore_UnitTests)
endif()
