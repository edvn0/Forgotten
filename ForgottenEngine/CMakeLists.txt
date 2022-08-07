cmake_minimum_required(VERSION 3.21)
project(ForgottenEngine)

find_package(Vulkan REQUIRED)

# ---------------------------------------
# SPIRV Shader Compilation
# ---------------------------------------
find_program(
  glslc_executable
  NAMES glslc
  HINTS Vulkan::glslc)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/resources/shaders)

file(
  GLOB
  SHADERS
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
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
  OUTPUT ${SHADER_BINARY_DIR}
  COMMENT "Creating ${SHADER_BINARY_DIR}")

foreach(shader_file IN LISTS SHADERS)
  get_filename_component(FILENAME ${shader_file} NAME)
  message(STATUS ${shader_file})
  add_custom_command(
    COMMAND ${glslc_executable} -o ${SHADER_BINARY_DIR}/${FILENAME}.spv
            ${shader_file}
    OUTPUT ${SHADER_BINARY_DIR}/${FILENAME}.spv
    DEPENDS ${shader_file} ${SHADER_BINARY_DIR}
    COMMENT "Compiling ${FILENAME}")
  list(APPEND SPV_SHADERS ${SHADER_BINARY_DIR}/${FILENAME}.spv)
endforeach()

add_custom_target(CompileEngineShaders ALL DEPENDS ${SPV_SHADERS})

# ----------------------------------------------
# End
# ----------------------------------------------
file(
  GLOB_RECURSE
  sources
  fg_pch.hpp
  include/**.hpp
  src/**.cpp
  test/*.hpp
  test/*.cpp)

add_subdirectory(vendor/VulkanMemoryAllocator)
add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/glfw)
add_subdirectory(vendor/tinyobjloader)
add_subdirectory(vendor/argparse)
add_subdirectory(vendor/imgui)
add_subdirectory(vendor/msdf-atlas-gen)
add_subdirectory(vendor/spirv-cross)
add_subdirectory(vendor/yaml-cpp)

add_library(ForgottenEngine STATIC ${sources})
target_include_directories(
  ForgottenEngine
  PUBLIC include src vendor/imgui
  PRIVATE /usr/local/include
          /opt/homebrew/include
          ${Vulkan_INCLUDE_DIRS}
          vendor/VulkanMemoryAllocator/include
          .
          vendor/glm
          vendor/spdlog/include
          vendor/vk_init
          vendor/glfw/include
          vendor/tinyobjloader
          vendor/stb
          vendor/argparse/include
          vendor/msdf-atlas-gen/msdf-atlas-gen
          vendor/spirv-cross
          vendor/shaderc
          vendor/yaml-cpp/include
          vendor/shaderc/libshaderc/include
          vendor/shaderc/libshaderc_util/include)
target_link_libraries(
  ForgottenEngine
  PRIVATE ${Vulkan_LIBRARIES}
          VulkanMemoryAllocator
          glfw
          spdlog
          tinyobjloader
          imgui
          argparse
          msdf-atlas-gen
          spirv-cross-core
          spirv-cross-glsl
          spirv-cross-msl
          spirv-cross-reflect
          yaml-cpp)
target_compile_definitions(ForgottenEngine PRIVATE FORGOTTEN_TRACK_MEMORY
                                                   SPIRV_CROSS_STATIC=ON)
target_compile_options(ForgottenEngine PUBLIC -Wno-nullability-completeness
                                              -fcolor-diagnostics)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_definitions(ForgottenEngine PUBLIC -fcolor-diagnostics)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_definitions(ForgottenEngine PUBLIC -fdiagnostics-color=always)
endif()

target_precompile_headers(ForgottenEngine PUBLIC fg_pch.hpp)

add_custom_command(
  TARGET ForgottenEngine
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/resources
    $<TARGET_FILE_DIR:ForgottenEngine>/resources)

add_dependencies(ForgottenEngine CompileEngineShaders)
