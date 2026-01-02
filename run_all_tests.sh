#!/bin/bash 

set -e

for i in `jq '.buildPresets.[].name' CMakePresets.json | sed -e s/"\""//g`;
do
  echo $i;
  cmake --preset $i;
  cmake --build --preset $i;
  ctest --preset $i;
done
