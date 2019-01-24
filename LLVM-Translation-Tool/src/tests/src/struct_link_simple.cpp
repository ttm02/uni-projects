#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#include "tests.h"

struct B
{
    int var_b;
};
struct A
{
    struct B *link;
};

void foo(struct A *a) { a->link->var_b++; }

int main()
{
    // init
    struct B *testB = (struct B *)malloc(sizeof(struct B));
    // struct B *testB = new struct B();
    testB->var_b = 40;

    struct A *testA = (struct A *)malloc(sizeof(struct A));
    testA->link = testB;

#pragma omp parallel firstprivate(testA) default(none)
    {
        // B itself is not defined here
        int thread = omp_get_thread_num();
        if (thread == 1)
        {
            foo(testA);
        }
        if (thread == 2)
        {
            sleep(1);
            foo(testA);
        }
#pragma omp barrier
        // expected to be 42 for all threads
        // if sharing does not work, thread 1 and 2 will have only 41 and others 40
        orderedPrintf("Var in struct B is %d\n", testA->link->var_b);
    }
}
