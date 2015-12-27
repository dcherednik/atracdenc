# atracdenc - ATRAC Decoder Encoder
Implementation of ATRAC1 encoder

Building:
You need C++11 compiler.
cmake > 2.8
libsndfiles 

`cd src`
`mkdir build`
`cd build`
`cmake ../`
`make`

Usage:
You can use --help option to get help

Limitations:
 - Bit allocation based on the tonality of the signal (see http://www.minidisc.org/aes_atrac.html)
 - Only 44100 16bit wav input file
 
Other problems:
 - Unfortunately software using ffmpeg library often incorrectly detects AEA file.
 Be careful, the noise in case of wrong detection can be extremely high.
