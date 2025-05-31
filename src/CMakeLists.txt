CMAKE_MINIMUM_REQUIRED(VERSION 3.1...3.26)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_C_STANDARD 11)

#add_definitions( "-Wall -O2 -g -Rpass-analysis=loop-vectorize" )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

include(CheckCXXCompilerFlag)

function(enable_cxx_compiler_flag_if_supported flag)
    string(FIND "${CMAKE_CXX_FLAGS}" "${flag}" flag_already_set)
    if(flag_already_set EQUAL -1)
        check_cxx_compiler_flag("${flag}" flag_supported)
        if(flag_supported)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
        endif()
        unset(flag_supported CACHE)
    endif()
endfunction()



project("atracdenc" CXX C)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

enable_cxx_compiler_flag_if_supported("-Wall")
enable_cxx_compiler_flag_if_supported("-Wextra")

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
    "."
    "lib"
    "lib/liboma/include"
    "lib/fft/kissfft_impl"
)

set(SOURCE_FFT_LIB
    lib/fft/kissfft_impl/kiss_fft.c
    lib/fft/kissfft_impl/tools/kiss_fftr.c
)

set_source_files_properties(${SOURCE_FFT_LIB} PROPERTIES COMPILE_FLAGS -Dkiss_fft_scalar=float)
add_library(fft_impl STATIC ${SOURCE_FFT_LIB})

set(GHA_FFT_LIB fft_impl)
add_subdirectory(lib/libgha)

set(SOURCE_OMA_LIB lib/liboma/src/liboma.c)
set(SOURCE_BITSTREAM_LIB lib/bitstream/bitstream.cpp)
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
    rm.cpp
    at3.cpp
    atrac3denc.cpp
    atrac/atrac3.cpp
    atrac/atrac3_bitstream.cpp
    atrac/atrac3plus_pqf/atrac3plus_pqf.c
    atrac/at3p/ff/atrac3plusdsp.c
    atrac/at3p/at3p.cpp
    atrac/at3p/at3p_bitstream.cpp
    atrac/at3p/at3p_gha.cpp
    atrac/at3p/at3p_mdct.cpp
    atrac/at3p/at3p_tables.cpp
    lib/mdct/mdct.cpp
    lib/bs_encode/encode.cpp
)

add_library(pcm_io STATIC ${SOURCE_PCM_IO_LIB})
if (NOT WIN32)
target_link_libraries(pcm_io ${SNDFILE_LIBRARIES})
endif()

add_library(oma STATIC ${SOURCE_OMA_LIB})
add_library(bitstream STATIC ${SOURCE_BITSTREAM_LIB})
add_library(atracdenc_impl STATIC ${SOURCE_ATRACDENC_IMPL})
target_link_libraries(atracdenc_impl fft_impl pcm_io oma bitstream ${SNDFILE_LIBRARIES} gha)
set(SOURCE_EXE
    main.cpp
    help.cpp
)
add_executable(atracdenc ${SOURCE_EXE})
target_link_libraries(atracdenc pcm_io oma atracdenc_impl ${SNDFILE_LIBRARIES})
install(TARGETS atracdenc)

#DEVTOOL
set(GHASENDTOOL_EXE
    atrac/at3p/ghasend_tool.cpp
)
add_executable(ghasendtool ${GHASENDTOOL_EXE})
target_link_libraries(ghasendtool pcm_io oma atracdenc_impl ${SNDFILE_LIBRARIES})
