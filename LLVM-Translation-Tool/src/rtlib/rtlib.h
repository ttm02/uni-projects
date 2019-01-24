#ifndef RTLIB_H_
#define RTLIB_H_

#include <mpi.h>

// compiler switches to influence the behaviour of rtlib:

// collect informations about array usage
//#define ARRAY_PROFILING
// collect information about task scheduling
//#define TASK_PROFILING

// mpi_malual_reduce will use a tree for reduction
#define USE_TREE_REDUCTION

// if defined: array on master will be destroyed in parallel region
#define DESTROY_GLOBAL_ARRAY_IN_PARALLEL_REGION

// space that will be reserved beforehand to store which array lines needs to be invalidated
// may be found out by profiling
#define RESERVE_CACHED_SPACE 2

// include all the other headers
#include "rtlib_typedefs.h"

#include "array.h"
#include "general.h"
#include "rtlib-profiling.h"
#include "singleValue.h"
#include "struct_ptr.h"
#include "utility.h"

// Globals
// Globals for rank, size and the sum of rank and size
int my_rank;
int numprocs;
int rank_and_size; // = my_rank + numprocs (so that it must not be calculated again and again)

struct memory_allocation_info *alloc_info;

// Debug Macro to switch printi statements on/off
#define RTLIB_DEBUG 0

#if RTLIB_DEBUG == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

#endif /* RTLIB_H_ */
