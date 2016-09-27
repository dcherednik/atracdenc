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

#add_definitions( "-Wall -O2 -g -Rpass-analysis=loop-vectorize" )
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")

add_definitions( "-Wall -O2 -g" )

project(atracdenc)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
INCLUDE(FindLibSndFile)

include_directories(${LIBSNDFILE_INCLUDE_DIR})
include_directories("oma/liboma/include")

set(SOURCE_FFT_LIB fft/kissfft_impl/kiss_fft.c)
set_source_files_properties(fft/kissfft_impl/kiss_fft.c PROPERTIES COMPILE_FLAGS -Dkiss_fft_scalar=double)
set(SOURCE_OMA_LIB oma/liboma/src/liboma.c)
set(SOURCE_EXE
    main.cpp
    wav.cpp
    aea.cpp
    transient_detector.cpp
    atrac1denc.cpp
    bitstream/bitstream.cpp
    atrac/atrac1.cpp
    atrac/atrac1_dequantiser.cpp
    atrac/atrac_scale.cpp
    atrac/atrac_psy_common.cpp
    atrac/atrac1_bitalloc.cpp
    oma.cpp
    atrac3denc.cpp
    atrac/atrac3.cpp
    atrac/atrac3_bitstream.cpp
    mdct/mdct.cpp
    )

add_library(fft_impl STATIC ${SOURCE_FFT_LIB})
add_library(oma STATIC ${SOURCE_OMA_LIB})
add_executable(atracdenc ${SOURCE_EXE})
target_link_libraries(atracdenc fft_impl oma ${SNDFILE_LIBRARIES})

