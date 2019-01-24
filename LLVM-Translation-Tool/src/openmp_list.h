#ifndef OMP_LIST_H_
#define OMP_LIST_H_

#include "linked_list.h"
#include <omp.h>
// implementation of OpenMp linked list

// Note for concurrent usage:
// if this should avoided guard the list usage with a mutex
// I think there can be even more concurrency issues than in the MPI version

template <class LIST_ELEM_TYPE> class OMP_linked_list : public Linked_list<LIST_ELEM_TYPE>
{

    typedef struct omp_list_elem_t_struct
    {
        LIST_ELEM_TYPE value;
        struct omp_list_elem_t_struct *next;
    } llist_elem_t_omp;

    // hold all information needed for the list
  private:
    llist_elem_t_omp *head;
    llist_elem_t_omp *tail;

  public:
    // create the list (only call this at one thread and share the resulting pointer)
    // returns the pointer to newly allocated List

    OMP_linked_list()
    {
        // printf("Using an OMP list\n");
        head = NULL;
        tail = NULL;
    }

    // destroy the list
    ~OMP_linked_list()
    {
        llist_elem_t_omp *elem;
        llist_elem_t_omp *next;
        elem = head;
        while (elem != NULL)
        {
            next = elem->next;
            free(elem);
            elem = next;
        }
    }

    // insert element at the end
    void push_back(LIST_ELEM_TYPE value) override
    {
        llist_elem_t_omp *new_elem = (llist_elem_t_omp *)calloc(1, sizeof(llist_elem_t_omp));
        assert(new_elem != NULL);
        new_elem->value = value;
        if (tail != NULL)
        {
            tail->next = new_elem;
        }
        tail = new_elem;
        // TODO if 2 inserts?
        if (head == NULL)
        {
            head = new_elem;
        }
    }

    // returns value of last elem or given value if list is empty
    LIST_ELEM_TYPE peek_last(LIST_ELEM_TYPE if_empty) override
    {
        if (tail == NULL)
        {
            return if_empty;
        }
        else
        {
            return tail->value;
        }
    }

    // returns value of first elem or given value if list is empty
    LIST_ELEM_TYPE peek(LIST_ELEM_TYPE if_empty) override
    {
        if (head == NULL)
        {
            return if_empty;
        }
        else
        {
            return head->value;
        }
    }

    // for Stack usage:
    void push(LIST_ELEM_TYPE value) override
    {
        llist_elem_t_omp *new_elem = (llist_elem_t_omp *)calloc(1, sizeof(llist_elem_t_omp));
        assert(new_elem != NULL);
        new_elem->value = value;
        new_elem->next = head;
        head = new_elem;
        // TODO if 2 inserts?
        if (tail == NULL)
        {
            tail = new_elem;
        }
    }

    LIST_ELEM_TYPE pop(LIST_ELEM_TYPE if_empty) override
    {
        if (head == NULL)
        {
            return if_empty;
        }
        llist_elem_t_omp *elem = head;
        head = elem->next;
        // TODO if 2 removes?

        LIST_ELEM_TYPE result = elem->value;
        free(elem);
        return result;
    }

    // for Queue usage;
    void enqueue(LIST_ELEM_TYPE value) override { this->push_back(value); }
    LIST_ELEM_TYPE dequeue(LIST_ELEM_TYPE if_empty) override { return this->pop(if_empty); }

    bool is_empty() override { return head == nullptr; }
};

#endif /* OMP_LIST_H_ */
