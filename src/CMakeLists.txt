CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

add_definitions( "-O2 -g" )
set(CMAKE_CXX_STANDARD 11)

project(atracdenc)

set(SOURCE_LIB mdct/vorbis_impl/mdct.c)
set(SOURCE_EXE main.cpp wav.cpp aea.cpp transient_detector.cpp atracdenc.cpp bitstream/bitstream.cpp atrac/atrac1.cpp atrac/atrac1_dequantiser.cpp atrac/atrac1_scale.cpp atrac/atrac1_bitalloc.cpp)

add_library(mdct_impl STATIC ${SOURCE_LIB})
add_executable(atracdenc ${SOURCE_EXE})
target_link_libraries(atracdenc mdct_impl)

