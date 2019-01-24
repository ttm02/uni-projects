#!/bin/bash
mpicxx -cxx=clang++ -O2   -g0 -fopenmp -c rtlib.cpp
mpicc -cc=clang -O2 -g0  -fopenmp -Xclang -load -Xclang build/omp2mpi/libomp2mpiPass.so -c shared_openmp.c
mpicxx -cxx=clang++  -fopenmp shared_openmp.o rtlib.o 
