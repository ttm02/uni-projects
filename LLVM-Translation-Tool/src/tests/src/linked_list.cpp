#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "omp.h"

#define NUM_ELEMS 32
#define NPROBE 100
#define ELEM_PER_ROW 16

//#include "linked_list.h"
#include "../../linked_list.h"

int main(int argc, char **argv)
{

    Linked_list<int> *list = get_new_list<int>();

#pragma omp parallel shared(list)
    {
        int procid, nproc, i;
        procid = omp_get_thread_num();
        nproc = omp_get_num_threads();
        /* All processes concurrently append NUM_ELEMS elements to the list */
        for (i = 0; i < NUM_ELEMS; i++)
        {
            // printf("Process %i inserts his %i nd elem\n", procid, i);
            // list->push(procid);
            list->push_back(procid);
        }

#pragma omp barrier

        /* Traverse the list and verify that all processes inserted exactly the correct
           number of elements. */
        if (procid == 0)
        {
            int errors = 0;
            int *counts, count = 0;

            counts = (int *)calloc(nproc, sizeof(int));
            assert(counts != NULL);

            int empty_list = -1;
            int elem;
            elem = list->pop(empty_list);

            while (elem != empty_list)
            {
                assert(elem >= 0 && elem < nproc);
                counts[elem]++;
                count++;
                elem = list->pop(empty_list);
            }

            /* Verify the counts we collected */
            for (i = 0; i < nproc; i++)
            {
                int expected = NUM_ELEMS;

                if (counts[i] != expected)
                {
                    printf("Error: Rank %d inserted %d elements, expected %d\n", i, counts[i],
                           expected);
                    errors++;
                }
            }

            printf("%s\n", errors == 0 ? " No Errors" : "FAIL");
            free(counts);
        }
    }
    delete list;

    return 0;
}
