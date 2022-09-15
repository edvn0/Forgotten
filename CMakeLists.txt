cmake_minimum_required(VERSION 3.21)
project(Forgotten CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(${USE_ALTERNATE_LINKER} STREQUAL "")
  add_link_options("-fuse-ld=${USE_ALTERNATE_LINKER}")
endif()

add_subdirectory(ForgottenEngine)
add_subdirectory(ForgottenApp)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cli_defaults.yml
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ForgottenApp)
