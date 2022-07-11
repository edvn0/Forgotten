cmake_minimum_required(VERSION 3.22)
project(ForgottenApp)

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

foreach(shader_file IN LISTS SHADERS)
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
endforeach()

add_custom_target(CompileClientShaders ALL DEPENDS ${SPV_SHADERS})

# ----------------------------------------------
# End
# ----------------------------------------------
file(GLOB_RECURSE sources
    include/**.hpp
    src/**.cpp
    test/*.hpp test/*.cpp)

add_executable(ForgottenApp ${sources})
target_include_directories(ForgottenApp PUBLIC
    include src
    /usr/local/include
    /opt/homebrew/include
    ${Vulkan_INCLUDE_DIRS}
    ../ForgottenEngine/vendor/VulkanMemoryAllocator/include .
    ../ForgottenEngine/vendor/glm
    ../ForgottenEngine/vendor/spdlog/include
    ../ForgottenEngine/vendor/vk_init
    ../ForgottenEngine/vendor/glfw/include
    ../ForgottenEngine/vendor/vk-bootstrap/src
    ../ForgottenEngine/vendor/tinyobjloader
    ../ForgottenEngine/vendor/stb
    ../ForgottenEngine/vendor/imgui
)
target_link_libraries(ForgottenApp PUBLIC ForgottenEngine)
target_compile_options(ForgottenApp PUBLIC -Wno-nullability-completeness)

add_custom_command(TARGET CompileClientShaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_BINARY_DIR}/resources/shaders $<TARGET_FILE_DIR:ForgottenApp>/resources/shaders)

add_dependencies(ForgottenApp CompileClientShaders)
