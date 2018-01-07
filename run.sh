#! /bin/bash

git submodule init && make -j10
./benchmark | tee results.txt
./pivot
gnuplot gnuplot.gnuplot