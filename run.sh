#! /bin/bash

git submodule update --init --recursive && make -j10; make -j10
clear
stdbuf -oL ./benchmark | tee results.txt
python ./pivot.py
gnuplot gnuplot.gnuplot