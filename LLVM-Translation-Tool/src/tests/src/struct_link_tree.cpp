#include <limits.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "tests.h"

typedef struct tree_elem_struct
{
    int value;
    struct tree_elem_struct *left_child;
    struct tree_elem_struct *right_child;
} tree_elem;

typedef struct
{
    int num_elem;
    tree_elem *root;
} tree_head;

void insert_rec(int value, tree_elem *current_node)
{

    if (value < current_node->value)
    {
        // go left
        if (current_node->left_child == nullptr)
        {
            tree_elem *new_elem = (tree_elem *)malloc(sizeof(tree_elem));
            new_elem->value = value;
            new_elem->left_child = nullptr;
            new_elem->right_child = nullptr;
            current_node->left_child = new_elem;
        }
        else
        {
            insert_rec(value, current_node->left_child);
        }
    }
    else
    {
        // go right
        if (current_node->right_child == nullptr)
        {
            tree_elem *new_elem = (tree_elem *)malloc(sizeof(tree_elem));
            new_elem->value = value;
            new_elem->left_child = nullptr;
            new_elem->right_child = nullptr;
            current_node->right_child = new_elem;
        }
        else
        {
            insert_rec(value, current_node->right_child);
        }
    }
}

void insert(int value, tree_head *head)
{

    head->num_elem++;
    if (head->num_elem == 1)
    {
        tree_elem *new_elem = (tree_elem *)malloc(sizeof(tree_elem));
        new_elem->value = value;
        new_elem->left_child = nullptr;
        new_elem->right_child = nullptr;
        head->root = new_elem;
    }
    else
    {
        insert_rec(value, head->root);
    }
}

void remove_this(tree_elem *to_remove, tree_elem *parent)
{
    if (to_remove->left_child == nullptr && to_remove->right_child == nullptr)
    {
        // printf("Remove leaf\n");
        if (to_remove->value < parent->value)
        {
            parent->left_child = nullptr;
        }
        else
        {
            parent->right_child = nullptr;
        }
    }
    else if (to_remove->left_child == nullptr || to_remove->right_child == nullptr)
    {
        // printf("Remove node with only one child\n");
        // copy components so that i do not have to deal with if to_remove == root
        if (to_remove->left_child == nullptr)
        {
            to_remove->value = to_remove->right_child->value;
            to_remove->left_child = to_remove->right_child->left_child;

            to_remove->right_child = to_remove->right_child->right_child;
        }
        else
        {
            to_remove->value = to_remove->left_child->value;
            to_remove->right_child = to_remove->left_child->right_child;

            to_remove->left_child = to_remove->left_child->left_child;
        }
    }

    else
    {
        // printf("Remove node with 2 childs\n");
        // search for lowest value in right tree:
        tree_elem *replacement_val = to_remove->right_child;
        tree_elem *replacement_parent = to_remove;
        while (replacement_val->left_child != nullptr)
        {
            replacement_parent = replacement_val;
            replacement_val = replacement_val->left_child;
        }

        to_remove->value = replacement_val->value;
        // replacement val does not have left child
        // as it is the leftmost val in the right subtree
        if (replacement_parent == to_remove)
        {
            replacement_parent->right_child = replacement_val->right_child;
        }
        else
        {
            replacement_parent->left_child = replacement_val->right_child;
        }
    }
}

// find to remove
void remove_rec(int value, tree_elem *current_node, tree_elem *parent)
{
    // else nothing to do
    if (current_node != nullptr)
    {
        if (value == current_node->value)
        {
            remove_this(current_node, parent);
        }
        else
        {
            if (value < current_node->value)
            {
                remove_rec(value, current_node->left_child, current_node);
            }
            else
            {
                remove_rec(value, current_node->right_child, current_node);
            }
        }
    }
}

void remove(int value, tree_head *head)
{
    head->num_elem--;
    if (head->num_elem == 0)
    {
        head->root = nullptr;
    }
    else
    {
        // parent is not needed as the root will always have at least one child
        remove_rec(value, head->root, nullptr);
    }
}

// returns the number of tree elems found
// or -1 if tree integrety was violated
int validate_rec(tree_elem *current_node, int min_allowed, int max_allowed)
{
    // validate that value is within the given range
    if (!(current_node->value >= min_allowed && current_node->value <= max_allowed))
    {
        return -1;
    }
    // sum of discovered tree nodes;
    int sum = 1;
    if (current_node->left_child != nullptr)
    {
        int res = validate_rec(current_node->left_child, min_allowed, current_node->value);
        // left : all elems have to be smaller
        if (res == -1)
        {
            return -1;
        }
        sum += res;
    }
    if (current_node->right_child != nullptr)
    {
        int res = validate_rec(current_node->right_child, current_node->value, max_allowed);
        // right : all elems have to be greater
        if (res == -1)
        {
            return -1;
        }
        sum += res;
    }
    return sum;
}

bool validate_tree(tree_head *head)
{

    if (head->num_elem == 0 && head->root == nullptr)
    {
        return true;
    }
    else if (head->root != nullptr)
    {
        int encountered_elems = validate_rec(head->root, INT_MIN, INT_MAX);
        // printf("head: %d encountered %d\n", head->num_elem, encountered_elems);
        return encountered_elems == head->num_elem;
    }
    return false;
}

#define NUM_ELEMS 1
#define RANDOM_LIMIT 1000

int main()
{
    // init
    tree_head *tree = (tree_head *)malloc(sizeof(tree_head));
    tree->num_elem = 0;
    tree->root = nullptr;

    srand(time(NULL));

// Pointer to tree head is private but the tree itself is shared
#pragma omp parallel firstprivate(tree) default(none)
    {
        int thread = omp_get_thread_num();
        int ntasks = omp_get_num_threads();

        // NO WORKSHARING for loop!
        // all threads will execute all iterations
        // this means each thread inserts NUM_ELEMS many random elements to the tree
        // that no race conditins occur teh insertion are one thread after the other
        for (int i = 0; i < ntasks; ++i)
        {
            if (thread == i)
            {
                for (int j = 0; j < NUM_ELEMS; ++j)
                {
                    int new_elem = rand() % RANDOM_LIMIT;
                    insert(new_elem, tree);
                }
            }
#pragma omp barrier
        }

        for (int i = 0; i < ntasks; ++i)
        {
            if (thread == i)
            {

                int new_elem = RANDOM_LIMIT + i; // each thread inserts a unique value
                insert(new_elem, tree);
            }
#pragma omp barrier
        }

        // another insertion of random elements
        for (int i = 0; i < ntasks; ++i)
        {
            if (thread == i)
            {
                for (int j = 0; j < NUM_ELEMS; ++j)
                {
                    int new_elem = rand() % RANDOM_LIMIT;
                    insert(new_elem, tree);
                }
            }
#pragma omp barrier
        }

#pragma omp barrier
        // expected to be numthreads*(2*NUM_ELEMS+1)
        orderedPrintf("size of tree is %d\n", tree->num_elem);
#pragma omp barrier

        // each thread removes his unique element
        for (int i = 0; i < ntasks; ++i)
        {
            if (thread == i)
            {

                int remove_value = RANDOM_LIMIT + i;
                remove(remove_value, tree);
            }
#pragma omp barrier
        }

#pragma omp barrier
        // expected to be numthreads*(2*NUM_ELEMS)
        orderedPrintf("size of tree is %d\n", tree->num_elem);

        // finally each process will validate the tree
        bool is_valid = validate_tree(tree);

#pragma omp barrier
        // expected: Tree is valid
        orderedPrintf("Tree is %s\n", is_valid ? "valid" : "NOT valid!");

        // if something of the handeling with the struct ptr failes it is also very likely that
        // a segfault will occur e.g. when a thread cannot locate the unique item to remove
    }
}
