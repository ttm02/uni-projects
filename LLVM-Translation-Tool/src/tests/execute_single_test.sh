#!/bin/bash

if [ -z "$1" ]; then
    echo "executes a single given test (arg1)"
    echo "if sedond arg is given then it will report the test result"
    echo "if -v or --verbose is given as arg2 it will show openmp and mpi output even if test passes"
    exit
fi

TEST_SRC="${DAS_TOOL_ROOT}/src/tests/src"

#Number of OpenMP-threads / MPI-ranks
NUM_RANKS=4

export OMP_NUM_THREADS=$NUM_RANKS

#Setup
#cd $TEST_SRC
#test file should contain valid path

TEST_FILE=$1

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

#openmp:

clang++ -fopenmp -o $TEST_FILE.omp.x $TEST_FILE
$TEST_FILE.omp.x > $TEST_FILE.omp.out

#mpi:
# aus execute_pass (angepasste ausgabedatei)
# damit alle tests nebenläufig ausgeführ werden können und es nicht zu konflikten kommt, bekommt jeder seine eigenen executables
# eigene rtlib.o und mpi_mutex.o damit sichergestellt ist, dass diese dateien neu gebaut werden, auch wenn man dieses skript direkt nur für einen einzelnen test benutzt
CXXFLAGS="-O2 -g0 -fopenmp"

RTLIB_PATH="${DAS_TOOL_ROOT}/src/build/rtlib.cpp"
PASS_PATH="${DAS_TOOL_ROOT}/src/build/omp2mpi/libomp2mpiPass.so"
CLANG_PLUGIN_PATH="${DAS_TOOL_ROOT}/src/build/omp2mpi/pragmaHandler/libomp2mpiClangPlugin.so"


mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -o $TEST_FILE.o -Xclang -load -Xclang $PASS_PATH -Xclang -load -Xclang $CLANG_PLUGIN_PATH -c $TEST_FILE 
mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION $CXXFLAGS -o $TEST_FILE.mpi.x $TEST_FILE.o

mpirun -np $NUM_RANKS $TEST_FILE.mpi.x > $TEST_FILE.mpi.out

# remove executables
rm $TEST_FILE.o $TEST_FILE.omp.x $TEST_FILE.mpi.x

if [ -n "$2" ]; then
    differ=$(diff --brief $TEST_FILE.omp.out $TEST_FILE.mpi.out)
    # report result of test
    if [ -z "${differ}" ]; then
        echo -e "${GREEN}PASSED${NC}"
        if [ $2 = "--verbose" ] || [ $2 = "-v" ]; then
            echo "OpenMP (left) output VS MPI (right) output:"
            diff --side-by-side $TEST_FILE.omp.out $TEST_FILE.mpi.out
        fi
    else
        echo -e "${RED}FAIL${NC}"
        echo "OpenMP (left) output VS MPI (right) output:"
        diff --side-by-side $TEST_FILE.omp.out $TEST_FILE.mpi.out
        echo ""
    fi 
rm $TEST_FILE.omp.out $TEST_FILE.mpi.out
fi

