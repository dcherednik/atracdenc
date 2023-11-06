# atracdenc - ATRAC Decoder Encoder
It is free LGPL implementation of ATRAC1, ATRAC3 encoders.

Building:

You need:
* C++11 compiler.
* cmake >= 3.1
* libsndfiles 

binary:

```
cd src
mkdir build
cd build
cmake ../
make
```


binary and tests:

```
cd test
cmake ../
make
```

Usage:

ATRAC1:
```
./atracdenc -e atrac1 -i ~/01.wav -o /tmp/01.aea
```

ATRAC3:
```
./atracdenc -e atrac3 -i ~/01.wav -o /tmp/01.oma
```

More information on the [atracdenc man page](man/atracdenc.1)

Limitations:
 - Bit allocation based on the tonality of the signal (see http://www.minidisc.org/aes_atrac.html)
 - Only 44100 16bit wav input file
