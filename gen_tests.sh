#!/bin/bash

if [[ $# -ne 1 ]];
then
	echo "format: ./gen_tests <test_name>"
	exit 1
fi

for i in $(seq 0 7);do
	touch debug_output/$1.out.$i
done
