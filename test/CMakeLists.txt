CMAKE_MINIMUM_REQUIRED(VERSION 3.1...3.26)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_C_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-omit-frame-pointer")

include_directories(
    "../src/lib"
)

set(atracdenc_ut
    ${CMAKE_SOURCE_DIR}/src/lib/mdct/mdct_ut.cpp
    ${CMAKE_SOURCE_DIR}/src/lib/bitstream/bitstream_ut.cpp
    ${CMAKE_SOURCE_DIR}/src/util_ut.cpp
    ${CMAKE_SOURCE_DIR}/src/atracdenc_ut.cpp
    ${CMAKE_SOURCE_DIR}/src/atrac3denc_ut.cpp
    ${CMAKE_SOURCE_DIR}/src/transient_detector_ut.cpp
    ${CMAKE_SOURCE_DIR}/src/atrac/atrac_scale_ut.cpp
)

add_executable(atracdenc_ut ${atracdenc_ut})

target_link_libraries(atracdenc_ut
    bitstream
    fft_impl
    atracdenc_impl
    oma
    GTest::gtest_main
)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/atrac/atrac3plus_pqf/ut/test_data/ipqftest_pcm_mr.dat
    ${CMAKE_CURRENT_BINARY_DIR}/test_data/ipqftest_pcm_mr.dat
    COPYONLY
)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/atrac/atrac3plus_pqf/ut/test_data/ipqftest_pcm_out.dat
    ${CMAKE_CURRENT_BINARY_DIR}/test_data/ipqftest_pcm_out.dat
    COPYONLY
)

set(at3plus_pqf_ut
    ${CMAKE_SOURCE_DIR}/src/atrac/atrac3plus_pqf/ut/ipqf_ut.cpp
    ${CMAKE_SOURCE_DIR}/src/atrac/atrac3plus_pqf/ut/atrac3plusdsp.c
    ${CMAKE_SOURCE_DIR}/src/atrac/atrac3plus_pqf/atrac3plus_pqf.c
    ${CMAKE_SOURCE_DIR}/src/lib/mdct/mdct.cpp
)

add_executable(at3plus_pqf_ut ${at3plus_pqf_ut})

target_link_libraries(at3plus_pqf_ut
    m
    fft_impl
    GTest::gtest_main
)

###

set(at3plus_bitstream_ut
    ${CMAKE_SOURCE_DIR}/src/atrac/at3p/at3p_bitstream_ut.cpp
)

add_executable(at3plus_bitstream_ut ${at3plus_bitstream_ut})

target_link_libraries(at3plus_bitstream_ut
    m
    fft_impl
    atracdenc_impl
    GTest::gtest_main
)

###

set(at3plus_gha_ut
    ${CMAKE_SOURCE_DIR}/src/atrac/at3p/at3p_gha_ut.cpp
)

add_executable(at3plus_gha_ut ${at3plus_gha_ut})

target_link_libraries(at3plus_gha_ut
    m
    fft_impl
    atracdenc_impl
    GTest::gtest_main
)

###

set(at3plus_mdct_ut
    ${CMAKE_SOURCE_DIR}/src/atrac/at3p/at3p_mdct_ut.cpp
)

add_executable(at3plus_mdct_ut ${at3plus_mdct_ut})

target_link_libraries(at3plus_mdct_ut
    m
    fft_impl
    atracdenc_impl
    GTest::gtest_main
)

###

set(bs_encode_ut
    ${CMAKE_SOURCE_DIR}/src/lib/bs_encode/encode_ut.cpp
    ${CMAKE_SOURCE_DIR}/src/lib/bs_encode/encode.cpp
)

add_executable(bs_encode_ut ${bs_encode_ut})

target_link_libraries(bs_encode_ut
    m
    bitstream
    GTest::gtest_main
)

###



enable_testing()
add_test(ut atracdenc_ut)
add_test(at3plus_pqf_ut at3plus_pqf_ut)
add_test(at3plus_bitstream_ut at3plus_bitstream_ut)
add_test(at3plus_gha_ut at3plus_gha_ut)
add_test(at3plus_mdct_ut at3plus_mdct_ut)
add_test(bs_encode_ut bs_encode_ut)
