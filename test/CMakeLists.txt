CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_C_STANDARD 11)

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
    atracdenc_impl
    oma
    GTest::gtest_main
)

configure_file(
    ../src/atrac/atrac3plus_pqf/ut/test_data/ipqftest_pcm_mr.dat
    ${CMAKE_CURRENT_BINARY_DIR}/test_data/ipqftest_pcm_mr.dat
    COPYONLY
)

configure_file(
    ../src/atrac/atrac3plus_pqf/ut/test_data/ipqftest_pcm_out.dat
    ${CMAKE_CURRENT_BINARY_DIR}/test_data/ipqftest_pcm_out.dat
    COPYONLY
)

set(at3plus_pqf_ut
    ../src/atrac/atrac3plus_pqf/ut/ipqf_ut.cpp
    ../src/atrac/atrac3plus_pqf/ut/atrac3plusdsp.c
    ../src/atrac/atrac3plus_pqf/atrac3plus_pqf.c
    ../src/mdct/mdct.cpp
)

add_executable(at3plus_pqf_ut ${at3plus_pqf_ut})

target_link_libraries(at3plus_pqf_ut
    m
    fft_impl
    GTest::gtest_main
)

enable_testing()
add_test(ut atracdenc_ut)
add_test(at3plus_pqf_ut at3plus_pqf_ut)
