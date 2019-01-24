#!/bin/bash
[ "$DEBUG" == 'true' ] && set -x
set -u

CXXFLAGS="-O2 -g0 -fopenmp"

RTLIB_PATH="${DAS_TOOL_ROOT}/src/rtlib.cpp"
RTLIB_OUT="${DAS_TOOL_ROOT}/src/rtlib.bc"
PASS_PATH="${DAS_TOOL_ROOT}/src/build/omp2mpi/libomp2mpiPass.so"

mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -emit-llvm -c -o $RTLIB_OUT $RTLIB_PATH


mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -mllvm -time-passes -mllvm -stats -o translated.o -Xclang -load -Xclang $PASS_PATH -c $1 
# to see which passes are called:
#mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -mllvm -debug-pass=Structure -o translated.o -Xclang -load -Xclang $PASS_PATH -c $1

mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -o translated translated.o
