#!/bin/bash -ex

cd $(dirname $0)

make afl
afl-fuzz -i testcases -o findings -- ./connection_afl
