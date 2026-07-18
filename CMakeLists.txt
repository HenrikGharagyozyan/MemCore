cmake_minimum_required(VERSION 3.21)
project(MemCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

add_library(MemCore STATIC
    src/VirtualUpstream.cpp
)
add_library(MemCore::MemCore ALIAS MemCore)

target_include_directories(MemCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ThreadSafeAllocator uses std::mutex; propagate the platform thread
# requirement so consumers link it transitively.
find_package(Threads REQUIRED)
target_link_libraries(MemCore PUBLIC Threads::Threads)

# ==========================================
# INSTALL / EXPORT  -> enables find_package(MemCore)
# ==========================================
# Off when consumed as a subproject so a parent's install isn't polluted.
option(MEMCORE_INSTALL "Generate MemCore install & package-config rules" ${PROJECT_IS_TOP_LEVEL})

if(MEMCORE_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS MemCore
        EXPORT MemCoreTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT MemCoreTargets
        FILE MemCoreTargets.cmake
        NAMESPACE MemCore::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MemCore
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MemCoreConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/MemCoreConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MemCore
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/MemCoreConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/MemCoreConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/MemCoreConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MemCore
    )
endif()

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

    target_link_libraries(MemCore_UnitTests PRIVATE MemCore GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(MemCore_UnitTests)
endif()

# ==========================================
# BENCHMARKS
# ==========================================
# Off by default: they need a Release build and pull in Google Benchmark.
option(MEMCORE_BUILD_BENCHMARKS "Build the MemCore benchmarks" OFF)

if(MEMCORE_BUILD_BENCHMARKS)
    include(FetchContent)
    FetchContent_Declare(
      googlebenchmark
      GIT_REPOSITORY https://github.com/google/benchmark.git
      GIT_TAG        v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(MemCore_Benchmarks benchmarks/AllocatorBench.cpp)
    target_link_libraries(MemCore_Benchmarks PRIVATE MemCore benchmark::benchmark)
endif()
