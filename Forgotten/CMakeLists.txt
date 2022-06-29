cmake_minimum_required(VERSION 3.22)
project(Forgotten)

find_package(Vulkan REQUIRED)

add_subdirectory(vendor/imgui)
add_subdirectory(vendor/glfw)
add_subdirectory(vendor/stb)

file(GLOB sources include/*.hpp src/*.cpp src/imgui/*.cpp test/*.hpp test/*.cpp)

add_library(Forgotten STATIC ${sources})
target_include_directories(Forgotten PUBLIC
    include
    /usr/local/include
    /opt/homebrew/include
    ${Vulkan_INCLUDE_DIRS}
    vendor/glm
    vendor/imgui
    vendor/glfw/include
    vendor/stb)
target_link_libraries(Forgotten PUBLIC ${Vulkan_LIBRARIES} imgui glfw)
