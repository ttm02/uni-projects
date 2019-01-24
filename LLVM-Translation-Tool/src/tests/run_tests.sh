#!/bin/bash

#DAS_TOOL_ROOT="/home/jammer/MA/LLVM-Translation-Tool"

EXECUTE_PASS="${DAS_TOOL_ROOT}/src/execute_pass.sh"
TEST_SRC="${DAS_TOOL_ROOT}/src/tests/src"
TEST_ROOT="${DAS_TOOL_ROOT}/src/tests"

#runs all tests given by testcases.txt file
# if -v or --verbose is set it will tell which tests have passed

#Number of OpenMP-threads / MPI-ranks
NUM_RANKS=4

export OMP_NUM_THREADS=$NUM_RANKS

#Setup
TEST_CASES_FILE="${TEST_ROOT}/testcases.txt"


GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color
NUM_TESTS=0
i=0

CXXFLAGS="-O2 -g0 -fopenmp"

PASS_PATH="${DAS_TOOL_ROOT}/src/build/omp2mpi/libomp2mpiPass.so"

# fork each test case
# use channel 10 so that stdin may be read
while read -u 10 line; do
    TEST_FILE=$TEST_SRC/$line
    $TEST_ROOT/execute_single_test.sh $TEST_FILE &
    pids[${i}]=$!
    let i++
    let NUM_TESTS++

done 10<$TEST_CASES_FILE

echo "Performing ${NUM_TESTS} Tests"

# wait for all pids (join)
for pid in ${pids[*]}; do
    wait $pid
    echo -e ".\c"
done

echo ""

i=0
passed=0
fail=0

while read -u 10 line; do
    TEST_FILE=$TEST_SRC/$line
    differ=$(diff --brief $TEST_FILE.omp.out $TEST_FILE.mpi.out)
    # report result of test
    if [ -z "${differ}" ]; then
        #passed
	    let passed++
        if [ -n "$1" ] && ( [ $1 = "--verbose" ] || [ $1 = "-v" ] ); then
            echo -e "${GREEN}PASSED${NC} ${line}"
        fi        
    else
        #fail
        let fail++
        echo -e "${RED}FAIL${NC} ${line}"
        echo "OpenMP (left) output VS MPI (right) output:"
	diff --side-by-side $TEST_FILE.omp.out $TEST_FILE.mpi.out
        echo ""
    fi 
    rm $TEST_FILE.omp.out $TEST_FILE.mpi.out
    let i++
done 10<$TEST_CASES_FILE

echo "Passed ${passed} of ${NUM_TESTS}"
if [ $fail -eq 0 ]; then
    echo -e "${GREEN}ALL TESTS PASSED${NC}"
fi
