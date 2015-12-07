
all:
	cd ./mdct && make && cd ../
	g++ -std=c++11 -O2 -g main.cpp wav.cpp aea.cpp atracdenc.cpp bitstream/bitstream.cpp atrac/atrac1.cpp atrac/atrac1_dequantiser.cpp atrac/atrac1_scale.cpp atrac/atrac1_bitalloc.cpp mdct/mdct_impl.o -o atracdenc

test:
	g++ -std=c++11 atracdenc_ut.cpp atracdenc.cpp aea.cpp bitstream/bitstream.cpp atrac/atrac1.cpp atrac/atrac1_dequantiser.cpp atrac/atrac1_scale.cpp atrac/atrac1_bitalloc.cpp -I ../3rd/gtest-1.7.0/include/  ../3rd/gtest-1.7.0/src/gtest-all.o ../3rd/gtest-1.7.0/src/gtest_main.o mdct/mdct_impl.o -o atracdenc_ut
	./atracdenc_ut
