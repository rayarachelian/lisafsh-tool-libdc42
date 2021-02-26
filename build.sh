#!/bin/bash
cd $(dirname $0)/src/tools || exit 1
./build.sh $@
