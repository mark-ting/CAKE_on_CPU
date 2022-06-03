#!/bin/bash

# git clone https://github.com/vnatesh/CAKE_on_CPU.git
# cd CAKE_on_CPU
# make install

# Download BLIS kernels
git clone https://github.com/amd/blis.git

BLIS_PATH=$PWD
cd blis

# reset to older blis version for now
#git reset --hard 961d9d5

# ./configure CC=aarch64-linux-gnu-gcc --prefix=$BLIS_PATH --enable-threading=openmp cortexa53
# install BLIS in curr dir and configire with openmp
./configure --prefix=$BLIS_PATH --enable-threading=openmp auto
# ./configure --enable-threading=openmp haswell
make -j4
make check

# install BLIS
make install
#make distclean
cd ..

#source ./env.sh
#make build

# # build CAKE pytorch extension 
# cd python
# python3 setup.py install --user

