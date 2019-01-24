#ifndef RTLIB_PROFILING_H_
#define RTLIB_PROFILING_H_

// should be defined in rtlib if one want to use it
//#define ARRAY_PROFILING

#ifdef ARRAY_PROFILING
typedef struct
{

    unsigned long num_cache_misses;
    unsigned long num_cache_hits;

    unsigned long num_cache_invalidation;
    unsigned long num_barriers;

    unsigned long num_false_rank_writes;
    unsigned long num_own_rank_writes;

} array_profiling_info;

array_profiling_info *init_array_profiling_info();
void print_array_profiling_information(array_profiling_info *info);
#endif

#ifdef TASK_PROFILING
typedef struct
{
    double time_spend_working;
    double time_spend_waiting;

    unsigned long tasks_done;
    unsigned long tasks_created;

} task_profiling_info;

task_profiling_info *init_task_profiling_info();
void print_task_profiling_information(task_profiling_info *info);
#endif

#endif /* RTLIB_PROFILING_H_ */
