#include "measure.h"

#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define FILENAME "perf-out"

rstruct* measure_cache(char* program, void* metric_args) {

	char* command = NULL;
	int PRINTstr_len;

	PRINTstr(command, "perf stat -o %s -x' '  -e cache-references,cache-misses,branches,branch-misses %s", FILENAME,
			program);

	system(command);

	long cache_ref = 0;
	long cache_miss = 0;
	long branches = 0;
	long branche_miss = 0;

	FILE* fd = fopen(FILENAME, "r");
	if (fd == -0) {
		printf("Error open perf-output\n");
		return NULL;
	}
	//read 4 lines:
	char* line = NULL;
	size_t len = 0;
	if (getline(&line, &len, fd) != -1) {
		// skip line
		if (getline(&line, &len, fd) != -1) {
			//skip line
			if (getline(&line, &len, fd) != -1) {
				// cache references
				sscanf(line, "%ld", &cache_ref);

				if (getline(&line, &len, fd) != -1) {
					//cache misses
					sscanf(line, "%ld", &cache_miss);
					if (getline(&line, &len, fd) != -1) {
						//branches s
						sscanf(line, "%ld", &branches);
						if (getline(&line, &len, fd) != -1) {
							//branch misses
							sscanf(line, "%ld", &branche_miss);
						}
					}
				}
			}
		}
	}

	fclose(fd);
	remove(FILENAME);
	free(command);
	free(line);

	double cache_percent = 0;
	if (cache_ref != 0) {
		cache_percent = ((double) cache_miss / (double) cache_ref) * 100;
	}
	double branch_percent = 0;
		if (branches != 0) {
			branch_percent = ((double) branche_miss / (double) branches) * 100;
		}

	int start_ID = *(int*) metric_args;
	rstruct* result = malloc(sizeof(rstruct));
	if (result != 0) {
		result->result_list = malloc(sizeof(double) * 2);
		if (result->result_list != 0) {
			result->result_list[0] = cache_percent;
			result->result_list[1] = branch_percent;
		}

		result->id_list = malloc(sizeof(int) * 2);
		if (result->id_list != 0) {
			result->id_list[0] = start_ID;
			result->id_list[1] = start_ID+1;
		}

		result->resultcount = 2;
	}
	return result;
}

void clearcache_metric(void* metric_args) {
	int* arg = (int*) metric_args;
	arg[1]--; // decrement reference count
	if (arg[1] == 0) {
		free(arg);
	}
}

metric** init_cache_Metric(unsigned int* result_count, GKeyFile* config_file, int start_ID) {
	*result_count = 2;
	metric** result_list = malloc(*result_count * sizeof(metric*));
	metric* m = malloc(sizeof(metric));
	if (m == NULL) {
		return NULL;
	}

	int* metric_args = malloc(2 * sizeof(int));
	metric_args[0] = start_ID;
	metric_args[1] = 2; // reference count

	int PRINTstr_len;
	m->id = start_ID;
	m->standard = ON;
	m->call_type_flag = CALL_TYPE_PROFILE;
	m->margin = 10;
	m->positive = 0;
	PRINTstr(m->name, "cache-miss-rate%s", "");
	PRINTstr(m->keyword, "cache%s", "");
	PRINTstr(m->help, "Cache-miss rate in percent%s", "");
	m->monitor_function = NULL;
	m->maximum = 100; // percentage
	m->profiling_function = &measure_cache;
	m->arguments = (void*) metric_args;
	m->clear_function = &clearcache_metric;

	result_list[0] = m;

	m = malloc(sizeof(metric));
	if (m == NULL) {
		return NULL;
	}
	m->id = start_ID+1;
	m->standard = ON;
	m->call_type_flag = CALL_TYPE_PROFILE;
	m->margin = 10;
	m->positive = 0;
	PRINTstr(m->name, "branche-miss-rate%s", "");
	PRINTstr(m->keyword, "branche%s", "");
	PRINTstr(m->help, "Brance-miss rate in percent%s", "");
	m->monitor_function = NULL;
	m->maximum = 100; // percentage
	m->profiling_function = &measure_cache;
	m->arguments = (void*) metric_args;
	m->clear_function = &clearcache_metric;

	result_list[1] = m;
	return result_list;
}
