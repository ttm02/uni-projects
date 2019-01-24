#ifndef MPI_LIST_H_
#define MPI_LIST_H_

#include "linked_list.h"
#include <assert.h>
#include <mpi.h>
// MPI distributed linked list implementation
// one may use the list as stack or queue

template <class LIST_ELEM_TYPE> class MPI_linked_list : public Linked_list<LIST_ELEM_TYPE>
{

    // Note for concurrent usage:
    // this concurrency issues will happen with unguarded shared lists in openmp just the same
    // way if this should avoided guard the list usage with a mutex

    // If one element is inserted at the end and another element is removed at the same time
    // and the list has only one element, the list might break (lost update)

    // if one removes an item from the list while two other processes adds an item
    // this might break the list (as it might have 2 differnt heads then)

    // if one inserts into beginning and end of an empty list at the same time it might break
    // the list

    // but doing the SAME operation concurrently will NOT break the list even if no mutex is
    // used

    // TODO implement GC as no list-members are ever deleted
    // (the rank that allocates the element must NOT be the one removing it)

    /* Linked list pointer */
    typedef struct
    {
        int rank;
        MPI_Aint disp;
    } llist_ptr_t;

    /* Linked list element */
    typedef struct
    {
        LIST_ELEM_TYPE value;
        llist_ptr_t next;
    } llist_elem_t;

    // hold all information needed for the list
  private:
    MPI_Win win;
    // which rank should store head and Tail ptr
    // so that if u use multiple lists the rank 0 does not have to hold them all and have
    // to deal with all the load himself
    int basis_rank;
    // needed to load had and tail ptr from basis rank:
    MPI_Aint head_disp;
    MPI_Aint tail_disp;

    // only relevant on rank basis_rank:
    llist_ptr_t local_head_ptr, local_tail_ptr;

    // List of locally allocated list elements.
    llist_elem_t **my_elems;
    unsigned int my_elems_size;
    unsigned int my_elems_count;

    // Allocate a new shared linked list element
    // returns displacement for pointer to this elem
    // next element ptr is set to nil
    MPI_Aint alloc_elem(LIST_ELEM_TYPE value)
    {
        MPI_Aint disp;
        llist_elem_t *elem_ptr;

        /* Allocate the new element and register it with the window */
        MPI_Alloc_mem(sizeof(llist_elem_t), MPI_INFO_NULL, &elem_ptr);
        elem_ptr->value = value;
        // nil ptr
        elem_ptr->next = {-1, (MPI_Aint)MPI_BOTTOM};
        MPI_Win_attach(win, elem_ptr, sizeof(llist_elem_t));

        /* Add the element to the list of local elements so we can free it later. */
        if (my_elems_size == my_elems_count)
        {
            my_elems_size += 100;
            my_elems =
                (llist_elem_t **)realloc(my_elems, my_elems_size * sizeof(llist_elem_t *));
        }
        my_elems[my_elems_count] = elem_ptr;
        my_elems_count++;

        MPI_Get_address(elem_ptr, &disp);
        return disp;
    }

    // helper fuction
    // if head = true it checks wether head =nil (list was empty before insert to tail)
    // then it sets head to ptr
    // if head=false same for tail
    void insert_if_empty(bool head, llist_ptr_t *new_elem_ptr)
    {
        MPI_Aint displacement;
        if (head)
        {
            displacement = head_disp;
        }
        else
        {
            displacement = tail_disp;
        }
        llist_ptr_t list_ptr;
        // fetch

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, basis_rank, 0, win);
        MPI_Get((void *)&list_ptr, sizeof(llist_ptr_t), MPI_BYTE, basis_rank, displacement,
                sizeof(llist_ptr_t), MPI_BYTE, win);
        // update
        if (list_ptr.rank == -1)
        {
            MPI_Put((void *)new_elem_ptr, sizeof(llist_ptr_t), MPI_BYTE, basis_rank,
                    displacement, sizeof(llist_ptr_t), MPI_BYTE, win);
        }
        MPI_Win_unlock(basis_rank, win);
    }

  public:
    // collective call: create the list
    MPI_linked_list(int basis_rank = 0)
    {
        // printf("Using an MPI list\n");
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        this->basis_rank = basis_rank;
        MPI_Win_create_dynamic(MPI_INFO_NULL, MPI_COMM_WORLD, &win);

        if (rank == basis_rank)
        {
            // init head and tail with nil
            // just to be shure: i do not trust that head and tail are stored contiguous
            local_head_ptr = {-1, (MPI_Aint)MPI_BOTTOM};
            MPI_Win_attach(win, &local_head_ptr, sizeof(llist_ptr_t));
            MPI_Get_address(&local_head_ptr, &head_disp);

            local_tail_ptr = {-1, (MPI_Aint)MPI_BOTTOM};
            MPI_Win_attach(win, &local_tail_ptr, sizeof(llist_ptr_t));
            MPI_Get_address(&local_tail_ptr, &tail_disp);
        }

        /* Broadcast the location of head and tail pointer to everyone */

        MPI_Bcast(&head_disp, 1, MPI_AINT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&tail_disp, 1, MPI_AINT, 0, MPI_COMM_WORLD);

        my_elems = NULL;
        my_elems_count = 0;
        my_elems_size = 0;
    }

    // collective call: destroy the list (and free all local elements)
    ~MPI_linked_list()
    {
        MPI_Win_free(&win);

        /* Free all the elements in the list */
        for (; my_elems_count > 0; my_elems_count--)
            MPI_Free_mem(my_elems[my_elems_count - 1]);

        free(my_elems);
    }

    // singular call: insert element at the end
    void push_back(LIST_ELEM_TYPE value) override
    {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        llist_ptr_t new_elem_ptr;
        /* Create a new list element and register it with the window */
        new_elem_ptr.rank = rank;
        new_elem_ptr.disp = this->alloc_elem(value);
        llist_ptr_t old_tail_ptr;

        /* Append the new node to the list.  This might take multiple attempts if
           others have already appended and our tail pointer is stale. */

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, basis_rank, 0, win);

        // replace tail with ptr to new element and fetch old ptr
        MPI_Get_accumulate((void *)&new_elem_ptr, sizeof(llist_ptr_t), MPI_BYTE,
                           (void *)&old_tail_ptr, sizeof(llist_ptr_t), MPI_BYTE, basis_rank,
                           tail_disp, sizeof(llist_ptr_t), MPI_BYTE, MPI_REPLACE, win);

        MPI_Win_unlock(basis_rank, win);

        if (old_tail_ptr.rank == -1)
        {
            this->insert_if_empty(true, &new_elem_ptr);
        }
        else
        {
            // replace pointer of old last elem:
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, old_tail_ptr.rank, 0, win);
            MPI_Put((void *)&new_elem_ptr, sizeof(llist_ptr_t), MPI_BYTE, old_tail_ptr.rank,
                    (MPI_Aint) & (((llist_elem_t *)old_tail_ptr.disp)->next),
                    sizeof(llist_ptr_t), MPI_BYTE, win);

            //(MPI_Aint) & (((llist_elem_t *)old_tail_ptr.disp)->next)
            // gives the displacement:
            // (llist_elem_t *)old_tail_ptr.disp) is the other processes address as the
            // list_elem struct
            //->next can dereference it just as normal and & gives adress of dereferenced
            MPI_Win_unlock(old_tail_ptr.rank, win);
        }
    }

    // other name of same function if it should be used as a queue
    void enqueue(LIST_ELEM_TYPE value) override { this->push_back(value); }

    // returns value of elem or given value if list is empty
    LIST_ELEM_TYPE peek_last(LIST_ELEM_TYPE if_empty) override
    {
        // fetch current tail pointer
        llist_ptr_t tail_ptr;
        MPI_Win_lock(MPI_LOCK_SHARED, basis_rank, 0, win);
        MPI_Get((void *)&tail_ptr, sizeof(llist_ptr_t), MPI_BYTE, basis_rank, tail_disp,
                sizeof(llist_ptr_t), MPI_BYTE, win);
        MPI_Win_unlock(basis_rank, win);

        if (tail_ptr.rank == -1)
        {
            return if_empty;
        }
        // fetch element
        llist_elem_t elem;
        MPI_Win_lock(MPI_LOCK_SHARED, tail_ptr.rank, 0, win);
        MPI_Get(&elem, sizeof(llist_elem_t), MPI_BYTE, tail_ptr.rank, tail_ptr.disp,
                sizeof(llist_elem_t), MPI_BYTE, win);
        MPI_Win_unlock(tail_ptr.rank, win);

        return elem.value;
    }

    LIST_ELEM_TYPE peek(LIST_ELEM_TYPE if_empty) override
    {
        // fetch current head pointer
        llist_ptr_t head_ptr;
        MPI_Win_lock(MPI_LOCK_SHARED, basis_rank, 0, win);
        MPI_Get((void *)&head_ptr, sizeof(llist_ptr_t), MPI_BYTE, basis_rank, head_disp,
                sizeof(llist_ptr_t), MPI_BYTE, win);
        MPI_Win_unlock(basis_rank, win);

        if (head_ptr.rank == -1)
        {
            return if_empty;
        }
        // fetch element
        llist_elem_t elem;
        MPI_Win_lock(MPI_LOCK_SHARED, head_ptr.rank, 0, win);
        MPI_Get(&elem, sizeof(llist_elem_t), MPI_BYTE, head_ptr.rank, head_ptr.disp,
                sizeof(llist_elem_t), MPI_BYTE, win);
        MPI_Win_unlock(head_ptr.rank, win);

        return elem.value;
    }

    LIST_ELEM_TYPE pop(LIST_ELEM_TYPE if_empty) override
    {
        llist_ptr_t head_ptr;
        llist_ptr_t current_head_ptr;
        llist_elem_t elem;
        // fetch current head pointer
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, basis_rank, 0, win);
        MPI_Get((void *)&head_ptr, sizeof(llist_ptr_t), MPI_BYTE, basis_rank, head_disp,
                sizeof(llist_ptr_t), MPI_BYTE, win);
        MPI_Win_unlock(basis_rank, win);
        bool success = false;
        do
        {
            if (head_ptr.rank == -1)
            {
                return if_empty;
            }
            // fetch element
            MPI_Win_lock(MPI_LOCK_SHARED, head_ptr.rank, 0, win);
            MPI_Get(&elem, sizeof(llist_elem_t), MPI_BYTE, head_ptr.rank, head_ptr.disp,
                    sizeof(llist_elem_t), MPI_BYTE, win);
            MPI_Win_unlock(head_ptr.rank, win);

            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, basis_rank, 0, win);
            // replace head with element.next
            MPI_Get_accumulate((void *)&elem.next, sizeof(llist_ptr_t), MPI_BYTE,
                               (void *)&current_head_ptr, sizeof(llist_ptr_t), MPI_BYTE,
                               basis_rank, head_disp, sizeof(llist_ptr_t), MPI_BYTE,
                               MPI_REPLACE, win);
            MPI_Win_unlock(basis_rank, win);

            success = (current_head_ptr.rank == head_ptr.rank &&
                       current_head_ptr.disp == head_ptr.disp);

            // if there where some changes to head redo operation with old head
            head_ptr.disp = current_head_ptr.disp;
            head_ptr.rank = current_head_ptr.rank;

        } while (!success);
        // if list is empty now
        if (success && elem.next.rank == -1)
        {
            // also set tail to empty
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, basis_rank, 0, win);
            MPI_Put((void *)&elem.next, sizeof(llist_ptr_t), MPI_BYTE, basis_rank, tail_disp,
                    sizeof(llist_ptr_t), MPI_BYTE, win);
            MPI_Win_unlock(basis_rank, win);
        }

        return elem.value;
    }

    // other name of same function if it should be used as a queue
    LIST_ELEM_TYPE dequeue(LIST_ELEM_TYPE if_empty) override { return this->pop(if_empty); }

    // singular call: insert element at the beginning
    void push(LIST_ELEM_TYPE value) override
    {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        llist_ptr_t new_elem_ptr;
        /* Create a new list element and register it with the window */
        new_elem_ptr.rank = rank;
        new_elem_ptr.disp = this->alloc_elem(value);
        llist_ptr_t old_head_ptr;

        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, basis_rank, 0, win);

        // replace tail with ptr to new element and fetch old ptr
        MPI_Get_accumulate((void *)&new_elem_ptr, sizeof(llist_ptr_t), MPI_BYTE,
                           (void *)&old_head_ptr, sizeof(llist_ptr_t), MPI_BYTE, basis_rank,
                           head_disp, sizeof(llist_ptr_t), MPI_BYTE, MPI_REPLACE, win);

        MPI_Win_unlock(basis_rank, win);

        // set pointer to old first elem:
        my_elems[my_elems_count - 1]->next.rank = old_head_ptr.rank;
        my_elems[my_elems_count - 1]->next.disp = old_head_ptr.disp;

        if (old_head_ptr.rank == -1)
        {
            this->insert_if_empty(false, &new_elem_ptr);
        }
    }

    bool is_empty() override
    {
        // fetch current head pointer
        llist_ptr_t head_ptr;
        MPI_Win_lock(MPI_LOCK_SHARED, basis_rank, 0, win);
        MPI_Get((void *)&head_ptr, sizeof(llist_ptr_t), MPI_BYTE, basis_rank, head_disp,
                sizeof(llist_ptr_t), MPI_BYTE, win);
        MPI_Win_unlock(basis_rank, win);

        return head_ptr.rank == -1;
    }

    // clean the list = free mem
    // only call this if there where 0 list elements
    void clean()
    {
        assert(is_empty());
        /* Free all the local elements in the list */
        for (; my_elems_count > 0; my_elems_count--)
        {
            MPI_Win_detach(win, my_elems[my_elems_count - 1]);
            MPI_Free_mem(my_elems[my_elems_count - 1]);
        }
    }
};

#endif /* MPI_LIST_H_ */
