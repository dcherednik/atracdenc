
all:
	cd ./mdct && make && cd ../
	g++ -std=c++11 -O2 -g main.cpp wav.cpp aea.cpp atracdenc.cpp bitstream/bitstream.cpp atrac/atrac1.cpp atrac/atrac1_dequantiser.cpp atrac/atrac1_scale.cpp atrac/atrac1_bitalloc.cpp mdct/mdct_impl.o -o atracdenc
