cmake_minimum_required(VERSION 3.21)
project(ForgottenEngine)

include(../cmake_utils/common/common.cmake)

find_package(Vulkan REQUIRED)

find_package(
  Boost 1.79
  COMPONENTS program_options
  REQUIRED)

file(
  GLOB_RECURSE
  sources
  fg_pch.hpp
  include/**.hpp
  src/**.cpp
  test/*.hpp
  test/*.cpp
  vendor/yaml-cpp/src/**.cpp
  vendor/yaml-cpp/src/**.h,
  vendor/yaml-cpp/include/**.h,
  vendor/VulkanMemoryAllocator/include/vk_mem_alloc.h,
  vendor/VulkanMemoryAllocator/src/VmaUsage.h,
  vendor/VulkanMemoryAllocator/src/VmaUsage.cpp,)

if(WIN32)
  list(FILTER sources EXCLUDE REGEX "MacOS")
  list(FILTER sources EXCLUDE REGEX "Linux")
endif()

if(APPLE)
  list(FILTER sources EXCLUDE REGEX "Windows")
  list(FILTER sources EXCLUDE REGEX "Linux")
endif()

set(MSDF_ATLAS_GEN_MSDFGEN_EXTERNAL 0)

set_directory_properties(PROPERTIES COMPILE_DEFINITIONS SHOW_MESSAGE=1)
add_subdirectory(vendor)

find_program(CLANG_FORMAT "clang-format")

if(CLANG_FORMAT)
  set(formattable_files ${sources})

  list(FILTER formattable_files EXCLUDE REGEX "yaml-cpp")

  add_custom_target(clang-format COMMAND ${CLANG_FORMAT} -i -style=file
                                         ${formattable_files})
endif()

add_library(ForgottenEngine STATIC ${sources})

target_compile_definitions(ForgottenEngine PRIVATE FORGOTTEN_TRACK_MEMORY)

if(APPLE)
  target_compile_options(ForgottenEngine PUBLIC -Wno-nullability-completeness
                                                -fcolor-diagnostics)
endif()

target_include_directories(
  ForgottenEngine
  PUBLIC . ${Boost_INCLUDE_DIRS}
  PRIVATE include
          src
          vendor/assimp/include
          vendor/imgui
          vendor/yaml-cpp/include
          vendor/msdf-atlas-gen/msdf-atlas-gen
          vendor/msdf-atlas-gen/msdfgen
          /usr/local/include
          /opt/homebrew/include
          /opt/homebrew/Cellar
          vendor/VulkanMemoryAllocator/include
          vendor/glm
          vendor/spdlog/include
          vendor/vk_init
          vendor/glfw/include
          vendor/tinyobjloader
          vendor/stb
          vendor/spirv-cross
          vendor/shaderc
          vendor/shaderc/libshaderc/include
          vendor/shaderc/libshaderc_util/include
          ${Vulkan_INCLUDE_DIRS})

target_link_libraries(
  ForgottenEngine
  PRIVATE ${Vulkan_LIBRARIES}
          glfw
          tinyobjloader
          imgui
          msdf-atlas-gen
          assimp
          shaderc
          spirv-cross-glsl
          spirv-cross-core
          ${minizip}
          Boost::program_options)

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
    ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:ForgottenEngine>/resources)

add_dependencies(ForgottenEngine clang-format)
