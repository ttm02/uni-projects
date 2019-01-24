#!/bin/bash
CPUS=$(nproc)
#nprocs -1 as building rtlib will use one CPU as well
let CPUS-=1

RTLIBFLAGS="-O2 -g0 -fopenmp -Wunknown-pragmas -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable"
RTLIB_PATH="${DAS_TOOL_ROOT}/src/rtlib/rtlib.cpp"
RTLIB_OUT="${DAS_TOOL_ROOT}/src/build/rtlib.bc"


if [ -n "$1" ] && [ $1 = "--rebuild" ]; then
    rm -rf build 
  elif [ -n "$1" ]; then
    echo "use --rebuild to first clean build dir"
fi
mkdir -p build
cd build

echo "building rtlib"
(mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $RTLIBFLAGS -emit-llvm -c -o $RTLIB_OUT $RTLIB_PATH ; echo "Built target rtlib" )&
pid=$!

#build the pass
cmake ..
make -j$CPU
wait $pid
