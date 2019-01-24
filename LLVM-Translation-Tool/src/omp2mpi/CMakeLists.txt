add_subdirectory(pragmaHandler)

add_library(omp2mpiPass MODULE
	#List your source files here.
    omp2mpi.cpp
    ParallelFunction.h
    ParallelFunction.cpp
    ParallelEntryFunction.h
    ParallelEntryFunction.cpp
    ParallelTaskEntry.h
    ParallelTaskEntry.cpp
    SharedVariable.h
    SharedVariable.cpp
    SharedArray.h
    SharedArray.cpp
    SharedSingleValue.h
    SharedSingleValue.cpp
    helper.h
    helper.cpp
    external_functions.h
    external_functions.cpp
    replace_pragmas.h
    replace_pragmas.cpp
    environment.h
    struct_pointer.h
    struct_pointer.cpp
    task_pragma.h
    task_pragma.cpp
    SCCP.h
    SCCP.cpp
	#in subdir:
    comm_patterns/commPatterns.h
    comm_patterns/default.cpp
    comm_patterns/push_update.cpp
    comm_patterns/task.cpp
    comm_patterns/array_distribution.cpp
)

#Use C++ 11 to compile our pass(i.e., supply -std = c++ 11).
#target_compile_features(omp2mpiPass PRIVATE cxx_range_for cxx_auto_type)
#need c++ 17 now
target_compile_features(omp2mpiPass PRIVATE cxx_range_for cxx_auto_type cxx_std_17)

#LLVM is(typically) built with no C++ RTTI.We need to match that;
#otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(omp2mpiPass PROPERTIES
	COMPILE_FLAGS "-O3 -fno-rtti -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable"
)
# COMPILE_FLAGS "-O3 -fno-rtti "