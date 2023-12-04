#!/bin/bash


mkdir -p test
g++ src/string_parser.cc -o test/string_parser -I${CONDA_PREFIX}/include/boost -D TEST
