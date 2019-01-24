#include <assert.h>
#include <map>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memcpy
#include <utility>

#include "rtlib-profiling.h"
#include "rtlib.h"

#ifdef ARRAY_PROFILING
array_profiling_info *init_array_profiling_info()
{
    return (array_profiling_info *)calloc(sizeof(array_profiling_info), 1);
}

// prints information and free the profiling info
void print_array_profiling_information(array_profiling_info *info)
{
    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    double rate =
        (double)info->num_cache_misses / (info->num_cache_misses + info->num_cache_hits);
    printf("Rank %d: reading cache: %lu hits %lu misses = miss rate of %f \n", my_rank,
           info->num_cache_hits, info->num_cache_misses, rate);

    rate = (double)info->num_false_rank_writes /
           (info->num_own_rank_writes + info->num_false_rank_writes);
    printf("Rank %d writing cache: %lu hits %lu misses = miss rate of %f \n", my_rank,
           info->num_own_rank_writes, info->num_false_rank_writes, rate);

    rate = (double)info->num_cache_invalidation / (info->num_barriers);
    printf("Cache Invlaidation Statistic for Rank %d:  %lu barriers %lu invalidations = avg "
           "%f per Barrier \n",
           my_rank, info->num_barriers, info->num_cache_invalidation, rate);

    // TODO combine results from different ranks

    free(info);
}

#endif

#ifdef TASK_PROFILING
task_profiling_info *init_task_profiling_info()
{
    task_profiling_info *profiling_info =
        (task_profiling_info *)malloc(sizeof(task_profiling_info));
    assert(profiling_info != nullptr);
    profiling_info->tasks_created = 0;
    profiling_info->tasks_done = 0;
    profiling_info->time_spend_working = 0;
    profiling_info->time_spend_waiting = 0;
    return profiling_info;
}

// prints information and free the profiling info
void print_task_profiling_information(task_profiling_info *info)
{
    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    unsigned long num_all_tasks_created = 0;
    MPI_Allreduce(&info->tasks_created, &num_all_tasks_created, 1, MPI_UNSIGNED_LONG, MPI_SUM,
                  MPI_COMM_WORLD);

    double rate = (double)info->tasks_done / (num_all_tasks_created);
    // print rate * 100 to give values in percent
    printf("Rank %d: worked on %f percent; %lu out of of the %lu Tasks\n", my_rank, rate * 100,
           info->tasks_done, num_all_tasks_created);

    rate = info->time_spend_waiting / (info->time_spend_waiting + info->time_spend_working);
    printf("Rank %d has waited %f percent (%f seconds compared to %f seconds working) \n",
           my_rank, rate * 100, info->time_spend_waiting, info->time_spend_working);

    free(info);
}
#endif
