#!/bin/bash

git submodule init
git submodule update

cd chipyard
./build-setup.sh --force
cd ..
