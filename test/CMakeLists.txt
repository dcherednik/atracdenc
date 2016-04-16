set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")

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
    ../src/atrac/atrac3.cpp #atrac_scale has explicit instantiation
    ../src/atracdenc_ut.cpp
    ../src/aea.cpp
)
add_executable(atrac1mdct_test ${atrac1mdct_test_sources})
target_link_libraries(atrac1mdct_test mdct_impl gtest_main)

set(atrac3mdct_test_sources
    ../src/atrac3denc.cpp
    ../src/transient_detector.cpp
    ../src/bitstream/bitstream.cpp
    ../src/atrac/atrac_scale.cpp
    ../src/atrac/atrac3_bitstream.cpp
    ../src/atrac/atrac1.cpp #atrac_scale has explicit instantiation
    ../src/atrac/atrac3.cpp
    ../src/atrac3denc_ut.cpp
    ../src/oma.cpp
)
add_executable(atrac3mdct_test ${atrac3mdct_test_sources})
target_link_libraries(atrac3mdct_test mdct_impl oma gtest_main)

set(util_test_sources
    ../src/util_ut.cpp
)
add_executable(util_test ${util_test_sources})
target_link_libraries(util_test gtest_main)
