cmake_minimum_required(VERSION 3.21)
project(ForgottenApp)

include(../cmake_utils/common/common.cmake)

file(GLOB_RECURSE sources include/**.hpp src/**.cpp test/*.hpp test/*.cpp)

add_executable(ForgottenApp ${sources})
target_include_directories(ForgottenApp PUBLIC . include src /usr/local/include
                                               /opt/homebrew/include)
target_link_libraries(ForgottenApp PUBLIC ForgottenEngine)

if(MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT
                                                              ForgottenApp)
  set_property(
    TARGET ForgottenApp
    PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
             "${CMAKE_BINARY_DIR}/ForgottenApp/${CMAKE_BUILD_TYPE}")
endif()

if(APPLE)
  target_compile_options(ForgottenApp PUBLIC -Wno-nullability-completeness)
endif()

add_custom_command(
  TARGET ForgottenApp
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:ForgottenApp>/resources)
