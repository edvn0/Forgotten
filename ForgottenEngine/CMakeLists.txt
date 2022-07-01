cmake_minimum_required(VERSION 3.22)
project(ForgottenEngine)

find_package(Vulkan REQUIRED)

file(GLOB sources fg_pch.hpp
        include/**/*.hpp
        include/*.hpp
        src/**/*.cpp
        src/*.cpp
        test/*.hpp test/*.cpp)

add_subdirectory(vendor/VulkanMemoryAllocator vendor/glfw)

add_library(ForgottenEngine STATIC ${sources})
target_include_directories(ForgottenEngine PUBLIC
    include src
    /usr/local/include
    /opt/homebrew/include
    ${Vulkan_INCLUDE_DIRS}
    vendor/VulkanMemoryAllocator/include .
        vendor/glm)
target_link_libraries(ForgottenEngine PUBLIC ${Vulkan_LIBRARIES} VulkanMemoryAllocator glfw)
target_compile_options(ForgottenEngine PRIVATE -Wnullability-completeness)
target_precompile_headers(ForgottenEngine PUBLIC fg_pch.hpp)
