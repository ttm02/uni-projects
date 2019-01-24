#!/bin/bash
[ "$DEBUG" == 'true' ] && set -x
set -u

i=0
# formats all code with clang-format

(clang-format -i -style=file $DAS_TOOL_ROOT/src/*.cpp ; clang-format -i -style=file $DAS_TOOL_ROOT/src/*.h )&
pids[${i}]=$!
    let i++

(clang-format -i -style=file $DAS_TOOL_ROOT/src/tests/src/*.cpp ; clang-format -i -style=file $DAS_TOOL_ROOT/src/tests/src/*.h )&
pids[${i}]=$!
    let i++

(clang-format -i -style=file $DAS_TOOL_ROOT/src/omp2mpi/*.cpp ; clang-format -i -style=file $DAS_TOOL_ROOT/src/omp2mpi/*.h )&
pids[${i}]=$!
    let i++

(clang-format -i -style=file $DAS_TOOL_ROOT/src/rtlib/*.cpp ; clang-format -i -style=file $DAS_TOOL_ROOT/src/rtlib/*.h )&
pids[${i}]=$!
    let i++

(clang-format -i -style=file $DAS_TOOL_ROOT/src/omp2mpi/comm_patterns/*.cpp ; clang-format -i -style=file $DAS_TOOL_ROOT/src/omp2mpi/comm_patterns/*.h )&
pids[${i}]=$!
    let i++

(clang-format -i -style=file $DAS_TOOL_ROOT/src/omp2mpi/pragmaHandler/*.cpp )&
pids[${i}]=$!
    let i++
#clang-format -i -style=file $DAS_TOOL_ROOT/src/omp2mpi/pragmaHandler/*.h # currently no header present there

for pid in ${pids[*]}; do
    wait $pid
done
