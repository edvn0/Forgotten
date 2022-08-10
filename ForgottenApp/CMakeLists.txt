cmake_minimum_required(VERSION 3.21)
project(ForgottenApp)

include(../cmake_utils/common/common.cmake)

find_package(Vulkan REQUIRED)
find_package(
  Boost 1.79
  COMPONENTS program_options
  REQUIRED)

file(GLOB_RECURSE sources include/**.hpp src/**.cpp test/*.hpp test/*.cpp)

add_executable(ForgottenApp ${sources})
target_include_directories(
  ForgottenApp
  PUBLIC .
         include
         src
         /usr/local/include
         /opt/homebrew/include
         ${Vulkan_INCLUDE_DIRS}
         ${Boost_INCLUDE_DIRS}
         ../ForgottenEngine/include
         ../ForgottenEngine/src
         ../ForgottenEngine/
         ../ForgottenEngine/vendor/VulkanMemoryAllocator/include
         ../ForgottenEngine/vendor/glm
         ../ForgottenEngine/vendor/spdlog/include
         ../ForgottenEngine/vendor/vk_init
         ../ForgottenEngine/vendor/glfw/include
         ../ForgottenEngine/vendor/tinyobjloader
         ../ForgottenEngine/vendor/stb
         ../ForgottenEngine/vendor/imgui
         ../ForgottenEngine/vendor/argparse/include
         ../ForgottenEngine/vendor/yaml-cpp/include
         ../ForgottenEngine/vendor/msdf-atlas-gen/msdf-atlas-gen
         ../ForgottenEngine/vendor/msdf-atlas-gen/msdfgen/include)
target_link_libraries(ForgottenApp PUBLIC ForgottenEngine)

if(APPLE)
  target_compile_options(ForgottenApp PUBLIC -Wno-nullability-completeness)
endif()

add_custom_command(
  TARGET ForgottenApp
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:ForgottenApp>/resources)
