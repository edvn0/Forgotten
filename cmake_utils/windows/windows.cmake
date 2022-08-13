list(APPEND CMAKE_MODULE_PATH "C:/Development")
list(APPEND CMAKE_MODULE_PATH "D:/Development")

set(BOOST_DIR "D:/Development/boost/boost_1_79_0")
set(BOOST_ROOT "D:/Development/boost/boost_1_79_0")
set(BOOST_LIBRARYDIR "D:/Development/boost/boost_1_79_0/stage/lib")
set(Boost_USE_STATIC_LIBS ON)
list(APPEND CMAKE_MODULE_PATH ${BOOST_LIBRARYDIR})
list(APPEND CMAKE_MODULE_PATH ${BOOST_DIR})
list(APPEND CMAKE_MODULE_PATH ${BOOST_ROOT})

set(FT_DISABLE_HARFBUZZ ON)
