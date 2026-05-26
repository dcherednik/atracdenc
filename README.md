# atracdenc - ATRAC Decoder Encoder
It is free LGPL implementation of ATRAC1, ATRAC3 encoders.

Building:

You need:
* C++17 compiler.
* CMake >= 3.1.
* PCM input/output backend:
  * `libsndfile` on Linux and MSYS2.
  * Media Foundation on Windows with MSVC.
* Python 3 (optional, only required to run the integration tests).

If the source was cloned with git, fetch submodules first:

```
git submodule update --init --recursive
```

The PCM backend can be selected with `ATRACDENC_PCM_IO_BACKEND`.
Supported values are `auto`, `mediafoundation`, and `libsndfile`.
The default is `mediafoundation` for MSVC Windows builds and `libsndfile`
everywhere else.

Linux, Debian/Ubuntu based:

```
sudo apt update
sudo apt install -y build-essential cmake libsndfile1-dev python3 googletest libgtest-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DATRACDENC_PCM_IO_BACKEND=libsndfile
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

The binary is created at `build/src/atracdenc`.

Windows, MSYS2 MinGW64:

Run the commands from the `MSYS2 MinGW x64` shell.

```
pacman -S --needed \
    git \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-gtest \
    mingw-w64-x86_64-libsndfile \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-python

cmake -S . -B build-msys2 -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DATRACDENC_PCM_IO_BACKEND=libsndfile
cmake --build build-msys2
ctest --test-dir build-msys2 --output-on-failure
```

The binary is created at `build-msys2/src/atracdenc.exe`.

To create a redistributable directory with required MSYS2 runtime DLLs and
license files:

```
bash tools/package-msys2-runtime.sh build-msys2/src/atracdenc.exe dist-msys2
```

Windows, MSVC:

Install Visual Studio or Visual Studio Build Tools with the C++ desktop
workload. Run the commands from an `x64 Native Tools Command Prompt for VS`.

```
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64 ^
    -DATRACDENC_PCM_IO_BACKEND=mediafoundation
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
```

The binary is created at `build-msvc/src/Release/atracdenc.exe`.

Usage:

By default the output container is selected from the output file extension.
Use `--container {aea|oma|riff|rm|raw}` to select it explicitly.
Valid combinations are ATRAC1: AEA/RAW, ATRAC3: OMA/RIFF/RM/RAW,
and ATRAC3PLUS: OMA/RIFF/RAW.

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
