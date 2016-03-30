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
