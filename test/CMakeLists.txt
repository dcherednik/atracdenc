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

enable_testing()
add_test(ut atracdenc_ut)
