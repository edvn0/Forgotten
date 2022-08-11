option(FORGOTTEN_OS "Is forgotten built on MacOS?" "MacOS")

if(FORGOTTEN_OS STREQUAL "")
    message(WARNING "No operating system chosen, defaulting to MacOS. [Use -DFORGOTTEN_OS=\"<Windows,MacOS,Linux>\"]")
    set(FORGOTTEN_MACOS ON)
endif()

if(FORGOTTEN_OS STREQUAL "MacOS")
    set(FORGOTTEN_MACOS ON)
endif()

if(FORGOTTEN_OS STREQUAL "Windows")
    set(FORGOTTEN_WINDOWS ON)
endif()

if(FORGOTTEN_OS STREQUAL "Linux")
    set(FORGOTTEN_LINUX ON)
endif()
