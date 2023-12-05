#!/bin/bash


mkdir -p test
rm test/*


function test_string_parser() {
  g++ src/string_parser.cc -o test/string_parser -I${CONDA_PREFIX}/include/boost -D TEST
  ./test/string_parser
}

function test_file_splitter() {
  g++ -c src/string_parser.cc -o test/string_parser.o -I${CONDA_PREFIX}/include/boost
  g++ -c src/file_splitter.cc -o test/file_splitter.o   -I${CONDA_PREFIX}/include/boost -D TEST
  g++ test/string_parser.o test/file_splitter.o -o test/file_splitter -I${CONDA_PREFIX}/include/boost -D TEST
  ./test/file_splitter
  cat test/test.txt
  ls test
}

test_file_splitter
