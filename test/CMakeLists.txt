include_directories(${gtest_SOURCE_DIR}/include)

set(mdct_test_sources
    ../src/mdct/mdct_ut.cpp
)
set(MDCT_SOURCE_LIB ../src/mdct/vorbis_impl/mdct.c)
add_executable(mdct_test ${mdct_test_sources})
target_link_libraries(mdct_test mdct_impl gtest_main)
