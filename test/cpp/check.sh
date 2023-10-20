#!/bin/sh

make clean
make -j all

./btreeFuzz
NE_FUZZ_LMDB=1 ./btreeFuzz
./lmdbTest
./subRange
