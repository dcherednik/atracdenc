all: lib

lib:
	gcc -O2 -c vorbis_impl/mdct.c -o mdct_impl.o

test: lib
	g++ -std=c++11 mdct_ut.cpp -I ../../3rd/gtest-1.7.0/include/  ../../3rd/gtest-1.7.0/src/gtest-all.o ../../3rd/gtest-1.7.0/src/gtest_main.o ./mdct_impl.o -o mdct_ut
	./mdct_ut

clean:
	rm ./mdct_ut
	rm ./mdct_impl.o
