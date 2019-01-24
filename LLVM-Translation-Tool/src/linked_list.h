#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

// Basis-Klasse für Linked Lists
// this is replaced by MPI or openmp version

// Note for concurrent usage:
// if this should avoided guard the list usage with a mutex
// I think there can be even more concurrency issues than in the MPI version

template <class LIST_ELEM_TYPE> class Linked_list
{
  public:
    // destroy the list
    virtual ~Linked_list() {}

    // insert element at the end
    virtual void push_back(LIST_ELEM_TYPE value) = 0;

    // returns value of last elem or given value if list is empty
    virtual LIST_ELEM_TYPE peek_last(LIST_ELEM_TYPE if_empty) = 0;

    // returns value of first elem or given value if list is empty
    virtual LIST_ELEM_TYPE peek(LIST_ELEM_TYPE if_empty) = 0;

    // for Stack usage:
    virtual void push(LIST_ELEM_TYPE value) = 0;

    virtual LIST_ELEM_TYPE pop(LIST_ELEM_TYPE if_empty) = 0;

    // for Queue usage;
    virtual void enqueue(LIST_ELEM_TYPE value) = 0;

    virtual LIST_ELEM_TYPE dequeue(LIST_ELEM_TYPE if_empty) = 0;

    virtual bool is_empty();
};

// include non virtual template classes
#ifdef ENABLE_MPI_LIST_IMPLEMENTATION
#include "mpi_list.h"
#endif
#include "openmp_list.h"

// use this function to get a linked list:
template <class T> Linked_list<T> *get_new_list()
{
    bool use_mpi = false;
    if (use_mpi)
    {
#ifdef ENABLE_MPI_LIST_IMPLEMENTATION
        int mpi_base_rank = 0;
        return new MPI_linked_list<T>(mpi_base_rank);
#endif
    }
    else
    {
        return new OMP_linked_list<T>();
    }
    // should never reach this anyway
    return nullptr;
}
// this guarantees that one will get an mpi list in mpi application

#endif /* LINKED_LIST_H_ */
