#include "measure.h"

#include <sys/sysinfo.h>
#include <unistd.h>
#include <time.h>

rstruct* stop_CPU_metrics(void* args, void* metric_args) {

	int start_ID = *(int*) metric_args;
	int num_cpu = sysconf(_SC_NPROCESSORS_CONF);

	long* before_vals = (long*) args;

	if (before_vals == NULL) {
		return NULL;
	}

	rstruct* result = malloc(sizeof(rstruct));
	if (result != NULL) {
		result->resultcount = num_cpu * 4;
	}
	result->result_list = malloc(sizeof(double) * num_cpu * 4);
	result->id_list = malloc(sizeof(int) * num_cpu * 4);
	if (result->result_list == NULL || result->id_list == NULL) {
		printf("Error allocating memory");
		return NULL;
	}

	FILE* fd = fopen("/proc/stat", "r");
	if (fd == 0) {
		printf("Error read /proc/stat\n");
		return NULL;
	}
	char* line = NULL;
	size_t len = 0;
// read first line and skip it
	if (getline(&line, &len, fd) == -1) {
		printf("Error read /proc/stat\n");
		return result;
	}

	long userT, niceT, systemT, idleT, iowaitT, irqT, softirqT, sumT;

// read line for each cpu
	for (int i = 0; i < num_cpu; ++i) {
		if (getline(&line, &len, fd) != -1) {

			sscanf(line, "%*s %ld %ld %ld %ld %ld %ld %ld", &userT, &niceT, &systemT, &idleT, &iowaitT, &irqT,
					&softirqT);
			sumT = userT + niceT + systemT + idleT + iowaitT + irqT + softirqT;

			userT = userT - before_vals[i * 5];
			systemT = systemT - before_vals[i * 5 + 1];
			sumT = sumT - before_vals[i * 5 + 2];
			iowaitT = iowaitT - before_vals[i * 5 + 3];
			idleT = idleT - before_vals[i * 5 + 4];

			if (sumT == 0) {
				printf("0 Time passed?????\n");
			}

			result->result_list[i * 4] = ((double) userT / (double) sumT)*100;
			result->id_list[i * 4] = start_ID + i * 4;
			result->result_list[i * 4 + 1] = ((double) systemT / (double) sumT)*100;
			result->id_list[i * 4 + 1] = start_ID + i * 4 + 1;

			result->result_list[i * 4 + 2] = ((double) iowaitT / (double) sumT)*100;
			result->id_list[i * 4 + 2] = start_ID + i * 4 + 2;
			result->result_list[i * 4 + 3] = ((double) idleT / (double) sumT)*100;
			result->id_list[i * 4 + 3] = start_ID + i * 4 + 3;

		}

	}
	free(line);

	fclose(fd);

	free(before_vals);

	return result;
}

struct stop_monitor_struct* start_CPU_metrics(char* program_name, void* metric_args) {

	struct stop_monitor_struct* stop_call = malloc(sizeof(struct stop_monitor_struct));
	if (stop_call == NULL) {
		return NULL;
	}

	stop_call->stop_call = &stop_CPU_metrics;
	stop_call->args = NULL;
	int num_cpu = sysconf(_SC_NPROCESSORS_CONF);

	long* read_vals = malloc(sizeof(long) * (num_cpu * 5));
//buffer for all read values

	FILE* fd = fopen("/proc/stat", "r");
	if (fd == 0) {
		printf("Error read /proc/stat\n");
		return stop_call;
	}
	char* line = NULL;
	size_t len = 0;
// read first line and skip it
	if (getline(&line, &len, fd) == -1) {
		printf("Error read /proc/stat\n");
		return stop_call;
	}
	long userT, niceT, systemT, idleT, iowaitT, irqT, softirqT, sumT;
// read line for each cpu
	for (int i = 0; i < num_cpu; ++i) {
		if (getline(&line, &len, fd) != -1) {

			sscanf(line, "%*s %ld %ld %ld %ld %ld %ld %ld", &userT, &niceT, &systemT, &idleT, &iowaitT, &irqT,
					&softirqT);
			sumT = userT + niceT + systemT + idleT + iowaitT + irqT + softirqT;

			read_vals[i * 5] = userT;
			read_vals[i * 5 + 1] = systemT;
			read_vals[i * 5 + 2] = sumT;
			read_vals[i * 5 + 3] = iowaitT;
			read_vals[i * 5 + 4] = idleT;
		}

	}
	free(line);

	fclose(fd);

	stop_call->args = (void*) read_vals;
	return stop_call;
}

// count the number of metrics for one CPU
// so there will be set up CPU_METRIC_NUMBER * num_cpu different metrics
#define CPU_METRIC_NUMBER 4

metric** initCPU_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID) {

	int num_cpu = sysconf(_SC_NPROCESSORS_CONF); /* processors configured */

	*result_count = num_cpu * CPU_METRIC_NUMBER;

	int* metric_args = malloc(2 * sizeof(int));
	metric_args[0] = start_ID;
	metric_args[1] = num_cpu * CPU_METRIC_NUMBER; // reference count

	metric** array_pointer = malloc(*result_count * sizeof(metric*));

	int i;
	int PRINTstr_len;

	for (i = 0; i < num_cpu; ++i) {
		// here: get all metrics for each CPU
		metric* m = malloc(sizeof(metric));
		if (m == NULL) {
			printf("ERROR Allocating memory");
		}
		m->id = start_ID + i * CPU_METRIC_NUMBER;
		m->standard = ON;
		//m->standard = OFF; // for debug-reasons
		m->call_type_flag = CALL_TYPE_MONITOR;
		m->margin = 10;
		m->positive = 1;
		PRINTstr(m->name, "CPU%i usertime", i);
		PRINTstr(m->keyword, "CPU%iut", i);
		PRINTstr(m->help, "measures the percentage of time  CPU%i spends in userspace", i);
		m->monitor_function = &start_CPU_metrics;
		m->maximum = 100; //Percentage
		m->profiling_function = NULL;
		m->arguments = (void*) metric_args;
		m->clear_function = &clearCPU_metrics;

		array_pointer[i * CPU_METRIC_NUMBER] = m;

		// next metric:
		m = malloc(sizeof(metric));
		if (m == 0) {
			printf("ERROR Allocating memory");
		}
		m->id = start_ID + i * CPU_METRIC_NUMBER + 1;
		m->standard = ON;
		//m->standard = OFF; // for debug-reasons
		m->call_type_flag = CALL_TYPE_MONITOR;
		m->margin = 10;
		m->positive = 0;
		PRINTstr(m->name, "CPU%i systime", i);
		PRINTstr(m->keyword, "CPU%ist", i);
		PRINTstr(m->help, "measures the percentage of time  CPU%i spends in systemspace", i);
		m->monitor_function = &start_CPU_metrics;
		m->maximum = 100; //Percentage
		m->profiling_function = NULL;
		m->arguments = (void*) metric_args;
		m->clear_function = &clearCPU_metrics;

		array_pointer[i * CPU_METRIC_NUMBER + 1] = m;

		// next metric:
		m = malloc(sizeof(metric));
		if (m == 0) {
			printf("ERROR Allocating memory");
		}
		m->id = start_ID + i * CPU_METRIC_NUMBER + 2;
		m->standard = ON;
		//m->standard = OFF; // for debug-reasons
		m->call_type_flag = CALL_TYPE_MONITOR;
		m->margin = 10;
		m->positive = 0;
		PRINTstr(m->name, "CPU%i iowait", i);
		PRINTstr(m->keyword, "CPU%iow", i);
		PRINTstr(m->help, "measures the percentage of time  CPU%i spends in iowait", i);
		m->monitor_function = &start_CPU_metrics;
		m->maximum = 100; //Percentage
		m->profiling_function = NULL;
		m->arguments = (void*) metric_args;
		m->clear_function = &clearCPU_metrics;

		array_pointer[i * CPU_METRIC_NUMBER + 2] = m;

		// next metric:
		m = malloc(sizeof(metric));
		if (m == 0) {
			printf("ERROR Allocating memory");
		}
		m->id = start_ID + i * CPU_METRIC_NUMBER + 3;
		m->standard = ON;
		//m->standard = OFF; // for debug-reasons
		m->call_type_flag = CALL_TYPE_MONITOR;
		m->margin = 10;
		m->positive = 0;
		PRINTstr(m->name, "CPU%i idle", i);
		PRINTstr(m->keyword, "CPU%idle", i);
		PRINTstr(m->help, "measures the percentage of time  CPU%i spends idling", i);
		m->monitor_function = &start_CPU_metrics;
		m->maximum = 100; //Percentage
		m->profiling_function = NULL;
		m->arguments = (void*) metric_args;
		m->clear_function = &clearCPU_metrics;

		array_pointer[i * CPU_METRIC_NUMBER + 3] = m;

	}

	return array_pointer;

}

void clearCPU_metrics(void* metric_args) {
	int* arg = (int*) metric_args;
	arg[1]--; // decrement reference count
	if (arg[1] == 0) {
		free(arg);
	}
}
