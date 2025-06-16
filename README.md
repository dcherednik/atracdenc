# atracdenc - ATRAC Decoder Encoder
It is free LGPL implementation of ATRAC1, ATRAC3 encoders.

Building:

You need:
* C++17 compiler.
* cmake >= 3.1
* libsndfiles 

In case of git clone command GHA submodule should be fetched:

```
git submodule update --init --recursive
```

binary:

```
cd src
mkdir build
cd build
cmake ../
make
```

Binary and tests (for development):

Make sure google test library is installed

debian:
```
apt-get install libgtest-dev cmake-extras
```

Now we can build
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

ATRAC3PLUS:
```
./atracdenc -e atrac3plus -i ~/01.wav -o /tmp/01.oma
```


More information on the [atracdenc man page](https://code.mastervirt.ru/atracdenc/about/man/atracdenc.1)

Limitations:
 - Only 44100 16bit wav input file
