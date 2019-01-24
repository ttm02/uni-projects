#!/bin/bash
[ "$DEBUG" == 'true' ] && set -x

if [ -z $1 ]; then
echo "Error: no input file given"
exit 1
fi

CXXFLAGS="-O2 -g0 -fopenmp -Wunknown-pragmas"

RTLIB_PATH="${DAS_TOOL_ROOT}/src/rtlib/rtlib.cpp"
RTLIB_OUT="${DAS_TOOL_ROOT}/src/build/rtlib.bc"
PASS_PATH="${DAS_TOOL_ROOT}/src/build/omp2mpi/libomp2mpiPass.so"
CLANG_PLUGIN_PATH="${DAS_TOOL_ROOT}/src/build/omp2mpi/pragmaHandler/libomp2mpiClangPlugin.so"

# only if --rtlib is not set: compile rtlib again
if [ -n $2 ] && ([[ $2 == "--rtlib" ]] || [[ $2 == "--rebuild" ]]); then
mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -emit-llvm -c -o $RTLIB_OUT $RTLIB_PATH
fi

mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -o translated.o -Xclang -load -Xclang $PASS_PATH -Xclang -load -Xclang $CLANG_PLUGIN_PATH -c $1  
mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -o translated translated.o
