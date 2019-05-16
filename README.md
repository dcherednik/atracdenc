# atracdenc - ATRAC Decoder Encoder
Implementation of ATRAC1, ATRAC3 encoders

Building:

You need:
* C++11 compiler.
* cmake >= 3.0
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
./atracdenc --encode -i ~/01.wav -o /tmp/01.aea
```

ATRAC3: - use it only if you want to improve it ;)

You can use --help option to get help

Limitations:
 - Bit allocation based on the tonality of the signal (see http://www.minidisc.org/aes_atrac.html)
 - Only 44100 16bit wav input file
 
Other problems:
 - Unfortunately software using ffmpeg library often incorrectly detects AEA file.
 Be careful, the noise in case of wrong detection can be extremely high.
