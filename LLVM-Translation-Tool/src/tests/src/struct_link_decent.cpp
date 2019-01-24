#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "tests.h"

typedef struct tree_elem_struct
{
    int value;
    struct tree_elem_struct *left_child;
    struct tree_elem_struct *right_child;
} tree_elem;

#define NUM_ELEMS 2

int main()
{
    // init
    tree_elem *root = (tree_elem *)malloc(sizeof(tree_elem));
    root->value = 0;
    root->left_child = nullptr;
    root->right_child = nullptr;

// Pointer to tree head is private but the tree itself is shared
#pragma omp parallel firstprivate(root) default(none)
    {
        int thread = omp_get_thread_num();
        int ntasks = omp_get_num_threads();

        // NO WORKSHARING for loop!
        // all threads will execute all iterations
        // this means each thread inserts NUM_ELEMS many random elements to the tree
        // that no race conditins occur the insertion are one thread after the other
        for (int i = 0; i < ntasks; ++i)
        {
            if (thread == i)
            {
                for (int j = 0; j < NUM_ELEMS; ++j)
                {
                    tree_elem *curr_pos = root;
                    printf("Rank %d inserts elem %d\n", thread, (i * NUM_ELEMS) + j + 1);
                    tree_elem *new_elem = (tree_elem *)malloc(sizeof(tree_elem));
                    new_elem->value = i * NUM_ELEMS + j + 1; // the number of elem in tree
                    new_elem->left_child = nullptr;
                    new_elem->right_child = nullptr;

                    // decent left until reaced nullptr
                    while (curr_pos->left_child != nullptr)
                    {
                        curr_pos = curr_pos->left_child;
                    }
                    curr_pos->left_child = new_elem;
                }
            }
#pragma omp barrier
        }

#pragma omp barrier
        // expected NUM_ELEMS * numthreads elements in the tree
        // count all tree elems:
        int i = 0;
        tree_elem *curr_pos = root;

        while (curr_pos->left_child != nullptr)
        {
            curr_pos = curr_pos->left_child;
            i++;
        }
        orderedPrintf("size of tree is %d val of last elem: %d\n", i, curr_pos->value);
    }
}
