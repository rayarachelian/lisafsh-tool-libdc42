#!/bin/bash
TOP="$(pwd)"
cd $TOP/src/lib/libdc42
./build.sh $@ || exit $?
cd $TOP/src/tools
./build.sh $@
