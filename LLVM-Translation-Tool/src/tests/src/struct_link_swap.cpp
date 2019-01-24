#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

typedef struct tree_elem_struct
{
    int value;
    struct tree_elem_struct *left_child;
    struct tree_elem_struct *right_child;
} tree_elem;

int main()
{
    // init
    tree_elem *left = (tree_elem *)malloc(sizeof(tree_elem));
    tree_elem *right = (tree_elem *)malloc(sizeof(tree_elem));
    tree_elem *root = (tree_elem *)malloc(sizeof(tree_elem));
    left->value = 0;
    left->left_child = nullptr;
    left->right_child = nullptr;
    right->value = 0;
    right->left_child = nullptr;
    right->right_child = nullptr;
    root->value = 0;
    root->right_child = right;
    root->left_child = left;

#pragma omp parallel firstprivate(root) default(none)
    {
        // tree nodes themselfs are not defined here
        int thread = omp_get_thread_num();
        if (thread == 1)
        {
            root->left_child->value = 10;
        }
#pragma omp barrier
        if (thread == 2)
        {
            // swap left and right
            tree_elem *tmp = root->left_child;
            root->left_child = root->right_child;
            root->right_child = tmp;
        }
#pragma omp barrier
        if (thread == 3)
        {
            root->left_child->value += 100;
        }
#pragma omp barrier
        // expected: left 100 right 10
        orderedPrintf("left %d right %d \n", root->left_child->value,
                      root->right_child->value);
    }
}
