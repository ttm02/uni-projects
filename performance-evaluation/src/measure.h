#ifndef MEASURE_H_INCLUDED
#define MEASURE_H_INCLUDED
// Include guard

//#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

#include "constants.h"// includes used constants
#include "datatypes.h"// includes the used datatypes
#include "metric_definitions.h" // includes the functions for metric initialization

extern int GLOBAL_metric_number;

metric** init_metrics();
void free_all_Metrics(metric** metric_array);
int check_metrics(metric** metric_array);

// utility macro:
// usage: char* foo=NULL;
// PRINTstr(foo,"foo%s ","bar");
// !!!use with caution!!!:
// need int PRINTstr_len! to be defined in order to work properly!!
// PRINTstr_len will be overwritten !!
//tgt will be malloced and therefore overwritten!!
#define PRINTstr(tgt,formatstr, ...) PRINTstr_len=snprintf(NULL,0,formatstr, __VA_ARGS__);\
	PRINTstr_len++;\
	tgt=malloc(sizeof(char)*PRINTstr_len);\
	snprintf(tgt,PRINTstr_len,formatstr, __VA_ARGS__)

#endif /* MEASURE_H_INCLUDED */
