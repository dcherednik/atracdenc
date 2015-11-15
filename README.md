# atracdenc - ATRAC Decoder Encoder
Implementation of ATRAC1 encoder

Building:
You need C++11 compiler.
Currently we do not use cmake, have no install target in Makefile. To build it just  type `make` in `src` dir.
Likely atracdenc executable file will be builded ))

Usage:
You can use --help option to get help

Limitations:
 - Only long window
 - Bit allocation based on the tonality of the signal (see http://www.minidisc.org/aes_atrac.html)
 - Only 44100 16bit wav input file
 
Other problems:
 - Unfortunately software using ffmpeg library often incorrectly detects AEA file.
 Be careful, the noise in case of wrong detection can be extremely high.
 - acording to http://atracdenc.mastervirt.ru/alias_fixes.html and https://yadi.sk/i/r-95jZkKkSnbu (ffmpeg decoded https://samples.ffmpeg.org/A-codecs/ATRAC1/Test%20tones%20disc%20-%20Chirp.aea)
 ffmpeg uses wrong delay so quality of decoding by ffmpeg is not perfect)
