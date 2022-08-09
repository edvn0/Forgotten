if(WIN32)
    include(${CMAKE_SOURCE_DIR}/cmake_utils/windows/windows.cmake)
endif()

if(APPLE)
    include(${CMAKE_SOURCE_DIR}/cmake_utils/macos/macos.cmake)
endif()