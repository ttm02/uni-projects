#include "measure.h"

#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


rstruct* measure_time(char* program,void* metric_args) {

	double time = 42; // just as a test vaslue

	system(program); // call profiling tool

	rstruct* result = malloc(sizeof(rstruct));
	if (result != 0) {
		result->result_list = malloc(sizeof(double) * 1);
		if (result->result_list != 0) {
			result->result_list[0] = time;
		}

		result->id_list = malloc(sizeof(int) * 1);
		if (result->id_list != 0) {
			result->id_list[0] = *(int*) metric_args;
		}

		result->resultcount = 1;
	}
	return result;
}

double maximum_time() {
	// just as a test value:
	return 42 * 42;
}

void clearMetric1(void* metric_args) {
	int* arg = (int*) metric_args;
	arg[1]--; // decrement reference count
	if (arg[1] == 0) {
		free(arg);
	}
}

metric** initNEW_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID) {
	*result_count = 1;
	metric** result_list = malloc(*result_count * sizeof(metric*));
	metric* m = malloc(sizeof(metric));
	if (m == NULL) {
		return NULL;
	}

	int* metric_args = malloc(2 * sizeof(int));
		metric_args[0] = start_ID;
		metric_args[1] = 1; // reference count

	int PRINTstr_len;
	m->id = start_ID;
	m->standard = OFF;
	m->call_type_flag = CALL_TYPE_PROFILE;
	m->margin = 10;
	m->positive = 0;
	PRINTstr(m->name, "testmetric%s", "");
	PRINTstr(m->keyword, "t%s", "");
	PRINTstr(m->help, "One Metric to rule them all (always give 42)%s", "");
	m->monitor_function = NULL;
	m->maximum = maximum_time();
	m->profiling_function = &measure_time;
	m->arguments=metric_args;
		m->clear_function=&clearMetric1;

	result_list[0] = m;
	return result_list;
}


