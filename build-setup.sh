#!/bin/bash

git submodule init
git submodule update

cd chipyard
./build-setup.sh --force
cd ..

source env.sh
cd prof/protobuf
git submodule update --init --recursive
./autogen.sh

mkdir -p build
mkdir -p install
PROTOBUF_INSTALL_DIR=$(pwd)/install

cd build
make clean || true
../configure --prefix=$PROTOBUF_INSTALL_DIR --disable-shared
make -j64
make install
