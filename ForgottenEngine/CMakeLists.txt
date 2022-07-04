cmake_minimum_required(VERSION 3.22)
project(ForgottenEngine)

find_package(Vulkan REQUIRED)

# ---------------------------------------
# SPIRV Shader Compilation
# ---------------------------------------
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/resources/shaders)

file(GLOB SHADERS
        ${SHADER_SOURCE_DIR}/*.vert
        ${SHADER_SOURCE_DIR}/*.frag
        ${SHADER_SOURCE_DIR}/*.comp
        ${SHADER_SOURCE_DIR}/*.geom
        ${SHADER_SOURCE_DIR}/*.tesc
        ${SHADER_SOURCE_DIR}/*.tese
        ${SHADER_SOURCE_DIR}/*.mesh
        ${SHADER_SOURCE_DIR}/*.task
        ${SHADER_SOURCE_DIR}/*.rgen
        ${SHADER_SOURCE_DIR}/*.rchit
        ${SHADER_SOURCE_DIR}/*.rmiss)

add_custom_command(
        COMMAND
        ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
        OUTPUT ${SHADER_BINARY_DIR}
        COMMENT "Creating ${SHADER_BINARY_DIR}"
)

foreach (shader_file IN LISTS SHADERS)
    get_filename_component(FILENAME ${shader_file} NAME)
    message(STATUS ${shader_file})
    add_custom_command(
            COMMAND
            ${glslc_executable}
            -o ${SHADER_BINARY_DIR}/${FILENAME}.spv
            ${shader_file}
            OUTPUT ${SHADER_BINARY_DIR}/${FILENAME}.spv
            DEPENDS ${shader_file} ${SHADER_BINARY_DIR}
            COMMENT "Compiling ${FILENAME}"
    )
    list(APPEND SPV_SHADERS ${SHADER_BINARY_DIR}/${FILENAME}.spv)
endforeach ()

add_custom_target(CompileShaders ALL DEPENDS ${SPV_SHADERS})

# ----------------------------------------------
# End
# ----------------------------------------------
file(GLOB sources fg_pch.hpp
        include/**/*.hpp
        include/*.hpp
        src/**/*.cpp
        src/*.cpp
        test/*.hpp test/*.cpp)

add_subdirectory(vendor/VulkanMemoryAllocator)
add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/glfw)
add_subdirectory(vendor/vk-bootstrap)

add_executable(ForgottenEngine main.cpp ${sources})
target_include_directories(ForgottenEngine PUBLIC
        include src
        /usr/local/include
        /opt/homebrew/include
        ${Vulkan_INCLUDE_DIRS}
        vendor/VulkanMemoryAllocator/include .
        vendor/glm
        vendor/spdlog/include
        vendor/vk_init
        vendor/glfw/include
        vendor/vk-bootstrap/src
        )
target_link_libraries(ForgottenEngine PUBLIC ${Vulkan_LIBRARIES} VulkanMemoryAllocator glfw spdlog vk-bootstrap)
target_compile_options(ForgottenEngine PUBLIC -Wno-nullability-completeness)
target_precompile_headers(ForgottenEngine PUBLIC fg_pch.hpp)

add_custom_command(TARGET CompileShaders PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/resources/shaders $<TARGET_FILE_DIR:ForgottenEngine>/resources/shaders)

add_dependencies(ForgottenEngine CompileShaders)
