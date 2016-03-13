CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

add_definitions( "-Wall -O2 -g" )

project(atracdenc)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
INCLUDE(FindLibSndFile)

include_directories(${LIBSNDFILE_INCLUDE_DIR})
include_directories("oma/liboma/include")

set(SOURCE_MDCT_LIB mdct/vorbis_impl/mdct.c)
set(SOURCE_OMA_LIB oma/liboma/src/liboma.c)
set(SOURCE_EXE
    main.cpp
    wav.cpp
    aea.cpp
    transient_detector.cpp
    atracdenc.cpp
    bitstream/bitstream.cpp
    atrac/atrac1.cpp
    atrac/atrac1_dequantiser.cpp
    atrac/atrac_scale.cpp
    atrac/atrac1_bitalloc.cpp
    oma.cpp
    atrac3denc.cpp
    atrac/atrac3.cpp
    atrac/atrac3_bitstream.cpp
    )

add_library(mdct_impl STATIC ${SOURCE_MDCT_LIB})
add_library(oma STATIC ${SOURCE_OMA_LIB})
add_executable(atracdenc ${SOURCE_EXE})
target_link_libraries(atracdenc mdct_impl oma ${SNDFILE_LIBRARIES})

