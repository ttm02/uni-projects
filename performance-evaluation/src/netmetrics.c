#include "measure.h"

#define _GNU_SOURCE

#include <stdio.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define METRICS_PER_NET_DEVICE 2

struct net_metrics_args {
	gchar** device_name_list;
	gsize device_count;
	int start_id;
	int reference_count;
};

rstruct* stop_net_metrics(void* args, void* metric_args) {
	struct net_metrics_args* arg_struct = (struct net_metrics_args*) metric_args;

	long* before_vals = (long*) args;
	if (before_vals == NULL) {
		return NULL;
	}
	int PRINTstr_len;

	rstruct* result = malloc(sizeof(rstruct));
	if (result != NULL) {
		result->resultcount = arg_struct->device_count * METRICS_PER_NET_DEVICE;

		result->result_list = malloc(sizeof(double) * result->resultcount);
		result->id_list = malloc(sizeof(int) * result->resultcount);
	}
	if (result == NULL || result->result_list == NULL || result->id_list == NULL) {
		printf("Error allocating memory");
		return NULL;
	}

	long elapsed_time = (long) time(NULL) - before_vals[arg_struct->device_count * METRICS_PER_NET_DEVICE];

	for (unsigned int i = 0; i < arg_struct->device_count; ++i) {
		char* device_name = arg_struct->device_name_list[i];
		long bytes_send = 0;
		long bytes_recv = 0;

		char* filenameR;
		PRINTstr(filenameR, "/sys/class/net/%s/statistics/rx_bytes", device_name);

		char* filenameS;
		PRINTstr(filenameS, "/sys/class/net/%s/statistics/tx_bytes", device_name);

		// read bytes send
		FILE* fd = fopen(filenameS, "r");
		if (fd == 0) {
			printf("Error read %s", filenameS);
			return NULL;
		}
		free(filenameS);

		char* line = NULL;
		size_t len = 0;

		if (getline(&line, &len, fd) != -1) {
			bytes_send = atol(line);
			bytes_send = bytes_send - before_vals[i * METRICS_PER_NET_DEVICE + 0];
		}
		fclose(fd);
		// read bytes received
		fd = fopen(filenameR, "r");
		if (fd == 0) {
			printf("Error read %s", filenameR);
			return NULL;
		}
		free(filenameR);

		if (getline(&line, &len, fd) != -1) {
			bytes_recv = atol(line);
			bytes_recv = bytes_recv - before_vals[i * METRICS_PER_NET_DEVICE + 1];
		}
		fclose(fd);
		free(line);

		result->id_list[i * METRICS_PER_NET_DEVICE + 0] = arg_struct->start_id + i * METRICS_PER_NET_DEVICE + 0;
		result->result_list[i * METRICS_PER_NET_DEVICE + 0] = (double) (bytes_send) / (double) elapsed_time;

		result->id_list[i * METRICS_PER_NET_DEVICE + 1] = arg_struct->start_id + i * METRICS_PER_NET_DEVICE + 1;
		result->result_list[i * METRICS_PER_NET_DEVICE + 1] = (double) (bytes_recv) / (double) elapsed_time;
	}	// end for each device

	free(before_vals);

	return result;
}

struct stop_monitor_struct* start_net_metrics(char* program_name, void* metric_args) {

	struct net_metrics_args* arg_struct = (struct net_metrics_args*) metric_args;
	struct stop_monitor_struct* stop_call = malloc(sizeof(struct stop_monitor_struct));
	if (stop_call == NULL) {
		return NULL;
	}
	int PRINTstr_len;

	stop_call->stop_call = &stop_net_metrics;
	stop_call->args = NULL;

	long* read_vals = malloc(sizeof(long) * (arg_struct->device_count * METRICS_PER_NET_DEVICE + 1));
//buffer for all read values
	// last value = current time
	read_vals[arg_struct->device_count * METRICS_PER_NET_DEVICE] = (long) time(NULL);

	for (unsigned int i = 0; i < arg_struct->device_count; ++i) {
		char* device_name = arg_struct->device_name_list[i];

		char* filenameR;
		PRINTstr(filenameR, "/sys/class/net/%s/statistics/rx_bytes", device_name);

		char* filenameS;
		PRINTstr(filenameS, "/sys/class/net/%s/statistics/tx_bytes", device_name);

		// read bytes send
		FILE* fd = fopen(filenameS, "r");
		if (fd == 0) {
			printf("Error read %s", filenameS);
			return stop_call;
		}
		free(filenameS);

		char* line = NULL;
		size_t len = 0;

		if (getline(&line, &len, fd) != -1) {
			read_vals[(i * METRICS_PER_NET_DEVICE) + 0] = atol(line);
		}
		fclose(fd);
		// read bytes received
		fd = fopen(filenameR, "r");
		if (fd == 0) {
			printf("Error read %s", filenameR);
			return stop_call;
		}
		free(filenameR);

		if (getline(&line, &len, fd) != -1) {
			read_vals[(i * METRICS_PER_NET_DEVICE) + 1] = atol(line);
		}
		fclose(fd);
		free(line);
	}	// end for each device

	stop_call->args = (void*) read_vals;
	return stop_call;
}

double maximum_speed(char* device_name) {
	int PRINTstr_len;
	double result = -1;
	char* filenameMAX = NULL;
	PRINTstr(filenameMAX, "/sys/class/net/%s/speed", device_name);

	FILE* fd = fopen(filenameMAX, "r");
	if (fd == 0) {
		printf("Error read %s", filenameMAX);
		return result;
	}
	free(filenameMAX);

	char* line = NULL;
	size_t len = 0;

	if (getline(&line, &len, fd) != -1) {
		result = (double) atol(line);
	}
	fclose(fd);
	free(line);
	result = result * 1000000;	// zahl wird in MB angegeben nicht in B

	return result;
}

metric* getBytes_send_Metric(char* device_name, int setting, int id, void* arguments) {
	int PRINTstr_len;
	metric* m = malloc(sizeof(metric));
	if (m == NULL) {
		return 0;
	}
	m->id = id;
	m->standard = setting;
	m->call_type_flag = CALL_TYPE_MONITOR;
	m->margin = 10;
	m->positive = 1;
	PRINTstr(m->name, "Bytes send by network device %s", device_name);
	PRINTstr(m->keyword, "%sS", device_name);
	PRINTstr(m->help, "Measures the Bytes send from network device %s per second", device_name);
	m->monitor_function = &start_net_metrics;
	m->maximum = maximum_speed(device_name);
	m->profiling_function = NULL;
	m->arguments = arguments;
	m->clear_function = &clearNet_metrics;

	return m;
}

metric* getBytes_recv_Metric(char* device_name, int setting, int id, void* arguments) {
	int PRINTstr_len;
	metric* m = malloc(sizeof(metric));
	if (m == NULL) {
		return 0;
	}
	m->id = id;
	m->standard = setting;
	m->call_type_flag = CALL_TYPE_MONITOR;
	m->margin = 10;
	m->positive = 1;
	PRINTstr(m->name, "Bytes received from network device %s", device_name);
	PRINTstr(m->keyword, "%sR", device_name);
	PRINTstr(m->help, "Measures the Bytes received from network device %s per second", device_name);
	m->monitor_function = &start_net_metrics;
	m->maximum = maximum_speed(device_name);
	m->profiling_function = NULL;
	m->arguments = arguments;
	m->clear_function = &clearNet_metrics;

	return m;
}

metric** initNet_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID) {
	gsize device_count = 0;
	gchar ** device_name_list = g_key_file_get_keys(config_file, "net", &device_count, NULL);
	*result_count = device_count * METRICS_PER_NET_DEVICE;
	if (device_count == 0) {
		return NULL;
	}

	struct net_metrics_args* arg_struct = malloc(sizeof(struct net_metrics_args));
	arg_struct->device_name_list = device_name_list;
	arg_struct->device_count = device_count;
	arg_struct->start_id = start_ID;
	arg_struct->reference_count = *result_count;

	metric** metric_array = malloc(*result_count * sizeof(metric*));
	unsigned int i = 0;
	for (i = 0; i < device_count; ++i) {
		int setting = g_key_file_get_integer(config_file, "net", device_name_list[i], NULL);
		metric_array[i * METRICS_PER_NET_DEVICE + 0] = getBytes_send_Metric(device_name_list[i], setting,
				start_ID + METRICS_PER_NET_DEVICE * i + 0, (void*) arg_struct);
		metric_array[i * METRICS_PER_NET_DEVICE + 1] = getBytes_recv_Metric(device_name_list[i], setting,
				start_ID + METRICS_PER_NET_DEVICE * i + 1, (void*) arg_struct);
	}

	return metric_array;
}

void clearNet_metrics(void* metric_args) {
	struct net_metrics_args* arg_struct = (struct net_metrics_args*) metric_args;
	arg_struct->reference_count--;
	if (arg_struct->reference_count == 0) {
		g_strfreev(arg_struct->device_name_list);
		free(arg_struct);
	}
}
