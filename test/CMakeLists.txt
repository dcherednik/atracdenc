CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

macro(use_11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    message ("cmake version: " ${CMAKE_VERSION} " --std arguments will be forced")

    set (CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
    set (CMAKE_C_FLAGS "-std=c11 ${CMAKE_C_FLAGS}" CACHE STRING "" FORCE)
  else ()
    set (CMAKE_CXX_STANDARD 11)
    set (CMAKE_C_STANDARD 11)
  endif ()
endmacro(use_11)

use_11()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    message ("Using -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-omit-frame-pointer")
endif ()


include_directories(${gtest_SOURCE_DIR}/include)

set(atracdenc_ut
    ../src/mdct/mdct_ut.cpp
    ../src/bitstream/bitstream_ut.cpp
    ../src/util_ut.cpp
    ../src/atracdenc_ut.cpp
    ../src/transient_detector_ut.cpp
)

add_executable(atracdenc_ut ${atracdenc_ut})

target_link_libraries(atracdenc_ut
    bitstream
    fft_impl
    gtest_main
    atracdenc_impl
    oma
)

enable_testing()
add_test(ut atracdenc_ut)
