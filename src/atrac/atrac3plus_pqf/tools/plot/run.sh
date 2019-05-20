#!/usr/bin/env bash

set -e

cd pqf

python3.7 setup.py build

cd ..

export PYTHONPATH=./pqf/build/lib.mingw-3.7/
./main.py "$@"
