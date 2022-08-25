cmake_minimum_required(VERSION 3.21)
project(Forgotten CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(ForgottenEngine)
add_subdirectory(ForgottenApp)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cli_defaults.yml
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ForgottenApp)
