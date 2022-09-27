cmake_minimum_required(VERSION 3.21)
project(ForgottenEngine)

include(../cmake_utils/common/common.cmake)

find_package(Vulkan REQUIRED)

message(STATUS ${Vulkan_LIBRARY})

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

include(../cmake_utils/common/three-operating-systems.cmake)

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

if(FORGOTTEN_OS STREQUAL "MacOS")
  target_compile_definitions(ForgottenEngine PRIVATE FORGOTTEN_MACOS)
endif()

if(FORGOTTEN_OS STREQUAL "Windows")
  target_compile_definitions(ForgottenEngine PRIVATE FORGOTTEN_WINDOWS)
endif()

if(FORGOTTEN_OS STREQUAL "Linux")
  target_compile_definitions(ForgottenEngine PRIVATE FORGOTTEN_LINUX)
endif()

if(APPLE)
  target_compile_options(
    ForgottenEngine PUBLIC -Wno-return-type -Wno-nullability-completeness
                           -fcolor-diagnostics)
endif()

set(Forgotten_INCLUDE_DIRS
    CACHE /usr/local/include
          /opt/homebrew/include
          /opt/homebrew/Cellar
          vendor/assimp/include
          vendor/imgui
          vendor/yaml-cpp/include
          vendor/msdf-atlas-gen/msdf-atlas-gen
          vendor/msdf-atlas-gen/msdfgen
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
          vendor/entt/src/entt
          vendor/entt/single_include
          vendor/box2d/include
          vendor/magic-enum/include)

target_include_directories(
  ForgottenEngine
  PUBLIC .
         include
         src
         ${Boost_INCLUDE_DIRS}
         ${Vulkan_INCLUDE_DIRS}
         vendor/assimp/include
         vendor/imgui
         vendor/yaml-cpp/include
         vendor/msdf-atlas-gen/msdf-atlas-gen
         vendor/msdf-atlas-gen/msdfgen
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
         vendor/entt/src/entt
         vendor/entt/single_include
         vendor/box2d/include
         vendor/magic-enum/include)

target_link_libraries(
  ForgottenEngine
  PUBLIC imgui Vulkan::Vulkan
  PRIVATE glfw
          tinyobjloader
          msdf-atlas-gen
          assimp
          shaderc
          spirv-cross-glsl
          spirv-cross-core
          EnTT::EnTT
          box2d
          freetype
          Boost::program_options)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_definitions(ForgottenEngine PUBLIC -fcolor-diagnostics)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_definitions(ForgottenEngine PUBLIC -fdiagnostics-color=always)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_definitions(ForgottenEngine PUBLIC /wd4715)
endif()

target_precompile_headers(ForgottenEngine PUBLIC fg_pch.hpp)

add_custom_command(
  TARGET ForgottenEngine
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:ForgottenEngine>/resources)

add_dependencies(ForgottenEngine clang-format)
