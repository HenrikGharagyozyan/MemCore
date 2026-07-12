cmake_minimum_required(VERSION 3.10)
project(MemCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(MemCore INTERFACE)

target_include_directories(MemCore INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_executable(MemCoreTest src/main.cpp)
target_link_libraries(MemCoreTest PRIVATE MemCore)