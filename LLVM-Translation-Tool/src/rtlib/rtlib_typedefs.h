#ifndef RTLIB_TYPES_H_
#define RTLIB_TYPES_H_

#include <map>
#include <mpi.h>
#include <vector>

#include "rtlib-profiling.h"

struct mpi_struct_ptr
{
    int rank;
    MPI_Aint displacement;
    size_t size; // size of target struct
    // MPI_Win win;
    // MPI Win is not included is is global var instead
};

struct memory_allocation_info
{
    std::map<void *, long> allocs;
};

// contains all information needed to distribute an array
typedef struct
{
    // void *base_ptr; // now part of an communication_info_struct
    MPI_Win win;
    // range own_upper/lower is inclusive
    long own_upper;
    long own_lower;
    int DIM;
    long D1_count;       // size of first dim
    size_t D1_elem_size; // size of each elem of first dim (= elem_size* Dn*Dn-1*...*D2) may be
                         // sizeof(type) for 1D array
    // This means that D1_elem_size is the allocation size of the data_layer of one full
    // "element/line/plane/cube/..."
    int *NDIM; // array of size DIM that indicate size of each array dim for mem allocastion
    int *location_info; // array length D1_count to indicate wether an
                        // "element/line/plane/cube/..." that belongs to another rank is
                        // currently cashed
    std::vector<long> *currently_cached; // sotores all indices which are currently cached
    // stores the value of rank the var belongs to or numprocs+ tgt_rank if it is present local
    // (so that if number >= numprocs value is present on this rank)
    long *upper_lines; // for each rank holds the line idx that is first of assigned chunk

    size_t elem_size; // size of a single elem
#ifndef DESTROY_GLOBAL_ARRAY_IN_PARALLEL_REGION
    void *
        orig_master_array_ptr; // master only: the original ptr to use as recvbuffer in gaterhv
#endif
#ifdef ARRAY_PROFILING
    array_profiling_info *array_profiling_info;
#endif
} array_distribution_info;

// information about a master-based array
typedef struct
{
    MPI_Win win;
    int DIM;
    int D1_count;        // size of first dim
    size_t D1_elem_size; // size of each elem of first dim (= elem_size* Dn*Dn-1*...*D2) may be
                         // sizeof(type) for 1D array
    // This means that D1_elem_size is the allocation size of the data_layer of one full
    // "element/line/plane/cube/..."
    int *NDIM; // array of size DIM that indicate size of each array dim for mem allocastion

    size_t elem_size; // size of a single elem
} master_based_array_info;

// information for a broadcasted array (reading comm pattern)
typedef struct
{
    int DIM;
    size_t elem_size;

    MPI_Win win;
} bcasted_array_info;

typedef struct
{
    MPI_Win win;
} single_value_info;

/*
// example layout for a communication_info_struct:
struct example_comm_info
{
    void* local_ptr_buffer;
    //int Pattern; // to store the communication pattern
    array_distribution_info comm_info;// direct a part of the strut
    // it is accessed with an offset (sizeof buffer_type) which is known at translation time
};
*/

// the pass will build a new struct type for each variable type found
// the struct just contain first local variabel buffer
// second the communication_information struct as defined in this header
// therefore each communication function should be accessed by a wrapper function, which
// "splits" it into the struct and local buffer part

enum Memory_allocation_style
{
    UNKNOWN,
    CONTINUOUS,
    NON_CONTINUOUS
};

#endif /* RTLIB_TYPES_H_ */
