#include <assert.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memcpy
#include <sys/time.h>
#include <unistd.h>

#include "rtlib-profiling.h"
#include "rtlib.h"
#include "tasklib.h"

// global variable that will hold the tasking info
global_tasking_info *tasking_info = nullptr;

// will contain functions that perform communication for the omp task pragmas

// collective Call: init the tasking info
// parameter win is the dynamic MPI win in which all the parameters should be communicated
// suggest to not inline this
void mpi_global_tasking_info_init(MPI_Win parameter_win, int basis_rank = 0)
{
    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    // printf("Rank%d init global task info\n", my_rank);
    tasking_info = new global_tasking_info();
    assert(tasking_info != nullptr);

    tasking_info->basis_rank = basis_rank;
    tasking_info->waiting_task_list = new task_info_list(basis_rank);
    MPI_Mutex_init(&tasking_info->mutex, basis_rank);

    tasking_info->local_memloc_buffer = nullptr;
    tasking_info->local_memloc_buffer_size = 0;
    tasking_info->local_memloc_buffer_num_entry = 0;
    tasking_info->parameter_win = parameter_win;

    // process status buffer is currently not used
    /*
    MPI_Info win_info;
    MPI_Info_create(&win_info);
    MPI_Info_set(win_info, "accumulate_ordering",
                 "none"); // we do not rely on a accumulate ordering
    // we only rely on that fetch and op is atomic
    MPI_Info_set(win_info, "accumulate_ops", "same_op_no_op"); // we only use SUM or NO_OP


    tasking_info->process_status = nullptr;
    if (my_rank == basis_rank)
    {
        int numprocs;
        MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
        size_t buffer_size =
            sizeof(int) * (numprocs + 1 + 1); // reserve space for 2 more counters
        tasking_info->process_status = (int *)malloc(buffer_size);
        assert(tasking_info->process_status != nullptr);
        memset(tasking_info->process_status, 0, buffer_size);

        MPI_Win_create(tasking_info->process_status, buffer_size, sizeof(int), win_info,
                       MPI_COMM_WORLD, &tasking_info->process_status_win);
    }
    else
    {
        MPI_Win_create(nullptr, 0, sizeof(int), win_info, MPI_COMM_WORLD,
                       &tasking_info->process_status_win);
    }

    MPI_Info_free(&win_info);
    */

    tasking_info->num_shared_vars = -1; // currently in serial region
    tasking_info->shared_vars = nullptr;

#ifdef TASK_PROFILING
    tasking_info->profiling_info = init_task_profiling_info();
#endif
}

// collective Call: free the tasking info
// (only call if there are no tasks left)
// suggest to not inline this
void mpi_global_tasking_info_destroy()
{
    // should come from serial code:
    assert(tasking_info->num_shared_vars == -1);

#ifdef TASK_PROFILING
    print_task_profiling_information(tasking_info->profiling_info);
#endif

    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    assert(tasking_info->waiting_task_list->is_empty());

    delete tasking_info->waiting_task_list;
    MPI_Mutex_destroy(tasking_info->mutex);

    MPI_Win_free(&tasking_info->parameter_win);
    if (tasking_info->local_memloc_buffer_size > 0)
    {
        for (int i = 0; i < tasking_info->local_memloc_buffer_num_entry; ++i)
        {
            free(tasking_info->local_memloc_buffer[i]);
        }
        free(tasking_info->local_memloc_buffer);
    }

    /*
    MPI_Win_free(&tasking_info->process_status_win);
    if (my_rank == tasking_info->basis_rank)
    {
        free(tasking_info->process_status);
    }
    */

    delete tasking_info;
}

// suggest to not inline this
void mpi_add_task(task_info *new_task_info)
{
#ifdef TASK_PROFILING
    tasking_info->profiling_info->tasks_created++;
#endif
    Debug(printf("add Task %d\n", new_task_info->task_callback_func_id);)
        MPI_Mutex_lock(tasking_info->mutex);
    tasking_info->waiting_task_list->enqueue(new_task_info);
    MPI_Mutex_unlock(tasking_info->mutex);

    // task_info was copied into the list therefore we may free it
    free(new_task_info);

    // if needed for other task_scheduling:
    // enter callback function on new_task event here
}

// ensures that all information needed for the task are present at the calling rank
void setup_task_for_calling(task_info *task)
{
    if (task->num_shared_vars != 0)
    { // no need to alloc a 0 element buffer
        task->LOCAL_shared_param_list = (int *)malloc(task->num_shared_vars * sizeof(int));
        assert(task->LOCAL_shared_param_list != nullptr);
        // fetch content
        mpi_load_shared_struct(task->LOCAL_shared_param_list, &task->shared_param_list,
                               &tasking_info->parameter_win);
        // use it as counter to acces the shared vars
        task->num_shared_vars = 0;
    }

    // fetching of private params will be done by the task itself
}

// collective call: cleans the tasking-data
// only call this if no tasks are present
// suggest to inline this
void clean_local_tasking_data()
{
    // assert(tasking_info->waiting_task_list->is_empty());
    // assertion may fail if someone enteres a new task directly when all tasks calling this
    // (race condition) as It only cleans unused local data, this is not harmful
    tasking_info->waiting_task_list->clean();

    if (tasking_info->local_memloc_buffer_size > 0)
    {
        for (int i = 0; i < tasking_info->local_memloc_buffer_num_entry; ++i)
        {
            MPI_Win_detach(tasking_info->parameter_win, tasking_info->local_memloc_buffer[i]);
            free(tasking_info->local_memloc_buffer[i]);
        }
        tasking_info->local_memloc_buffer_num_entry = 0;
    }
}

// retruns true if this process is allowed to leave
bool sync_point(bool is_taskwait)
{
    bool may_leave = false;

    char send = is_taskwait;
    char recv = 0;

    // implicit barrier:
    MPI_Allreduce(&send, &recv, 1, MPI_CHAR, MPI_LOR, MPI_COMM_WORLD);
    if (recv && is_taskwait)
    {
        may_leave = true;
    }
    if (!recv)
    { // no taskwait: all may leave
        may_leave = true;
    }

    // actual Sync

    // call all the sync_point_callback_func if present
    for (int i = 0; i < tasking_info->num_shared_vars; ++i)
    {
        if (tasking_info->callback_funcs[i] != nullptr)
        {
            tasking_info->callback_funcs[i](tasking_info->shared_vars[i]);
        }
    }

    clean_local_tasking_data(); // release now unused memory from all tasks before this
                                // taskwait/barrier
    // TODO evaluate if needed:
    // needed,as all sync callback functions may assume that a barrier is following (or other
    // sync functions ofc)
    MPI_Barrier(MPI_COMM_WORLD);
    return may_leave;
}

// will try to dequeue a task and start working on it
// retruns true if a task was dequeued false otherwise
// used for simple scheduling
bool simple_dequeue_task()
{
#ifdef TASK_PROFILING
    struct timeval start_time;
    struct timeval stop_time;
    double time_spend;
#endif

    task_info next_task_to_call;

    MPI_Mutex_lock(tasking_info->mutex);
    if (!tasking_info->waiting_task_list->is_empty())
    {
        tasking_info->waiting_task_list->dequeue(&next_task_to_call);
        MPI_Mutex_unlock(tasking_info->mutex);

        Debug(printf("Rank %d start working on Task %d\n", my_rank,
                     next_task_to_call.task_callback_func_id);)

#ifdef TASK_PROFILING
            gettimeofday(&start_time, NULL);
#endif
        int next_task_num_shared_vars = next_task_to_call.num_shared_vars;
        // where use a local buffer here to ensure it does not get overwritten
        // may be possible if task has shared values but does not use then
        setup_task_for_calling(&next_task_to_call);
        // actually call the task
        // this function have to be inserted by the pass in IR
        call_this_task(&next_task_to_call);
        // free local task data
        if (next_task_num_shared_vars != 0)
        {
            free(next_task_to_call.LOCAL_shared_param_list);
        }
#ifdef TASK_PROFILING
        gettimeofday(&stop_time, NULL);
        time_spend = (stop_time.tv_sec - start_time.tv_sec) +
                     (stop_time.tv_usec - start_time.tv_usec) * 1e-6;
        tasking_info->profiling_info->time_spend_working += time_spend;
        tasking_info->profiling_info->tasks_done++;
#endif
        return true;
    }
    else
    { // other process have dequeued the last task before us, so nothing more to do
        MPI_Mutex_unlock(tasking_info->mutex);
        return false;
    }
}

// this type of thas scheduling is compliant to the OpenMP standard
// may not result in best performance but minimizes the need for comm (and race conditions)
void simple_task_scheduling(bool is_taskwait)
{
#ifdef TASK_PROFILING
    struct timeval start_time;
    struct timeval stop_time;
    double time_spend;
#endif

    bool may_leave = false;
    while (!may_leave)
    {
#ifdef TASK_PROFILING
        gettimeofday(&start_time, NULL);
#endif
        // wait until all processes are ready working on task
        MPI_Barrier(MPI_COMM_WORLD);
#ifdef TASK_PROFILING
        gettimeofday(&stop_time, NULL);
        time_spend = (stop_time.tv_sec - start_time.tv_sec) +
                     (stop_time.tv_usec - start_time.tv_usec) * 1e-6;
        tasking_info->profiling_info->time_spend_waiting += time_spend;
#endif

        bool dequeued = true;
        while (dequeued) // while there still may be tasks around
        {
            dequeued = simple_dequeue_task();
        }

#ifdef TASK_PROFILING
        gettimeofday(&start_time, NULL);

        // wait until all tasks are finished
        MPI_Barrier(MPI_COMM_WORLD); // if profiling is disabled, use the barrier in
                                     // task sync point

        gettimeofday(&stop_time, NULL);
        time_spend = (stop_time.tv_sec - start_time.tv_sec) +
                     (stop_time.tv_usec - start_time.tv_usec) * 1e-6;
        tasking_info->profiling_info->time_spend_waiting += time_spend;
#endif

        may_leave = sync_point(is_taskwait);
    }
}

// each Process will poll for new tasks
void polling_task_scheduling(bool is_taskwait)
{
#ifdef TASK_PROFILING
    struct timeval start_time;
    struct timeval stop_time;
    double time_spend;
#endif

    int sleep_time =
        my_rank; // so that it will be different for every Process, this should
                 // further lower conflicting communication when access the task queue

    MPI_Request barrier_req;
    bool may_leave = false;
    while (!may_leave)
    {
        // wait until all processes are ready working on task
        MPI_Ibarrier(MPI_COMM_WORLD, &barrier_req);

#ifdef TASK_PROFILING
        gettimeofday(&start_time, NULL); // start waiting
#endif

        bool polling_loop = true;
        bool all_have_arrived = false;
        while (polling_loop) // while there still may be tasks around
        {
            task_info next_task_to_call;

            MPI_Mutex_lock(tasking_info->mutex);
            if (!tasking_info->waiting_task_list->is_empty())
            {
                tasking_info->waiting_task_list->dequeue(&next_task_to_call);
                MPI_Mutex_unlock(tasking_info->mutex);

                Debug(printf("Rank %d start working on Task %d\n", my_rank,
                             next_task_to_call.task_callback_func_id);)

#ifdef TASK_PROFILING
                    // stop waiting timer
                    gettimeofday(&stop_time, NULL);
                time_spend = (stop_time.tv_sec - start_time.tv_sec) +
                             (stop_time.tv_usec - start_time.tv_usec) * 1e-6;
                tasking_info->profiling_info->time_spend_waiting += time_spend;
                // start working timer
                gettimeofday(&start_time, NULL);
#endif
                int next_task_num_shared_vars = next_task_to_call.num_shared_vars;
                // where use a local buffer here to ensure it does not get overwritten
                // may be possible if task has shared values but does not use then
                setup_task_for_calling(&next_task_to_call);
                // actually call the task
                // this function have to be inserted by the pass in IR
                call_this_task(&next_task_to_call);
                // free local task data
                if (next_task_num_shared_vars != 0)
                {
                    free(next_task_to_call.LOCAL_shared_param_list);
                }
#ifdef TASK_PROFILING
                // stop working
                gettimeofday(&stop_time, NULL);
                time_spend = (stop_time.tv_sec - start_time.tv_sec) +
                             (stop_time.tv_usec - start_time.tv_usec) * 1e-6;
                tasking_info->profiling_info->time_spend_working += time_spend;
                tasking_info->profiling_info->tasks_done++;
                // start waiting
                gettimeofday(&start_time, NULL);
#endif
#ifdef SLEEP_BETWEEN_POLLING
                sleep_time = my_rank / 2; // reset sleep time as there was a task
                // here we will set lower initial sleep time (arbitrary) as we suppose that new
                // tasks will come in ore all processes will execute taskwait soon
#endif
            } // end if !task list empty
            else
            { // other process have dequeued the last task before us, so nothing more to do
                MPI_Mutex_unlock(tasking_info->mutex);
#ifdef SLEEP_BETWEEN_POLLING
                // only sleep if dequeue task was not succesful
                if (!all_have_arrived) // and if there are still processes out there that may
                                       // introduce new tasks
                {
                    sleep_time = (sleep_time < MAX_SLEEP_BETWEEN_POLLING_TIME)
                                     ? sleep_time + 1
                                     : MAX_SLEEP_BETWEEN_POLLING_TIME;
                    Debug(printf("Rank %d calls usleep(%d), as no tasks are present\n",
                                 my_rank, sleep_time);) usleep(sleep_time);
                }
#endif
                if (all_have_arrived)
                {
                    polling_loop = false;
                    // this means tasklist are empty
                }
            } // end if task list empty (else)

            if (!all_have_arrived)
            {
                int flag;
                MPI_Test(&barrier_req, &flag, MPI_STATUS_IGNORE);
                if (flag)
                {
                    all_have_arrived = true;
                }
            }
        } // end while polling_loop

#ifdef TASK_PROFILING
        // wait until all tasks are finished
        MPI_Barrier(MPI_COMM_WORLD); // if profiling is disabled, use the barrier in
                                     // task sync point

        gettimeofday(&stop_time, NULL);
        time_spend = (stop_time.tv_sec - start_time.tv_sec) +
                     (stop_time.tv_usec - start_time.tv_usec) * 1e-6;
        tasking_info->profiling_info->time_spend_waiting += time_spend;
#endif

        may_leave = sync_point(is_taskwait);
    }
}

// "collective" call: all processes must enter this when a taskwait or barrier occur
// if process enter this he will work on tasks as long as tasks are present
// if as least one process enters it with is_taskwait flag set: only process that entered it
// with is_taskwait flag set are allowed to leave, other processes will stay ready for new
// tasks if no one enter with taskwait flag set, this is a before barrier and all processes
// will continiue once all tasks are finished
// suggest to not inline this
void mpi_sync_task(bool is_taskwait)
{
#ifdef USE_SIMPLE_SCHEDULE
    simple_task_scheduling(is_taskwait);
#endif
#ifdef USE_POLLING_SCHEDULE
    polling_task_scheduling(is_taskwait);
#endif
    // here one may write other task scheduling functions
}

// suggest to inline this
void add_to_local_mem_list(void *new_mem_loc)
{
    // add a memory region to local_memloc_buffer
    // so it will get released once all tasks where completed
    if (tasking_info->local_memloc_buffer_size == tasking_info->local_memloc_buffer_num_entry)
    {
        tasking_info->local_memloc_buffer_size += 10;
        tasking_info->local_memloc_buffer =
            (void **)realloc(tasking_info->local_memloc_buffer,
                             tasking_info->local_memloc_buffer_size * sizeof(void *));
    }
    tasking_info->local_memloc_buffer[tasking_info->local_memloc_buffer_num_entry] =
        new_mem_loc;
    tasking_info->local_memloc_buffer_num_entry++;
}

// Funktionen für die Parameterübergabe (erzeugen des taskinfo struct)
// suggest to inline this
task_info *create_new_task_info(int task_callback_func_id, int num_shared_vars)
{
    task_info *result = (task_info *)malloc(sizeof(task_info));
    assert(result != nullptr);

    result->task_callback_func_id = task_callback_func_id;
    mpi_copy_shared_struct_ptr(nullptr, &result->private_param_list);
    result->LOCAL_private_param_list_tail_ptr = nullptr;
    result->num_shared_vars = num_shared_vars;
    mpi_copy_shared_struct_ptr(nullptr, &result->shared_param_list);
    result->LOCAL_shared_param_list = nullptr;
    if (num_shared_vars > 0)
    {
        result->LOCAL_shared_param_list = (int *)malloc(num_shared_vars * sizeof(int));
        assert(result->LOCAL_shared_param_list != nullptr);
        // initialize with -1
        for (int i = 0; i < num_shared_vars; ++i)
        {
            result->LOCAL_shared_param_list[i] = -1;
        }
        // so that other tasks may read it
        MPI_Win_attach(tasking_info->parameter_win, result->LOCAL_shared_param_list,
                       num_shared_vars * sizeof(int));
        mpi_store_shared_struct_ptr(result->LOCAL_shared_param_list,
                                    num_shared_vars * sizeof(int), &result->shared_param_list);
        add_to_local_mem_list(result->LOCAL_shared_param_list);
    }

    // use it as counter to insert the shared vars
    result->num_shared_vars = 0;

    return result;
}

// add a shared Parameter to the list
// suggest to inline this
void add_shared_param(task_info *task, void *param)
{
    Debug(printf("Add shared param %d\n", task->num_shared_vars);)

        int var_idx = -1;
    for (int i = 0; i < tasking_info->num_shared_vars; ++i)
    {
        if (tasking_info->shared_vars[i] == param)
        {
            var_idx = i;
            break;
        }
    }
    // in parralel region private Variablen dürfen im moment nicht geshared sein:
    assert(var_idx != -1 &&
           "ERROR: Sharinng a variable that is private in the enclosing Parallel "
           "Region is currently not supported\n");

    task->LOCAL_shared_param_list[task->num_shared_vars] = var_idx;
    task->num_shared_vars++; // this willresult in the correct number of shared vars set once
                             // all vars are added
}

// same for private params but we need to copy the param into a seperate buffer
// suggest to inline this
void add_private_param(task_info *task, void *param, size_t size)
{
    Debug(printf("Add private param of size%lu\n", size);)

        int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // copy param to a buffer, as caller private buffer should not be used when fetching
    // parameter
    void *local_param_buffer = malloc(size);
    assert(local_param_buffer != nullptr);
    memcpy(local_param_buffer, param, size);
    MPI_Win_attach(tasking_info->parameter_win, local_param_buffer, size);
    // add buffer to local list so that it will be freed
    // as only free will be called on prt than the type of ptr does not matter
    add_to_local_mem_list(local_param_buffer);

    struct task_param_list_struct *new_param_list_entry =
        (struct task_param_list_struct *)malloc(sizeof(struct task_param_list_struct));
    assert(new_param_list_entry != nullptr);
    add_to_local_mem_list(new_param_list_entry);
    MPI_Win_attach(tasking_info->parameter_win, new_param_list_entry,
                   sizeof(struct task_param_list_struct));

    mpi_store_shared_struct_ptr(local_param_buffer, size, &new_param_list_entry->param_ptr);
    mpi_store_shared_struct_ptr(nullptr, 0, &new_param_list_entry->next_element);

    // insert it at the end of list
    if (task->LOCAL_private_param_list_tail_ptr == nullptr)
    {
        task->private_param_list.rank = my_rank;
        MPI_Get_address(new_param_list_entry, &task->private_param_list.displacement);
        task->private_param_list.size = sizeof(struct task_param_list_struct);
    }
    else
    {
        task->LOCAL_private_param_list_tail_ptr->next_element.rank = my_rank;
        MPI_Get_address(new_param_list_entry,
                        &task->LOCAL_private_param_list_tail_ptr->next_element.displacement);
        task->LOCAL_private_param_list_tail_ptr->next_element.size =
            sizeof(struct task_param_list_struct);
    }
    task->LOCAL_private_param_list_tail_ptr = new_param_list_entry;
}

// to call on thread who execute the task:
// fetch the next shared ptr to the shared param
// returns the ptr to the local comm_info_struct for this shared var
// suggest to inline this
void *fetch_next_shared_param(task_info *task)
{
    Debug(printf("Fetch shared param %d\n", task->num_shared_vars);)

        // the LOCAL_shared_param_list was fetched before in setup_task_for_calling()
        void *result =
            tasking_info->shared_vars[task->LOCAL_shared_param_list[task->num_shared_vars]];
    task->num_shared_vars++;

    return result;
}

// param content win is the win in which the parameter content can be accessed
// param_buffer has to be of same size as the param
// suggest to inline this
void fetch_next_private_param(void *param_buffer, task_info *task)
{
    struct task_param_list_struct next_param_list_entry;
    assert(task->private_param_list.rank != -1 && "Error: Empty Parmaeter list\n");
    // fetch next list elem
    mpi_load_shared_struct(&next_param_list_entry, &task->private_param_list,
                           &tasking_info->parameter_win);

    Debug(printf("Fetch private param of size%lu\n", next_param_list_entry.param_ptr.size);)
        // fetch the parameter
        mpi_load_shared_struct(param_buffer, &next_param_list_entry.param_ptr,
                               &tasking_info->parameter_win);
    // update list head ptr (for next call )
    mpi_copy_shared_struct_ptr(&next_param_list_entry.next_element, &task->private_param_list);
}

void init_all_shared_vars_for_tasking(int num_vars)
{
    // should come from serial code:
    assert(tasking_info->num_shared_vars == -1);
    tasking_info->num_shared_vars = num_vars;
    if (num_vars > 0)
    {
        tasking_info->shared_vars = (void **)malloc(num_vars * sizeof(void *));
        tasking_info->callback_funcs =
            (sync_point_callback_func *)malloc(num_vars * sizeof(sync_point_callback_func));
    }
}

void wrap_up_all_shared_vars_for_tasking()
{
    assert(tasking_info->num_shared_vars != -1); // may NOT come from serial code
    if (tasking_info->num_shared_vars != 0)
    {
        free(tasking_info->shared_vars);
        free(tasking_info->callback_funcs);
    }
    tasking_info->num_shared_vars = -1; // now proceed into serial code
}

void setup_this_shared_var_for_tasking(void *local_var_ptr,
                                       sync_point_callback_func callback_func,
                                       int number_of_var)
{
    assert(number_of_var < tasking_info->num_shared_vars);
    tasking_info->shared_vars[number_of_var] = local_var_ptr;
    tasking_info->callback_funcs[number_of_var] = callback_func;
}
