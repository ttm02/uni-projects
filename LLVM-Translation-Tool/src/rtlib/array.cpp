#include "rtlib.h"
#include <limits.h>

// suggest to inline this
void log_memory_allocation(void *base_ptr, long size)
{
    alloc_info->allocs.insert(std::make_pair(base_ptr, size));
    Debug(printf("Memory Allocation of: %ld bytes at address: %p\n", size, base_ptr);)
}

// suggest to  inline this
long get_array_size_from_address(void *base_ptr, size_t elem_size)
{
    Debug(printf("try to find Array size\n");) long size = 1;
    if (alloc_info->allocs.find(base_ptr) != alloc_info->allocs.end())
    {
        size = alloc_info->allocs[base_ptr] / elem_size;
    }
    else
    {
        printf("Warinig: Array size not found from adress, therefore assuming 1\n");
    }

    Debug(printf("Array size: %ld\n", size);)

        return size;
}

// suggest to inline this
int check_2d_array_memory_allocation_style(void *base_ptr)
{
    long size = alloc_info->allocs[base_ptr];
    Debug(printf("Checking memory allocation style for 2d-array at address: %p, size: %ld\n",
                 base_ptr, size);)

        long **p = (long **)base_ptr;

    if (size > 0)
    {
        for (unsigned long i = 0; i < size / sizeof(long *); i++)
        {
            long size2 = alloc_info->allocs[p[i]];
            if (size2 > 0)
            {
                Debug(printf("Memory allocation at array[%d][0]: %p with size: %ld\n", i, p[i],
                             size2);)
            }
            else
            {
                if (alloc_info->allocs[p[0]] > 0)
                {
                    Debug(printf("Detected continuous memory allocation for "
                                 "2d-array\n");) return CONTINUOUS;
                }
                else
                {
                    Debug(printf("Memory allocation style for 2d array can't be figured "
                                 "out\n");) return UNKNOWN;
                }
            }
        }
        Debug(printf("Detected non continuous memory allocation for "
                     "2d-array\n");) return NON_CONTINUOUS;
    }
    else
    {
        printf("Could not find any memory allocations at the provided address\n");
        return UNKNOWN;
    }
}

// helper function that get the ptr to data lvl for index i of array
// suggest to inline this
void *get_data_lvl_ptr(array_distribution_info *array_info, void **base_ptr_pos, long idx)
{
    if (array_info->DIM == 1) // get pointer to elem
    {
        // as char* so indexing element size is byte
        char *as_char = (char *)(*base_ptr_pos);
        return &as_char[idx * array_info->elem_size];
    }
    else
    {
        void **current_lvl = &(((void **)(*base_ptr_pos))[idx]);
        // decent down to data lvl (starting with 1 as we are currently in lvl 1)
        for (int i = 1; i < array_info->DIM; ++i)
        {
            current_lvl = (void **)(current_lvl[0]);
        }
        // printf("Dim:%d Data-Level[%lu]:%p\n", array_info->DIM,idx, current_lvl);
        return (void *)current_lvl;
    }
}

// helper function that allocates a N dimensional contigous array with malloc
// void* array is array base ptr (null on worker)
// array_dim is dimension of array
// size_of_dim is array of size DIM that indicate size of each array dim
// elem_size is the size for each element
// retruns base ptr
// suggest to not inline this
void *alloc_array(unsigned int array_dim, int *size_of_dim, size_t elem_size)
{
    unsigned long i, j;
    unsigned long layer_elem_count;
    assert(array_dim > 0);
    assert(*size_of_dim > 0);
    assert(elem_size > 0);

    if (array_dim == 1) // trivial:
    {
        return malloc(*size_of_dim * elem_size);
    }
    else // FUN:
    {
        // pointer to each array layer:
        void ***dim_ptr = (void ***)malloc(array_dim * sizeof(void *));

        // allocate each "middle" layer
        for (i = 0; i < array_dim - 1; ++i)
        {
            assert(size_of_dim[i] > 0);
            layer_elem_count = size_of_dim[i];
            for (j = 0; j < i; ++j)
            {
                layer_elem_count = layer_elem_count * size_of_dim[j];
            }
            dim_ptr[i] = (void **)malloc(layer_elem_count * sizeof(void *));
            // printf("layer:%d count %d\n", i, layer_elem_count);
        }

        // allocate data layer
        i = array_dim - 1;
        assert(size_of_dim[i] > 0);
        layer_elem_count = size_of_dim[i];
        for (j = 0; j < i; ++j)
        {
            layer_elem_count = layer_elem_count * size_of_dim[j];
        }
        dim_ptr[i] = (void **)malloc(layer_elem_count * elem_size);
        // printf("final layer:%d count %d\n", i, layer_elem_count);

        // initialize middle layers:
        for (i = 0; i < array_dim - 2; ++i)
        {
            layer_elem_count = size_of_dim[i];
            for (j = 0; j < i; ++j)
            {
                layer_elem_count = layer_elem_count * size_of_dim[j];
            }
            // for each elem in this layer
            unsigned long offset = size_of_dim[i + 1];
            unsigned long current_pos = 0;
            // offset for next layer
            for (j = 0; j < layer_elem_count; ++j)
            {
                // total pointer FUN:
                dim_ptr[i][j] = (void *)&dim_ptr[i + 1][current_pos];
                current_pos += offset;
            }
        }
        // initialize second lowest layer with ptr to data
        i = array_dim - 1 - 1;
        layer_elem_count = size_of_dim[i];
        for (j = 0; j < i; ++j)
        {
            layer_elem_count = layer_elem_count * size_of_dim[j];
        }
        // for each elem in this layer
        unsigned long offset = size_of_dim[i + 1] * elem_size;
        unsigned long current_pos = 0;
        // offset for next layer
        for (j = 0; j < layer_elem_count; ++j)
        {
            // even more FUN than above:
            dim_ptr[i][j] = (void *)&((char *)(dim_ptr[i + 1]))[current_pos];
            current_pos += offset;
        }

        // return first layer
        void *result = (void *)dim_ptr[0];
        // free temp mem:
        free(dim_ptr);

        return result;
    }
}

// suggest to not inline this
void free_array(void *array, int array_dim)
{
    void **next_layer = (void **)array;
    void **this_layer = (void **)array;
    for (int i = 0; i < array_dim - 1; ++i)
    {
        this_layer = next_layer;
        next_layer = (void **)this_layer[0];
        free(this_layer);
    }
    // final layer
    free(next_layer);
}

// analogous to calculate_for_boundaries
// input is global upper and lower (inclusive range)
// out is local (inclusive range)
// suggest to inline this
void calculate_array_boundaries(long *upper, long *lower)
{
    // claculate own_upper_and own_lower
    long global_size = *lower - *upper + 1; //+1 as range is inclusive;

    long chunk_size = global_size / numprocs; // may be 0 if to less elems to distribute
    long rem = global_size % numprocs;

    long own_upper = chunk_size * my_rank + (my_rank < rem ? my_rank : rem);
    long own_lower = own_upper + chunk_size + (my_rank < rem ? 1 : 0) - 1; // inclusive range

    if (my_rank == numprocs - 1)
    {
        assert(own_lower == *lower);
    }

    *upper = own_upper;
    *lower = own_lower;
}

// LEGACY
// get sendcounts and displacement for a call of scatterv/gatehrVv
void calculate_sendcounts_and_displacement(array_distribution_info *array_info,
                                           int *sendcounts, int *displacement,
                                           int *my_sendcount)
{
    Debug(
        // to be safe all mem is init:
        memset(sendcounts, 0, numprocs * sizeof(int));
        memset(displacement, 0, numprocs * sizeof(int)); *my_sendcount = 0;)

        int own_size = array_info->own_lower - array_info->own_upper + 1;
    if (array_info->own_upper == -1)
    {
        assert(array_info->own_lower == -1);
        own_size = 0;
    }
    assert(own_size >= 0);

    // own vlaues:
    *my_sendcount = own_size * array_info->D1_elem_size;
    int my_displacement = array_info->own_upper * array_info->D1_elem_size;
    my_displacement = my_displacement < 0 ? 0 : my_displacement;

    MPI_Allgather(my_sendcount, 1, MPI_INT, sendcounts, 1, MPI_INT, MPI_COMM_WORLD);
    MPI_Allgather(&my_displacement, 1, MPI_INT, displacement, 1, MPI_INT, MPI_COMM_WORLD);

    Debug(if (my_rank == 0) {
        printf("Sendcounts , displacement\n");
        for (int t = 0; t < numprocs; ++t)
        {
            printf("%d , %d\n", sendcounts[t], displacement[t]);
        }
    })
}

void get_array_NDIM_form_master(void *array, int DIM, size_t elem_size, int *NDIM,
                                size_t *D1_elem_size)
{
    long elem_count = 1;
    // init NDIM Vector for mem
    // allocation
    // TODO this calculation only works
    // when each layer is allocated
    // Continous!
    if (my_rank == 0)
    {
        Debug(printf("%dD Array Dimensions: ", DIM);) void **current_ptr = (void **)array;
        // need areess from array as for
        // loop will first decent a layer
        for (int i = 0; i < DIM - 1; ++i)
        {
            NDIM[i] = get_array_size_from_address(current_ptr, sizeof(void *)) / elem_count;
            elem_count = elem_count * NDIM[i];
            current_ptr = (void **)current_ptr[0]; // go one
                                                   // level
                                                   // deeper
            Debug(printf(" %d", NDIM[i]);)
        }
        // final layer
        NDIM[DIM - 1] = get_array_size_from_address(current_ptr, elem_size) / elem_count;
        Debug(printf(" %d\n", NDIM[DIM - 1]);) elem_count = elem_count * NDIM[DIM - 1];
        long elem_count = 1;
        // init NDIM Vector for mem
        // allocation
        // TODO this calculation only works
        // when each layer is allocated
        // Continous!
        if (my_rank == 0)
        {
            Debug(printf("%dD Array Dimensions: ", DIM);) void **current_ptr = (void **)array;
            // need areess from array as for
            // loop will first decent a layer
            for (int i = 0; i < DIM - 1; ++i)
            {
                NDIM[i] =
                    get_array_size_from_address(current_ptr, sizeof(void *)) / elem_count;
                elem_count = elem_count * NDIM[i];
                current_ptr = (void **)current_ptr[0]; // go one
                                                       // level
                                                       // deeper
                Debug(printf(" %d", NDIM[i]);)
            }
            // final layer
            NDIM[DIM - 1] = get_array_size_from_address(current_ptr, elem_size) / elem_count;
            Debug(printf(" %d\n", NDIM[DIM - 1]);) elem_count = elem_count * NDIM[DIM - 1];
            int D1_count = NDIM[0];
            *D1_elem_size = elem_size * elem_count / D1_count;

            Debug(printf("This means: D1 Count: "
                         "%d D1_elem_size=%lu "
                         "with elem_size: %lu\n",
                         D1_count, *D1_elem_size, elem_size);)
        }
    }
    MPI_Bcast(NDIM, DIM, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(D1_elem_size, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
}

// collective call: distribute a shared array from the master
// void** is the ptr to where the array_ptr may be found
// as the ptr is passed in thes form
// DIM is dimension of array
// elem_size is the size for each element
// retruns array_distribution_info for all threads

// suggest to not inline this
void init_array_distribution_info(array_distribution_info *array_info, void **base_ptr_pos,
                                  int DIM, size_t elem_size)
{
#ifdef ARRAY_PROFILING
    array_info->array_profiling_info = init_array_profiling_info();
#endif

    void *array = nullptr;

    if (my_rank == 0)
    {
        Debug(printf("Distribute Array\n");) array =
            *base_ptr_pos; // array ptr is given only at master

        // TODO auf N-D verallgemeinern:
        if (DIM > 1 && check_2d_array_memory_allocation_style(array) != CONTINUOUS)
        {
            printf("Non-Continous array "
                   "distribution is not "
                   "supported yet\n");
            return;
            // lateŕ we need to Bcast the
            // result of the allocation
            // style check if non contigous is supported
        }
    }

    // first: need to get DIM from master
    array_info->DIM = DIM; // only useful on master
    MPI_Bcast(&array_info->DIM, 1, MPI_INT, 0, MPI_COMM_WORLD);

#ifndef DESTROY_GLOBAL_ARRAY_IN_PARALLEL_REGION
    array_info->orig_master_array_ptr = array;
    // will result nullptr on worker
#endif
    array_info->elem_size = elem_size;

    array_info->NDIM = (int *)malloc(array_info->DIM * sizeof(int));
    assert(array_info->NDIM != nullptr);

    get_array_NDIM_form_master(array, array_info->DIM, array_info->elem_size, array_info->NDIM,
                               &array_info->D1_elem_size);
    array_info->D1_count = array_info->NDIM[0]; // no need to
                                                // bcast this as
                                                // well
    assert(array_info->D1_elem_size < INT_MAX &&
           "distributing objects larger then INT_MAX is currently not supported");

    // global inclusive range
    array_info->own_upper = 0;
    array_info->own_lower = array_info->D1_count - 1;
    calculate_array_boundaries(&array_info->own_upper, &array_info->own_lower);
    long own_size = array_info->own_lower - array_info->own_upper + 1;
    if (array_info->own_upper == -1)
    {
        assert(array_info->own_lower == -1);
        own_size = 0;
    }

    Debug(printf("Rank %d got %ld lines from "
                 "%ld to %ld (inclusive)\n",
                 my_rank, own_size, array_info->own_upper, array_info->own_lower);)

        // ALL ranks will reserve a local buffer
        // reserve enough space for the first dimension
        if (array_info->DIM == 1)
    {
        *base_ptr_pos = alloc_array(array_info->DIM, array_info->NDIM, elem_size);
        // TODO is memset 0 needed here?
    }
    else
    {
        void **first_dim = (void **)calloc(array_info->D1_count, sizeof(void *));
        if (own_size != 0)
        {
            array_info->NDIM[0] = own_size;
            Debug(printf("Allocating local "
                         "%d-D array part "
                         "with size_of_dim",
                         array_info->DIM);
                  for (int t = 0; t < array_info->DIM; ++t) {
                      printf(" %d", array_info->NDIM[t]);
                  } printf(" and elem "
                           "size %lu\n",
                           elem_size);) void **local_part =
                (void **)alloc_array(array_info->DIM, array_info->NDIM, elem_size);
            // copy ptr to own rows to
            // correct location in global
            // array
            for (int i = 0; i < own_size; ++i)
            {
                first_dim[i + array_info->own_upper] = local_part[i];
            }
            // This only frees the first
            // dimension of local part,
            // as the content was copied
            // above
            free(local_part);
        }

        *base_ptr_pos = (void *)first_dim;
    }

    void *data_level_ptr = nullptr;
    if (own_size != 0)
    {
        data_level_ptr = get_data_lvl_ptr(array_info, base_ptr_pos, array_info->own_upper);
    }

    array_info->upper_lines = (long *)malloc(numprocs * sizeof(long));
    assert(array_info->upper_lines != nullptr);
    MPI_Allgather(&array_info->own_upper, 1, MPI_LONG, array_info->upper_lines, 1, MPI_LONG,
                  MPI_COMM_WORLD);

    void **master_data_level_ptr = (void **)array; // = null on worker
    if (my_rank == 0)
    {
        // we start at 1 as we are already at lvl 1
        for (int i = 1; i < array_info->DIM; ++i)
        {
            master_data_level_ptr = (void **)master_data_level_ptr[0];
            // printf("master decent one
            // lvl to %p\n", my_rank,
            // (void *)data_level_ptr);
        }
    } // else master_data_level_ptr was
      // set null and not be used anyway

    long my_sendcount = 0;
    // distribution of the array data.
    // Temporary window used for distribution of the array
    MPI_Win temp_win;
    if (my_rank == 0)
    {
        MPI_Win_create(master_data_level_ptr, array_info->D1_elem_size * array_info->D1_count,
                       1, MPI_INFO_NULL, MPI_COMM_WORLD, &temp_win);
    }
    else
    {
        MPI_Win_create(nullptr, 0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &temp_win);
    }
    MPI_Win_fence(0, temp_win);
    // MPI_Win_fence(MPI_MODE_NOPUT, temp_win);

    // get each line from master using the temp win
    for (long i = array_info->own_upper; i <= array_info->own_lower; ++i)
    {
        if (i != -1)
        { // then nothing do do
            my_sendcount += array_info->D1_elem_size;
            void *rcev_buffer = get_data_lvl_ptr(array_info, base_ptr_pos, i);
            MPI_Get(rcev_buffer, array_info->D1_elem_size, MPI_BYTE, 0,
                    array_info->D1_elem_size * i, array_info->D1_elem_size, MPI_BYTE,
                    temp_win);
        }
    }

    MPI_Win_fence(0, temp_win);
    MPI_Win_free(&temp_win);

    // displacement unit is 1 (=Bytes)
    MPI_Win_create(data_level_ptr, my_sendcount, 1, MPI_INFO_NULL, MPI_COMM_WORLD,
                   &array_info->win);

#ifdef DESTROY_GLOBAL_ARRAY_IN_PARALLEL_REGION
    if (my_rank == 0)
    {
        // not needed in parallel region
        // will alocate new one when
        // gather
        free_array(array, array_info->DIM);
    }
#endif

    // init location_info
    array_info->location_info = (int *)malloc(array_info->D1_count * sizeof(int));
    assert(array_info->location_info != nullptr);

    array_info->currently_cached = new std::vector<long>;
    array_info->currently_cached->reserve(
        RESERVE_CACHED_SPACE); // This number may be found out with profiling

    int curr_rank = 0;
    for (long i = 0; i < array_info->D1_count; ++i)
    {
        if (i == array_info->upper_lines[curr_rank + 1])
        {
            // from now on lines reside
            // on the next rank
            // TODO if some rank in
            // between has 0 lines this
            // will fail
            //(currently this can not
            // happen)
            curr_rank++;
        }
        array_info->location_info[i] = curr_rank; // own line is
                                                  // always present
                                                  // on this rank
        if (curr_rank == my_rank)
        {
            array_info->location_info[i] += numprocs; // line is
                                                      // present at
                                                      // own rank
        }
    }
    // NO assertion that last rank owns
    // at least one row
    /*
        sleep(my_rank);
        printf("rank %d cache:", my_rank);
        for (int i = 0; i < array_info->D1_count; ++i)
        {
            printf(" %d", array_info->location_info[i]);
        }
        printf("\n");
    */
    // needed for later allocation of
    // just one line/plain/cube/...
    array_info->NDIM[0] = 1;
    // therefore original value was
    // stored in D1_count

    Debug(if (my_rank == 0) { printf("Array was distributed\n"); })

        MPI_Win_lock_all(0, array_info->win); // start exposure
                                              // epoch
}

// suggest to inline this
void distribute_shared_array_from_master(void *comm_info, size_t buffer_type_size, int DIM,
                                         size_t array_elem_size)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct
    // comm_info = location off array base ptr (offset 0)
    init_array_distribution_info(array_info, (void **)comm_info, DIM, array_elem_size);
}

// TODO Rename it ?
// suggest to not inline this
void free_distributed_shared_array_from_master(array_distribution_info *array_info,
                                               void **base_ptr_pos)
{
    MPI_Win_unlock_all(array_info->win); // end exposure epoch
                                         // = wait for all
                                         // outstanding RMA
                                         // calls to finish
#ifdef ARRAY_PROFILING
    print_array_profiling_information(array_info->array_profiling_info);
#endif

    int own_size = array_info->own_lower - array_info->own_upper + 1; // +1:range is inclusive
    if (array_info->own_upper == -1)
    {
        assert(array_info->own_lower == -1);
        own_size = 0;
    }

    // as the OpenMP threads may
    // reallocate a shared array (e.g. to
    // extend its size) this behaviour
    // should not break the serial code
    void **master_data_level_ptr = nullptr;
    void **new_array_at_master = nullptr;
    if (my_rank == 0)
    {
        array_info->NDIM[0] = array_info->D1_count; // set back
                                                    // NDIM to
                                                    // original
                                                    // size
        Debug(printf("Master allocates the "
                     "full %d-D array again "
                     "with size_of_dim",
                     array_info->DIM);
              for (int t = 0; t < array_info->DIM;
                   ++t) { printf(" %d", array_info->NDIM[t]); } printf(" and elem size "
                                                                       "%lu\n",
                                                                       array_info->elem_size);)

#ifdef DESTROY_GLOBAL_ARRAY_IN_PARALLEL_REGION
            // allocate new array
            new_array_at_master =
                (void **)alloc_array(array_info->DIM, array_info->NDIM, array_info->elem_size);
#else
            new_array_at_master = array_info->orig_master_array_ptr;
        // reuse old ptr
#endif
        // decent to data Layer
        master_data_level_ptr = new_array_at_master;
        // start at lv 1
        for (int i = 1; i < array_info->DIM; ++i)
        {
            master_data_level_ptr = (void **)master_data_level_ptr[0];
        }
    }

    // gather
    // same code as for scatter but using MPI put instead of get
    MPI_Win temp_win;
    if (my_rank == 0)
    {
        MPI_Win_create(master_data_level_ptr, array_info->D1_elem_size * array_info->D1_count,
                       1, MPI_INFO_NULL, MPI_COMM_WORLD, &temp_win);
    }
    else
    {
        MPI_Win_create(nullptr, 0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &temp_win);
    }
    MPI_Win_fence(0, temp_win);

    // get each line from master using the temp win
    for (long i = array_info->own_upper; i <= array_info->own_lower; ++i)
    {
        if (i != -1)
        {
            void *send_buffer = get_data_lvl_ptr(array_info, base_ptr_pos, i);
            MPI_Put(send_buffer, array_info->D1_elem_size, MPI_BYTE, 0,
                    array_info->D1_elem_size * i, array_info->D1_elem_size, MPI_BYTE,
                    temp_win);
        }
    }

    MPI_Win_fence(0, temp_win);
    MPI_Win_free(&temp_win);

    free(array_info->upper_lines);

    MPI_Win_free(&array_info->win);
    free(array_info->NDIM);
    free(array_info->location_info);
    delete array_info->currently_cached;

    // free local array part
    if (array_info->DIM == 1)
    {
        free(*base_ptr_pos);
    }
    else
    {
        void *line_ptr;
        // free each line independently
        for (int i = 0; i < array_info->own_upper; ++i)
        {
            line_ptr = ((void **)(*base_ptr_pos))[i];
            if (line_ptr != nullptr)
            {
                free_array(line_ptr, array_info->DIM - 1);
            }
        }
        // own data was allocated en
        // Block
        if (array_info->own_upper != -1)
        {
            line_ptr = ((void **)(*base_ptr_pos))[array_info->own_upper];
            if (line_ptr != nullptr) // may be null if
                                     // own_size==0
            {
                free_array(line_ptr, array_info->DIM - 1);
            }
        }
        for (int i = array_info->own_lower + 1; i < array_info->D1_count; ++i)
        {
            line_ptr = ((void **)(*base_ptr_pos))[i];
            if (line_ptr != nullptr)
            {
                free_array(line_ptr, array_info->DIM - 1);
            }
        }
        free(*base_ptr_pos);
    }
    // reset the ptr at master (will set nullptr on worker)
    *base_ptr_pos = new_array_at_master;
}

// suggest to inline this
void free_distributed_shared_array_from_master(void *comm_info, size_t buffer_type_size)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct
    // comm_info = location off array base ptr (offset 0)
    free_distributed_shared_array_from_master(array_info, (void **)comm_info);
}

// this will also give tgt_displacement for the line idx
// suggest to inline this
int get_target_rank_of_array_line(array_distribution_info *array_info, long idx,
                                  long *target_disp)
{
    long chunk_size = array_info->D1_count / numprocs;
    long rem = array_info->D1_count % numprocs;

    int tgt_rank = array_info->location_info[idx];
    if (tgt_rank >= numprocs)
    {
        tgt_rank -= numprocs;
    }

    // this is based on the fact that each rank own a contigouus chunk of the data
    long tgt_upper = array_info->upper_lines[tgt_rank];
    *target_disp = (idx - tgt_upper) * array_info->D1_elem_size;

    Debug(int my_rank; MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
          printf("Rank %d access array line "
                 "%ld which is %lds line of "
                 "rank %d\n",
                 my_rank, idx, (idx - tgt_upper), tgt_rank);) return tgt_rank;
}

// suggest to not inline this
__attribute__((noinline)) void load_shared_array_line(array_distribution_info *array_info,
                                                      void **base_ptr_pos, long idx)
{
    assert(!(idx >= array_info->own_upper && idx <= array_info->own_lower) &&
           "There should not be "
           "communication when accessing "
           "an owned line\n");
    if (array_info->DIM == 1)
    {
        long tgt_disp;
        int tgt_rank = get_target_rank_of_array_line(array_info, idx, &tgt_disp);
        assert(array_info->D1_elem_size == array_info->elem_size);
        assert(tgt_rank != my_rank); // no comm if I already own the line

        // Load element directly: get
        // correct element location char*
        // to get correct offset in bytes
        void *elem_loc = &(((char *)(*base_ptr_pos))[idx * array_info->D1_elem_size]);

        MPI_Get(elem_loc, array_info->D1_elem_size, MPI_BYTE, tgt_rank, tgt_disp,
                array_info->D1_elem_size, MPI_BYTE, array_info->win);
        MPI_Win_unlock_all(array_info->win); // end exposure
                                             // epuch = wait
                                             // for all
                                             // outstanding
                                             // RMA calls to
                                             // finish
        MPI_Win_lock_all(0,
                         array_info->win); // start exposure
                                           // epoch
    }
    else
    {
        void **ptr_to_line = &(((void **)(*base_ptr_pos))[idx]);
        void *line = *ptr_to_line;
        if (line == nullptr)
        {
            assert(array_info->NDIM[0] == 1);
            Debug(printf("Allocating local "
                         "%d-D array part "
                         "with size_of_dim",
                         array_info->DIM);
                  for (int t = 0; t < array_info->DIM; ++t) {
                      printf(" %d", array_info->NDIM[t]);
                  } printf(" and elem "
                           "size %lu\n",
                           array_info->elem_size);)
                // allocate new local
                // buffer
                line = alloc_array(array_info->DIM, array_info->NDIM, array_info->elem_size);
            // has only one line
            *ptr_to_line = ((void **)line)[0];
            free(line); // frees only the
                        // first layer
                        // (the ptr to
                        // line) as it was
                        // copied above

        } // else buffer for this line is
          // already present
        long tgt_disp;
        int tgt_rank = get_target_rank_of_array_line(array_info, idx, &tgt_disp);
        // decent to data_layer
        void *data_level_ptr = get_data_lvl_ptr(array_info, base_ptr_pos, idx);
        // finally load the data
        MPI_Get(data_level_ptr, array_info->D1_elem_size, MPI_BYTE, tgt_rank, tgt_disp,
                array_info->D1_elem_size, MPI_BYTE, array_info->win);
        MPI_Win_unlock_all(array_info->win); // end exposute
                                             // epoch = wait
                                             // for all
                                             // outstanding
                                             // RMA calls to
                                             // finish
        MPI_Win_lock_all(0,
                         array_info->win); // start exposure
                                           // epoch
    }

    array_info->location_info[idx] += numprocs;
    array_info->currently_cached->push_back(idx);
}

// loads one element/line/plain/cube/...
// form another rank if needed
// suggest to inline this
void cache_shared_array_line(void *comm_info, size_t buffer_type_size, long idx)
{
    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    // __builtin_expect Tell optimizer it should
    // optimize this assuming condition is likely to be false
    if (__builtin_expect(!(array_info->location_info[idx] >= numprocs), 0))
    {
        load_shared_array_line(array_info, (void **)comm_info, idx);
#ifdef ARRAY_PROFILING
        array_info->array_profiling_info->num_cache_misses++;
#endif
    }
#ifdef ARRAY_PROFILING
    else
    {
        array_info->array_profiling_info->num_cache_hits++;
    }
#endif
}

// suggest inline this
void invlaidate_shared_array_cache(void *comm_info, size_t buffer_type_size)
{
    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

#ifdef ARRAY_PROFILING
    array_info->array_profiling_info->num_barriers++;
#endif

    MPI_Win_unlock_all(array_info->win);  // end exposute epoch
                                          // = wait for all
                                          // outstanding RMA
                                          // calls to finish
    MPI_Win_lock_all(0, array_info->win); // start next
                                          // exposure epoch
    for (auto i : *array_info->currently_cached)
    {
        Debug(int my_rank; MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); printf("Rank %d "
                                                                           "invalidates "
                                                                           "line %ld\n",
                                                                           my_rank, i);)
            array_info->location_info[i] -= numprocs;
#ifdef ARRAY_PROFILING
        array_info->array_profiling_info->num_cache_invalidation++;
#endif
    }
    array_info->currently_cached->clear();
}

// suggest inline this
// function that do not require buffer_type_size as arg (as array buffer size =
// sizeof(pointertype)
void invlaidate_shared_array_cache(void *comm_info)
{
    invlaidate_shared_array_cache(comm_info, sizeof(void *));
}
// suggest to not inline this
__attribute__((noinline)) void
store_to_remote_shared_array_line(array_distribution_info *array_info, void **base_ptr_pos,
                                  long idx, void *store_address)
{
    assert(!(idx >= array_info->own_upper && idx <= array_info->own_lower) &&
           "There should not be "
           "communication when accessing "
           "an owned line\n");

    long tgt_line_disp;
    int tgt_rank = get_target_rank_of_array_line(array_info, idx, &tgt_line_disp);

    void *data_level_ptr = get_data_lvl_ptr(array_info, base_ptr_pos, idx);
    // this means data level ptr point to
    // the beginning of the current
    // elem/line/plane/cube/...

    assert(store_address >= data_level_ptr); // must be
                                             // ptr within
                                             // that line
    // displacement within the current
    // line
    unsigned long local_disp =
        ((unsigned long)store_address) - ((unsigned long)data_level_ptr);
    // printf("local disp= %lu
    // line_start: %p, loc:%p\n",
    // local_disp, data_level_ptr,
    // store_address);

    assert(local_disp < array_info->D1_elem_size); // local
                                                   // disp is
                                                   // only the
                                                   // displacement
                                                   // within
                                                   // one line

    long disp = tgt_line_disp + local_disp;

    MPI_Put(store_address, array_info->elem_size, MPI_BYTE, tgt_rank, disp,
            array_info->elem_size, MPI_BYTE, array_info->win);
    // we do not force immediate
    // completion on this RMA operarion
    // it may continiue in the background
}

// applys the store to a remote array
// line suggest inline this
void store_to_shared_array_line(void *comm_info, size_t buffer_type_size, long idx,
                                void *store_address)
{
    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    // __builtin_expect Tell optimizer it should
    // optimize this assuming condition is likely to be false
    if (__builtin_expect(!(array_info->location_info[idx] == rank_and_size), 0))
    {
        store_to_remote_shared_array_line(array_info, (void **)comm_info, idx, store_address);
#ifdef ARRAY_PROFILING
        array_info->array_profiling_info->num_false_rank_writes++;
#endif
    }
#ifdef ARRAY_PROFILING
    else
    {
        array_info->array_profiling_info->num_own_rank_writes++;
    }
#endif
}

long get_own_lower(void *comm_info, size_t buffer_type_size)
{
    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct
    return array_info->own_lower;
}

long get_own_upper(void *comm_info, size_t buffer_type_size)
{
    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct
    return array_info->own_upper;
}

bool is_array_row_own(void *comm_info, size_t buffer_type_size, long idx)
{
    // same condition as store_to_shared_array_line
    // exept just the check wetehr comm is needed

    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    return (array_info->location_info[idx] == rank_and_size);
}

// same as get_data_lvl_ptr above but for bcasted arrays
void *get_data_lvl_ptr(bcasted_array_info *array_info, void **base_ptr_pos)
{
    if (array_info->DIM == 1) // get pointer to elem
    {
        // as char* so indexing element size is byte
        char *as_char = (char *)(*base_ptr_pos);
        return &as_char[0];
    }
    else
    {
        void **current_lvl = &(((void **)(*base_ptr_pos))[0]);
        // decent down to data lvl (starting with 1 as we are currently in lvl 1)
        for (int i = 1; i < array_info->DIM; ++i)
        {
            current_lvl = (void **)(current_lvl[0]);
        }
        // printf("Dim:%d Data-Level[%lu]:%p\n", array_info->DIM,idx, current_lvl);
        return (void *)current_lvl;
    }
}

// suggest inline this
void invlaidate_shared_array_cache_release_mem(void *comm_info, size_t buffer_type_size)
{
    array_distribution_info *array_info =
        (array_distribution_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

#ifdef ARRAY_PROFILING
    array_info->array_profiling_info->num_barriers++;
#endif

    MPI_Win_unlock_all(array_info->win);  // end exposute epoch
                                          // = wait for all
                                          // outstanding RMA
                                          // calls to finish
    MPI_Win_lock_all(0, array_info->win); // start next
                                          // exposure epoch
    for (auto i : *array_info->currently_cached)
    {
        Debug(int my_rank; MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); printf("Rank %d "
                                                                           "invalidates "
                                                                           "line %ld\n",
                                                                           my_rank, i);)
            array_info->location_info[i] -= numprocs;
#ifdef ARRAY_PROFILING
        array_info->array_profiling_info->num_cache_invalidation++;
#endif
        // free local buffer at position i
        if (array_info->DIM > 1)
        {
            void *line_ptr = ((void **)(*(void **)comm_info))[i];
            free_array(line_ptr, array_info->DIM - 1);
            ((void **)(*(void **)comm_info))[i] =
                nullptr; // set it to null as no buffer is present
        }
    }
    array_info->currently_cached->clear();
}

void bcast_array_from_master(void *comm_info, size_t buffer_type_size, int DIM,
                             size_t array_elem_size)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    bcasted_array_info *array_info =
        (bcasted_array_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    //
    void **base_ptr_pos = (void **)comm_info;
    // array is given at first pos of comm_info

    void *array = nullptr;

    if (my_rank == 0)
    {
        Debug(printf("Bcast Array\n");) array =
            *base_ptr_pos; // array ptr is given only at master

        // TODO auf N-D verallgemeinern:
        if (DIM > 1 && check_2d_array_memory_allocation_style(array) != CONTINUOUS)
        {
            printf("Non-Continous array "
                   "distribution is not "
                   "supported yet\n");
            return;
            // lateŕ we need to Bcast the
            // result of the allocation
            // style check if non contigous is supported
        }
    }

    // first: need to get DIM from master
    array_info->DIM = DIM; // only useful on master
    MPI_Bcast(&array_info->DIM, 1, MPI_INT, 0, MPI_COMM_WORLD);

    array_info->elem_size = array_elem_size;

    int *NDIM = (int *)malloc(array_info->DIM * sizeof(int));
    assert(NDIM != nullptr);
    size_t D1_elem_size;

    get_array_NDIM_form_master(array, array_info->DIM, array_info->elem_size, NDIM,
                               &D1_elem_size);

    size_t total_data_size = D1_elem_size * NDIM[0];

    if (my_rank != 0)
    {
        // master already has array
        *base_ptr_pos = alloc_array(array_info->DIM, NDIM, array_info->elem_size);
    }

    void *data_lvl = get_data_lvl_ptr(array_info, base_ptr_pos);

    mpi_bcast_byte_long(data_lvl, total_data_size, /*MPI_BYTE,*/ 0, MPI_COMM_WORLD);

    MPI_Win_create(data_lvl, total_data_size, 1, MPI_INFO_NULL, MPI_COMM_WORLD,
                   &array_info->win);

    free(NDIM);

    MPI_Barrier(MPI_COMM_WORLD);
}

void free_bcasted_array_from_master(void *comm_info, size_t buffer_type_size)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    bcasted_array_info *array_info =
        (bcasted_array_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    MPI_Win_free(&array_info->win);

    // master will keep his array
    if (my_rank != 0)
    {
        free_array(*(void **)comm_info, array_info->DIM);
    }
}

void store_to_bcasted_array_line(void *comm_info, size_t buffer_type_size, long idx,
                                 void *store_address)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    bcasted_array_info *array_info =
        (bcasted_array_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    void *data_level_ptr = get_data_lvl_ptr(array_info, (void **)comm_info);

    unsigned long disp = ((unsigned long)store_address) - ((unsigned long)data_level_ptr);

    mpi_one_sided_bcast(store_address, disp, &array_info->win, array_info->elem_size,
                        MPI_BYTE);
}

void *get_data_lvl_ptr(master_based_array_info *array_info, void **base_ptr_pos, long idx)
{
    if (array_info->DIM == 1) // get pointer to elem
    {
        // as char* so indexing element size is byte
        char *as_char = (char *)(*base_ptr_pos);
        return &as_char[idx * array_info->elem_size];
    }
    else
    {
        void **current_lvl = &(((void **)(*base_ptr_pos))[idx]);
        // decent down to data lvl (starting with 1 as we are currently in lvl 1)
        for (int i = 1; i < array_info->DIM; ++i)
        {
            current_lvl = (void **)(current_lvl[0]);
        }
        // printf("Dim:%d Data-Level[%lu]:%p\n", array_info->DIM,idx, current_lvl);
        return (void *)current_lvl;
    }
}

// suggest to inline this
void init_master_based_array_info(void *comm_info, size_t buffer_type_size, int DIM,
                                  size_t array_elem_size)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    master_based_array_info *array_info =
        (master_based_array_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct
    // comm_info = location off array base ptr (offset 0)

    void **base_ptr_pos = (void **)comm_info;

    void *array = nullptr;

    if (my_rank == 0)
    {
        Debug(printf("Master based Array\n");) array =
            *base_ptr_pos; // array ptr is given only at master

        // TODO auf N-D verallgemeinern:
        if (DIM > 1 && check_2d_array_memory_allocation_style(array) != CONTINUOUS)
        {
            printf("Non-Continous array "
                   "distribution is not "
                   "supported yet\n");
            return;
            // lateŕ we need to Bcast the
            // result of the allocation
            // style check if non contigous is supported
        }
    }

    // first: need to get DIM from master
    array_info->DIM = DIM; // only useful on master
    MPI_Bcast(&array_info->DIM, 1, MPI_INT, 0, MPI_COMM_WORLD);

    array_info->elem_size = array_elem_size;

    array_info->NDIM = (int *)malloc(array_info->DIM * sizeof(int));
    assert(array_info->NDIM != nullptr);

    get_array_NDIM_form_master(array, array_info->DIM, array_info->elem_size, array_info->NDIM,
                               &array_info->D1_elem_size);
    array_info->D1_count = array_info->NDIM[0]; // no need to
                                                // bcast this as
                                                // well

    // allocate buffer for first dim
    if (my_rank != 0) // master have the whole array
    {
        if (array_info->DIM == 1)
        {
            *base_ptr_pos =
                alloc_array(array_info->DIM, array_info->NDIM, array_info->elem_size);
            // TODO is memset 0 needed here?
        }
        else
        {
            void **first_dim = (void **)calloc(array_info->D1_count, sizeof(void *));

            *base_ptr_pos = (void *)first_dim;
        }
    }

    int my_sendcount = 0; // worker will expose 0 mem
    void *data_level_ptr = nullptr;
    if (my_rank == 0)
    {
        data_level_ptr = get_data_lvl_ptr(array_info, base_ptr_pos, 0);
        my_sendcount = array_info->D1_elem_size * array_info->D1_count;
    }

    // displacement unit is 1 (=Bytes)
    MPI_Win_create(data_level_ptr, my_sendcount, 1, MPI_INFO_NULL, MPI_COMM_WORLD,
                   &array_info->win);

    int tgt_rank = 0;
    if (my_rank != tgt_rank)
    {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, tgt_rank, 0, array_info->win); // start exposue epoch
        // exclusive lock for writing operations as this parrern is meant for writing
    }

    array_info->NDIM[0] = 1; // for allocation of one line at the time
}

void free_master_based_array(void *comm_info, size_t buffer_type_size)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    master_based_array_info *array_info =
        (master_based_array_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    void **base_ptr_pos = (void **)comm_info;
    int tgt_rank = 0;
    if (my_rank != tgt_rank)
    {
        MPI_Win_unlock(tgt_rank,
                       array_info->win); // end exposure epoch = end all outstanding rma
    }
    MPI_Win_free(&array_info->win);

    // master will keep his array
    if (my_rank != 0)
    {
        if (array_info->DIM == 1)
        {
            free(*base_ptr_pos);
        }
        else
        {
            void *line_ptr;
            // free each line independently
            for (int i = 0; i < array_info->D1_count; ++i)
            {
                line_ptr = ((void **)(*base_ptr_pos))[i];
                if (line_ptr != nullptr)
                {
                    free_array(line_ptr, array_info->DIM - 1);
                }
            }
        }
    }
}

void store_to_master_based_array_line(void *comm_info, size_t buffer_type_size, long idx,
                                      void *store_address)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    master_based_array_info *array_info =
        (master_based_array_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    int tgt_rank = 0;
    if (my_rank != tgt_rank)
    {
        void *data_level_ptr = get_data_lvl_ptr(array_info, (void **)comm_info, idx);

        unsigned long disp = ((unsigned long)store_address) - ((unsigned long)data_level_ptr);

        disp = disp + array_info->D1_elem_size * idx; // add displacement for current line

        MPI_Put(store_address, array_info->elem_size, MPI_BYTE, tgt_rank, disp,
                array_info->elem_size, MPI_BYTE, array_info->win);
    }
}
void load_from_master_based_array_line(void *comm_info, size_t buffer_type_size, long idx)
{
    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    master_based_array_info *array_info =
        (master_based_array_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    int tgt_rank = 0;
    if (my_rank != tgt_rank)
    {
        void **base_ptr_pos = (void **)comm_info;
        // check if there is a buffer for this line:
        if (array_info->DIM > 1)
        {
            void **ptr_to_line = &(((void **)(*base_ptr_pos))[idx]);
            void *line = *ptr_to_line;
            if (line == nullptr)
            {
                assert(array_info->NDIM[0] == 1);
                Debug(printf("Allocating local "
                             "%d-D array part "
                             "with size_of_dim",
                             array_info->DIM);
                      for (int t = 0; t < array_info->DIM; ++t) {
                          printf(" %d", array_info->NDIM[t]);
                      } printf(" and elem "
                               "size %lu\n",
                               array_info->elem_size);)
                    // allocate new local
                    // buffer
                    line =
                        alloc_array(array_info->DIM, array_info->NDIM, array_info->elem_size);
                // has only one line
                *ptr_to_line = ((void **)line)[0];
                free(line); // frees only the
                            // first layer
                            // (the ptr to
                            // line) as it was
                            // copied above
            }               // else buffer for this line is
                            // already present
        }

        void *data_level_ptr = get_data_lvl_ptr(array_info, (void **)comm_info, idx);

        unsigned long disp =
            array_info->D1_elem_size * idx; // add displacement for current line

        // load a whole line
        // master based arrays have no chaching therefore whole line will be loaded again and
        // again they are optimized for writing
        MPI_Get(data_level_ptr, array_info->D1_elem_size, MPI_BYTE, tgt_rank, disp,
                array_info->D1_elem_size, MPI_BYTE, array_info->win);

        MPI_Win_unlock(tgt_rank,
                       array_info->win); // end exposure epoch to make shure data is present
        MPI_Win_lock(MPI_LOCK_SHARED, tgt_rank, 0, array_info->win); // start new one
    }
}

void sync_master_based_array(void *comm_info, size_t buffer_type_size)
{

    assert(buffer_type_size == sizeof(void **)); // buffer should be a ptr
    // char* for correct pointer arithmetic (byt as base unit)
    master_based_array_info *array_info =
        (master_based_array_info
             *)((char *)comm_info +
                buffer_type_size); // offset to acces the communication info struct

    int tgt_rank = 0;
    if (my_rank != tgt_rank)
    {
        MPI_Win_unlock(tgt_rank, array_info->win); // end exposure epoch to achive a
                                                   // synchronization of all rma operations
        MPI_Win_lock(MPI_LOCK_SHARED, tgt_rank, 0, array_info->win); // start new one

        // if it is assured no load will occur one may even use fence synchronization
    }
}
