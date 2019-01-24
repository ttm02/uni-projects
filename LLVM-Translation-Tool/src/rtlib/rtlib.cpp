#include "rtlib.h"

// includes all the implementation for all rtlib functions so that they may be copied into
// translated module instead of linked against rtlib for optimization (most importent inlining)
// purpouses

#include "mpi_mutex.cpp"

#include "tasklib.cpp"

#include "rtlib-profiling.cpp"

#include "array.cpp"

#include "singleValue.cpp"

#include "struct_ptr.cpp"

#include "utility.cpp"

#include "general.cpp"

#include "legacy.cpp"
