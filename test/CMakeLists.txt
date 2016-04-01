include_directories(${gtest_SOURCE_DIR}/include)

set(mdct_test_sources
    ../src/mdct/mdct_ut.cpp
)
set(MDCT_SOURCE_LIB ../src/mdct/vorbis_impl/mdct.c)
add_executable(mdct_test ${mdct_test_sources})
target_link_libraries(mdct_test mdct_impl gtest_main)

set(bitstream_test_sources
    ../src/bitstream/bitstream.cpp
    ../src/bitstream/bitstream_ut.cpp
)
add_executable(bitstream_test ${bitstream_test_sources})
target_link_libraries(bitstream_test gtest_main)

set(atrac1mdct_test_sources
    ../src/atracdenc.cpp
    ../src/transient_detector.cpp
    ../src/bitstream/bitstream.cpp
    ../src/atrac/atrac_scale.cpp
    ../src/atrac/atrac1_dequantiser.cpp
    ../src/atrac/atrac1_bitalloc.cpp
    ../src/atrac/atrac1.cpp
    ../src/atracdenc_ut.cpp
    ../src/aea.cpp
)
add_executable(atrac1mdct_test ${atrac1mdct_test_sources})
target_link_libraries(atrac1mdct_test mdct_impl gtest_main)
