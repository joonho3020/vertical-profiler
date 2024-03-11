#!/bin/bash

git submodule init
git submodule update

cd chipyard
./build-setup.sh --force
cd ..

source env.sh
cd src/protobuf
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

cd ../../riscv-isa-sim-private
mkdir build && cd build
../configure --prefix=$RISCV --with-boost=no --with-boost-asio=no --with-boost-regex=no
make -j$(nproc)
make install

cd ../../spike-devices
make -j$(nproc)

pip install meson
pip install pyright
