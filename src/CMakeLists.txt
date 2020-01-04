CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

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

if (UNIX)
  add_definitions( "-Wall -O2 -g" )
endif()

project(atracdenc)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

if (WIN32)
    add_compile_definitions(PLATFORM_WINDOWS)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    include_directories("platform/win/getopt")
    set(SOURCE_PCM_IO_LIB
        platform/win/pcm_io/mf/pcm_io_mf.cpp
		platform/win/pcm_io/win32/pcm_io_win32.cpp
		platform/win/pcm_io/pcm_io.cpp
    )
else()
    INCLUDE(FindLibSndFile)
    include_directories(${LIBSNDFILE_INCLUDE_DIR})
    set(SOURCE_PCM_IO_LIB
        pcm_io_sndfile.cpp
    )
endif()

include (TestBigEndian)
TEST_BIG_ENDIAN(BIGENDIAN_ORDER)
if (${BIGENDIAN})
  add_compile_definitions(BIGENDIAN_ORDER)
endif()

include_directories(
    "lib"
    "oma/liboma/include"
)

set(SOURCE_FFT_LIB fft/kissfft_impl/kiss_fft.c)
set_source_files_properties(fft/kissfft_impl/kiss_fft.c PROPERTIES COMPILE_FLAGS -Dkiss_fft_scalar=double)
set(SOURCE_OMA_LIB oma/liboma/src/liboma.c)
set(SOURCE_BITSTREAM_LIB bitstream/bitstream.cpp)
set(SOURCE_ATRACDENC_IMPL
    wav.cpp
    aea.cpp
    env.cpp
    transient_detector.cpp
    atrac1denc.cpp
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
add_library(pcm_io STATIC ${SOURCE_PCM_IO_LIB})
add_library(oma STATIC ${SOURCE_OMA_LIB})
add_library(bitstream STATIC ${SOURCE_BITSTREAM_LIB})
add_library(atracdenc_impl STATIC ${SOURCE_ATRACDENC_IMPL})
set(SOURCE_EXE
    main.cpp
)
add_executable(atracdenc ${SOURCE_EXE})
target_link_libraries(atracdenc fft_impl pcm_io oma bitstream atracdenc_impl ${SNDFILE_LIBRARIES})

