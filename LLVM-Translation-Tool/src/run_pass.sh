#!/bin/bash
mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION -O2   -g0 -fopenmp -c rtlib.cpp
mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION -O2 -g0  -fopenmp -Xclang -load -Xclang build/omp2mpi/libomp2mpiPass.so -c shared_openmp.cpp 
mpicxx -cxx=clang++ -D ENABLE_MPI_LIST_IMPLEMENTATION -fopenmp shared_openmp.o rtlib.o 
