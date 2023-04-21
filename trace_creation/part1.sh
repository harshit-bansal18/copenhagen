#!/bin/bash

gcc -O3 -static -pthread prog1.c -o prog1
gcc -O3 -static -pthread prog2.c -o prog2
gcc -O3 -static -pthread prog3.c -o prog3
gcc -O3 -static -pthread prog4.c -o prog4

# run pin instrumentation file
make obj-intel64/addrtrace.so
mkdir prog1_output
../../../pin -t obj-intel64/addrtrace.so -- ./prog1 8
mkdir prog2_output
../../../pin -t obj-intel64/addrtrace.so -- ./prog2 8
mkdir prog3_output
../../../pin -t obj-intel64/addrtrace.so -- ./prog3 8
mkdir prog4_output
../../../pin -t obj-intel64/addrtrace.so -- ./prog4 8
rm prog1 prog2 prog3 prog4