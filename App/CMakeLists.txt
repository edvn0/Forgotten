cmake_minimum_required(VERSION 3.22)
project(App)

add_executable(App src/main.cpp src/Renderer.cpp)
target_include_directories(App PRIVATE ../Forgotten/include)
target_link_libraries(App Forgotten)
