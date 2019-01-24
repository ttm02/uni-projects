#!/bin/bash
[ "$DEBUG" == 'true' ] && set -x

if [ -z $1 ]; then
echo "Error: no input file given"
exit 1
fi

#disable assertions in rtlib
#(does not disable assertions in the pass but they will not ĺower runtime performance anyway)
CXXFLAGS="-O3 -Ofast -fopenmp -DNDEBUG"

PASS_PATH="${DAS_TOOL_ROOT}/src/build/omp2mpi/libomp2mpiPass.so"

mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -emit-llvm -o translated.bc -Xclang -load -Xclang $PASS_PATH -c $1 
llvm-dis -o AFTER_PASS_IR.txt translated.bc
