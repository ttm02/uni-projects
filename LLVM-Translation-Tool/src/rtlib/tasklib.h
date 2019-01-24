#ifndef TASKLIB_H_
#define TASKLIB_H_

#include "mpi_mutex.h"
#include "rtlib.h"
#include <mpi.h>

#include "task_info_list.h"

// use ONE define to define the task scheduling strategy
//#define USE_SIMPLE_SCHEDULE
#define USE_POLLING_SCHEDULE

// if defined it will call uspeep between two polls in order to lower communication amount
// therefore it is strongly advised to using it!!
#define SLEEP_BETWEEN_POLLING
#define MAX_SLEEP_BETWEEN_POLLING_TIME 1000

#define TASK_LIB_MSG_TAG 98765

/*
//DEFINED in task_info_list.h:
// information about a single task to call
typedef struct task_info_struct
{
    int task_callback_func_id;
    int num_shared_vars;

    struct mpi_struct_ptr shared_param_list;
    struct mpi_struct_ptr private_param_list;
    // LOCAL Pointers should only be used on the rank constructing the lists
    struct task_param_list_struct *LOCAL_private_param_list_tail_ptr;
} task_info;
*/

typedef void (*sync_point_callback_func)(void *param);

struct task_param_list_struct
{
    // ptr to the shared param:
    struct mpi_struct_ptr param_ptr;
    // ptr to next list elem
    struct mpi_struct_ptr next_element;
};
// Parameterlist cannot be linked list because they are not known to all threads when creating
// them

// struct that contain the info for tasking
typedef struct global_tasking_info_struct
{
    int basis_rank = 0;
    MPI_Mutex *mutex = nullptr;
    task_info_list *waiting_task_list = nullptr;

    // so that memory can be freed if task where done
    int local_memloc_buffer_size = 0;
    int local_memloc_buffer_num_entry = 0;
    void **local_memloc_buffer = nullptr;

    // dynamic win where all private params are supposed to be communicated with
    MPI_Win parameter_win;

    // currently not used:
    // MPI_Win process_status_win;
    // content is only relevant on basis rank:
    // array with size = numproc +1
    // int *process_status = nullptr;
    // process_status[0] = waiting_process_count

    // list of all variables shared in the current parallel region
    void **shared_vars;
    // list of callback functions to use when a syncronization point is reached
    sync_point_callback_func *callback_funcs;
    int num_shared_vars;

#ifdef TASK_PROFILING
    task_profiling_info *profiling_info;
#endif
} global_tasking_info;

// function that have to be inserted in IR by the pass;
void call_this_task(task_info_struct *);

#endif /* TASKLIB_H_ */
