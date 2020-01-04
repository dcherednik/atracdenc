CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(atracdenc)

if (UNIX)
    add_subdirectory(3rd/gtest-1.7.0)
    add_subdirectory(test)
endif()
add_subdirectory(src)
