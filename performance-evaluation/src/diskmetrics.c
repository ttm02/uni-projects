#include "measure.h"

#define _GNU_SOURCE

#include <stdio.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define METRICS_PER_DISK_DEVICE 3

struct disk_metrics_args {
	gchar** device_name_list;
	gsize device_count;
	int start_id;
	int reference_count;
};

rstruct* stop_disk_metrics(void* args, void* metric_args) {
	struct disk_metrics_args* arg_struct = (struct disk_metrics_args*) metric_args;

	long* before_vals = (long*) args;
	if (before_vals == NULL) {
		return NULL;
	}

	rstruct* result = malloc(sizeof(rstruct));
	if (result != NULL) {
		result->resultcount = arg_struct->device_count * METRICS_PER_DISK_DEVICE;

		result->result_list = malloc(sizeof(double) * result->resultcount);
		result->id_list = malloc(sizeof(int) * result->resultcount);
	}
	if (result == NULL || result->result_list == NULL || result->id_list == NULL) {
		printf("Error allocating memory");
		return NULL;
	}

	long elapsed_time = (long) time(NULL) - before_vals[3];

	FILE* fd = fopen("/proc/diskstats", "r");
	if (fd == 0) {
		printf("Error read /proc/diskstats\n");
		return result;
	}
	char* line = NULL;
	size_t len = 0;
	int major_number, minor_mumber;
	char* device_name = malloc(sizeof(char) * MAX_DEVICE_NAME_LENGTH);
	long num_reads, reads_merged, sectors_read, reading_time, num_writes, writes_merged, sectors_written, writing_time,
			current_IO, completeIO_time, weighted_IO_time;
	while (getline(&line, &len, fd) != -1) {
		// read all lines and break when right one was found
		sscanf(line, "%d %d %" MAX_DEVICE_NAME_LENGTH_S "s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", &major_number,
				&minor_mumber, device_name, &num_reads, &reads_merged, &sectors_read, &reading_time, &num_writes,
				&writes_merged, &sectors_written, &writing_time, &current_IO, &completeIO_time, &weighted_IO_time);

		for (unsigned int i = 0; i < arg_struct->device_count; ++i) {

			if (strcmp(device_name, arg_struct->device_name_list[i]) == 0) {
				sectors_read = sectors_read - before_vals[(i * METRICS_PER_DISK_DEVICE) + 0];
				sectors_written = sectors_written - before_vals[(i * METRICS_PER_DISK_DEVICE) + 1];
				completeIO_time = completeIO_time - before_vals[(i * METRICS_PER_DISK_DEVICE) + 2];
				int Bytes_per_sector = 1;
				int PRINTstr_len;
				char* buffer;
				PRINTstr(buffer, "/sys/block/%s/queue/hw_sector_size", device_name);
				fd = fopen(buffer, "r");
				free(buffer);
				if (fd == 0) {
					printf("Error read device %s block size\n", device_name);
					return result;
				}
				if (getline(&line, &len, fd) != -1) {
					sscanf(line, "%d", &Bytes_per_sector);
				}

				result->id_list[0] = arg_struct->start_id + (i * METRICS_PER_DISK_DEVICE) + 0;
				result->result_list[0] = (double) (Bytes_per_sector * sectors_read) / (double) elapsed_time;

				result->id_list[1] = arg_struct->start_id + (i * METRICS_PER_DISK_DEVICE) + 1;
				result->result_list[1] = (double) (Bytes_per_sector * sectors_written) / (double) elapsed_time;

				result->id_list[2] = arg_struct->start_id + (i * METRICS_PER_DISK_DEVICE) + 2;
				result->result_list[2] = (double) completeIO_time / (double) elapsed_time;
				break;
			}
		}
	}
	free(device_name);
	fclose(fd);
	free(before_vals);

	free(line);

	return result;
}

struct stop_monitor_struct* start_disk_metrics(char* program_name, void* metric_args) {

	struct disk_metrics_args* arg_struct = (struct disk_metrics_args*) metric_args;

	struct stop_monitor_struct* stop_call = malloc(sizeof(struct stop_monitor_struct));
	if (stop_call == NULL) {
		return NULL;
	}

	stop_call->stop_call = &stop_disk_metrics;
	stop_call->args = NULL;

	long* read_vals = malloc(sizeof(long) * (METRICS_PER_DISK_DEVICE * arg_struct->device_count + 1));
//buffer for all read values
	// last value = current time
	read_vals[METRICS_PER_DISK_DEVICE * arg_struct->device_count] = (long) time(NULL);

	FILE* fd = fopen("/proc/diskstats", "r");
	if (fd == 0) {
		printf("Error read /proc/diskstats\n");
		return stop_call;
	}
	char* line = NULL;
	size_t len = 0;

	int major_number, minor_mumber;
	char* device_name = malloc(sizeof(char) * MAX_DEVICE_NAME_LENGTH);
	long num_reads, reads_merged, sectors_read, reading_time, num_writes, writes_merged, sectors_written, writing_time,
			current_IO, completeIO_time, weighted_IO_time;
	while (getline(&line, &len, fd) != -1) {
		// read all lines and break when right one was found
		sscanf(line, "%d %d %" MAX_DEVICE_NAME_LENGTH_S "s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", &major_number,
				&minor_mumber, device_name, &num_reads, &reads_merged, &sectors_read, &reading_time, &num_writes,
				&writes_merged, &sectors_written, &writing_time, &current_IO, &completeIO_time, &weighted_IO_time);

		for (unsigned int i = 0; i < arg_struct->device_count; ++i) {

			if (strcmp(device_name, arg_struct->device_name_list[i]) == 0) {
				read_vals[(i * arg_struct->device_count) + 0] = sectors_read;
				read_vals[(i * arg_struct->device_count) + 1] = sectors_written;
				read_vals[(i * arg_struct->device_count) + 2] = completeIO_time;
				break;
			}
		}

	}

	free(line);
	free(device_name);

	fclose(fd);

	stop_call->args = (void*) read_vals;
	return stop_call;
}

double maximum_disk_PERCENTAGES(void* metric_args) {
	return 100;
}

// it will use time the disk spends serving requests as the value to detrermine if disk load is maximum
double maximum_Bytes_written(char* device_name) {
	return -1;
}

double maximum_Bytes_read(char* device_name) {
	return -1;
}

metric* getIO_TimeMetric(char* device_name, int setting, int id, void* args) {
	metric* m = malloc(sizeof(metric));
	if (m == NULL) {
		return 0;
	}
	int PRINTstr_len;
	m->id = id;
	m->standard = setting;
	m->call_type_flag = CALL_TYPE_MONITOR;
	m->margin = 10;
	m->positive = 0;
	PRINTstr(m->name, "IO Time device %s", device_name);
	PRINTstr(m->keyword, "ioT%s", device_name);
	PRINTstr(m->help, "Percentage of time device %s spends in IO", device_name);
	m->monitor_function = &start_disk_metrics;
	m->maximum = 100; //Percentage;
	m->profiling_function = NULL;
	m->arguments = args;
	m->clear_function = &clearDisk_metrics;

	return m;

}

metric* getBytes_written_Metric(char* device_name, int setting, int id, void* args) {
	metric* m = malloc(sizeof(metric));
	if (m == NULL) {
		return 0;
	}
	int PRINTstr_len;
	m->id = id;
	m->standard = setting;
	m->call_type_flag = CALL_TYPE_MONITOR;
	m->margin = 10;
	m->positive = 1;
	PRINTstr(m->name, "Bytes Written to disk %s", device_name);
	PRINTstr(m->keyword, "%sW", device_name);
	PRINTstr(m->help, "Measures the Bytes written to disk %s per second", device_name);
	m->monitor_function = &start_disk_metrics;
	m->maximum = maximum_Bytes_written(device_name);
	m->profiling_function = NULL;
	m->arguments = args;
	m->clear_function = &clearDisk_metrics;

	return m;
}

metric* getBytesRead_Metric(char* device_name, int setting, int id, void* args) {
	metric* m = malloc(sizeof(metric));
	if (m == NULL) {
		return 0;
	}
	int PRINTstr_len;
	m->id = id;
	m->standard = setting;
	m->call_type_flag = CALL_TYPE_MONITOR;
	m->margin = 10;
	m->positive=1;
	PRINTstr(m->name, "Bytes Read from disk %s", device_name);
	PRINTstr(m->keyword, "%sR", device_name);
	PRINTstr(m->help, "Measures the Bytes read from disk %s per second", device_name);
	m->monitor_function = &start_disk_metrics;
	m->maximum = maximum_Bytes_read(device_name);
	m->profiling_function = NULL;
	m->arguments = args;
	m->clear_function = &clearDisk_metrics;

	return m;
}

metric** initDisk_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID) {

	gsize device_count = 0;
	gchar ** device_name_list = g_key_file_get_keys(config_file, "disk", &device_count, NULL);
	*result_count = device_count * METRICS_PER_DISK_DEVICE;
	if (device_count == 0) {
		return NULL;
	}

	struct disk_metrics_args* arg_struct = malloc(sizeof(struct disk_metrics_args));
	arg_struct->device_count = device_count;
	arg_struct->device_name_list = device_name_list;
	arg_struct->start_id = start_ID;
	arg_struct->reference_count = *result_count;

	metric** metric_array = malloc(*result_count * sizeof(metric*));
	unsigned int i = 0;
	for (i = 0; i < device_count; ++i) {
		int setting = g_key_file_get_integer(config_file, "disk", device_name_list[i], NULL);
		metric_array[i * METRICS_PER_DISK_DEVICE + 0] = getIO_TimeMetric(device_name_list[i], setting,
				start_ID + METRICS_PER_DISK_DEVICE * i + 0, (void*) arg_struct);
		metric_array[i * METRICS_PER_DISK_DEVICE + 1] = getBytes_written_Metric(device_name_list[i], setting,
				start_ID + METRICS_PER_DISK_DEVICE * i + 1, (void*) arg_struct);
		metric_array[i * METRICS_PER_DISK_DEVICE + 2] = getBytesRead_Metric(device_name_list[i], setting,
				start_ID + METRICS_PER_DISK_DEVICE * i + 2, (void*) arg_struct);
	}
	return metric_array;

}

void clearDisk_metrics(void* metric_args) {
	struct disk_metrics_args* arg_struct = (struct disk_metrics_args*) metric_args;
	arg_struct->reference_count--;
	if (arg_struct->reference_count == 0) {
		g_strfreev(arg_struct->device_name_list);
		free(arg_struct);
	}
}
