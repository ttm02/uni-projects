#!/bin/bash
mpicxx -cxx=clang++ -fopenmp -Xclang -load -Xclang build/experimentpass/libexperimentpass.so  shared_openmp.cpp
