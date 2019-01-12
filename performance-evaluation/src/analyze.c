#include "measure.h"

#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <malloc.h>
#include <sys/time.h>
#include <string.h>
//#include <regex.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <sys/sysinfo.h>
#include <float.h>
#include <regex.h>

#include "kmeans.h"

struct program_options {
	char* output; //output-file
	char* input; // program to measure
	int* measure_metrics; // list of metrics to measure
	int metrics_count; // count the number of metrics requested to measure
	int dir; // directory-mode
	double threshold; // threshold for suppress the output of non utilized metrics
	int top_x; //only show the top x differences of samples
	int clustering_factor; // factor to divide sample_count in order to get number of clusters
	int verbose; // do some additional output
	char* hostname; // Analyze data for given hostname
};

static void report_sample(int sample, double** data, FILE* output, struct program_options* options,
		metric** metric_array);
static double check_cpu_usage(int sample, double** data, FILE* output, struct program_options* options,
		metric** metric_array);

struct list_elem {
	double value;
	struct list_elem* next;
};

// saves the start-ids of important metrics
// in order to use theese ids in the analysis
struct metric_ids {
	int number_cpu;
	int cpuMetrics;
	int number_net_devices;
	int netMetrics;
	int number_disk_devices;
	int diskMetrics;
	int cacheMetrics;
	int branceMetrics;
	// metrics not defined in version 1.0
	int number_additional_metrics;
	int additional_metrics;
};

int GLOBAL_metric_number = 0;

struct metric_ids GLOBAL_metric_ids;

/*
 * Match string against the extended regular expression in
 * pattern, treating errors as no match.
 *
 * Return 1 for match, 0 for no match.
 */
static int string_match_regex(const char *string, char *pattern) {
	int status;
	regex_t re;

	if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
		return (0); /* Report error. */
	}
	status = regexec(&re, string, (size_t) 0, NULL, 0);
	regfree(&re);
	if (status != 0) {
		return (0); /* Report error. */
	}
	return (1);
}

static void init_metric_ids(struct program_options* options, metric** metric_array) {

	int i, j;
	GLOBAL_metric_ids.cpuMetrics = -1;
	GLOBAL_metric_ids.diskMetrics = -1;
	GLOBAL_metric_ids.netMetrics = -1;
	GLOBAL_metric_ids.cacheMetrics = -1;
	GLOBAL_metric_ids.branceMetrics = -1;
	GLOBAL_metric_ids.additional_metrics = -1;

	GLOBAL_metric_ids.number_cpu = 0;
	GLOBAL_metric_ids.number_disk_devices = 0;
	GLOBAL_metric_ids.number_net_devices = 0;
	GLOBAL_metric_ids.number_additional_metrics = 0;

	i = 0;
	while (i < GLOBAL_metric_number) {
		//printf("for i= %i %s\n ", i, metric_array[i]->keyword);
		if (options->measure_metrics[i] != OFF) {
			//printf("ON\n");
			if (string_match_regex(metric_array[i]->name, " usertime")) {
				GLOBAL_metric_ids.cpuMetrics = i;
				for (j = i;
						j < GLOBAL_metric_number - 2 && string_match_regex(metric_array[j]->name, " usertime")
								&& string_match_regex(metric_array[j + 1]->name, " systime")
								&& string_match_regex(metric_array[j + 2]->name, " iowait")
								&& string_match_regex(metric_array[j + 3]->name, " idle"); j = j + 4) {
					// only count if all metrics are present for this cpu
					++GLOBAL_metric_ids.number_cpu;
				}
				i = j;

			} else if (GLOBAL_metric_ids.diskMetrics == -1
					&& string_match_regex(metric_array[i]->name, "IO Time device")) {
				GLOBAL_metric_ids.diskMetrics = i;

				for (j = i;
						j < GLOBAL_metric_number - 2 && string_match_regex(metric_array[j]->name, "IO Time device")
								&& string_match_regex(metric_array[j + 1]->name, "Bytes Written to disk")
								&& string_match_regex(metric_array[j + 2]->name, "Bytes Read from disk"); j = j + 3) {
					// only count if all metrics are present for this disk
					++GLOBAL_metric_ids.number_disk_devices;
				}
				i = j;
			} else if (GLOBAL_metric_ids.netMetrics == -1
					&& string_match_regex(metric_array[i]->name, "Bytes send by network device")) {
				GLOBAL_metric_ids.netMetrics = i;
				for (j = i;
						j < GLOBAL_metric_number - 1
								&& string_match_regex(metric_array[j]->name, "Bytes send by network device")
								&& string_match_regex(metric_array[j + 1]->name, "Bytes received from network device");
						j = j + 2) {
					++GLOBAL_metric_ids.number_net_devices;
				}
				i = j;
			} else if (strcmp(metric_array[i]->keyword, "cache") == 0) {
				GLOBAL_metric_ids.cacheMetrics = i;
				++i;
			} else if (strcmp(metric_array[i]->keyword, "branche") == 0) {
				GLOBAL_metric_ids.branceMetrics = i;
				++i;
			} else {
				++GLOBAL_metric_ids.number_additional_metrics;
				++i;
				if (GLOBAL_metric_ids.additional_metrics == -1) {
					GLOBAL_metric_ids.additional_metrics = i;
				}
			}

		} else {
			// metric was not measured
			++i;
		}

	}

	//printf("detected %i cpu %i disk %i net and %i other at %i\n", GLOBAL_metric_ids.number_cpu,
	//		GLOBAL_metric_ids.number_disk_devices, GLOBAL_metric_ids.number_net_devices,
	//		GLOBAL_metric_ids.number_additional_metrics, GLOBAL_metric_ids.additional_metrics);
}

static double** get_input_data(struct program_options* options, char* filename, int* sample_count) {

	double** data = malloc(GLOBAL_metric_number * sizeof(double*));
	if (data == NULL) {
		printf("Error allocating memory\n");
		return NULL;
	}

	int i = 0;
	FILE* fd = fopen(filename, "r");
	char* line = NULL;
	size_t len = 0;

	// see if we are reading a file with all measured values
	if (string_match_regex(filename, "_allMeasurements$") || string_match_regex(filename, "_allMeasurements.out$")) {
		*sample_count = INT_MAX;
		// get minimum sample count for all metrics where there are samples
		while (getline(&line, &len, fd) != -1 && i < GLOBAL_metric_number) {
			if (strcmp(line, "\n") == 0) {
				data[i] = NULL;
				//printf("read: no data for  metric %i\n", i);
			} else {
				int datacount = 0;
				for (int j = 0; line[j] != '\0'; ++j) {
					if (line[j] == '	') {
						datacount++;
					}
				}			// count number of tabs as before every date there is a tab
				data[i] = malloc(datacount * sizeof(double));
				*sample_count = (*sample_count < datacount) ? *sample_count : datacount;			// min
				if (data[i] == NULL) {
					printf("Error allocating memory");
					return NULL;
				}

				datacount = 0;
				for (int j = 0; line[j] != '\0'; ++j) {
					if (line[j] == DATA_SEPERATOR) {
						// read the value
						sscanf(&line[j], DATA_SEPERATOR_S"%lf", &data[i][datacount]);
						datacount++;
					}
					options->measure_metrics[i] = ON;
				}
				//printf("read: %i data for  metric %i\n", datacount, i);
			}
			i++;
		}

	} else {
		// read file containing only the mean values
		*sample_count = 1;
		if (getline(&line, &len, fd) != -1) {
			// skip first line
			while (getline(&line, &len, fd) != -1 && i < GLOBAL_metric_number) {
				if (strcmp(line, "\n") == 0) {
					data[i] = NULL;
				} else {

					data[i] = malloc(1 * sizeof(double));
					if (data[i] == NULL) {
						printf("Error allocating memory");
						return NULL;
					}
					sscanf(line, "%lf", data[i]);
					options->measure_metrics[i] = ON;
				}
				++i;
			}
		}
	}
	free(line);
	return data;
}

static double*** get_all_input_data(struct program_options* options, char*** hostnames, int* sample_count,
		int* host_count) {

	struct dirent* entry;
	DIR* dir;
	int file_count = 0;
	double*** result;
	if ((dir = opendir(options->input)) == NULL) {
		printf("Can't open %s\n", options->input);
		return NULL;
	}
	// see if there is a _allMeasurements.out file
	int all = 0;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_REG) { /* If the entry is a regular file */
			if (string_match_regex(entry->d_name, "_allMeasurements.out$")) {
				all = 1;
				break;
			}
		}
	}
	rewinddir(dir);

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_REG) { /* If the entry is a regular file */
			if ((string_match_regex(entry->d_name, "_allMeasurements.out$") && all) || !all) {
				//if present will only count _allMeasurements.out otherwise all files are count
				file_count++;
			}
		}
	}
	rewinddir(dir);

	*host_count = file_count;
	hostnames[0] = malloc(file_count * sizeof(char*));
	result = malloc(file_count * sizeof(double**));
	if (result == NULL || *hostnames == NULL) {
		printf("Error allocating memory\n");
		return NULL;
	}
	int min_samples = INT_MAX;
	int samples;
	int i = 0;
	int PRINTstr_len;
	int j;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_REG) { /* If the entry is a regular file */
			if ((string_match_regex(entry->d_name, "_allMeasurements.out$") && all) || !all) {
				//if present will only use _allMeasurements.out otherwise all files are used
				if (i >= *host_count) {
					printf("Error in reading all files\n");
					break;
				}
				//get hostname
				j = 0;
				while (entry->d_name[j] != '\0') {
					++j;
				}
				// find string end
				if (string_match_regex(entry->d_name, "_allMeasurements.out$")) {
					j = j - 20;
					//_allMeasurements.out
				} else {
					j = j - 4;
					//.out
				}
				char* hostname = malloc((j + 1) * sizeof(char));
				hostname[j] = '\0';
				memcpy(hostname, entry->d_name, j);

				hostnames[0][i] = hostname;

				char* filename = NULL;
				PRINTstr(filename, "%s/%s", options->input, entry->d_name);

				result[i] = get_input_data(options, filename, &samples);
				if (options->verbose && samples != min_samples && min_samples != INT_MAX) {
					printf("Warning: different number of samples found\n");
				}
				min_samples = min_samples < samples ? min_samples : samples;
				free(filename);

				++i;
			}
		}
	}
	closedir(dir);

	*sample_count = min_samples;
	return result;
}

// print the help
static void help_string() {
	printf(
			"usage: ./analyze [-o output -v -d -t threshold -c cluster -t top -h host] input \n"
					"-o output sets the output file\n"
					"-v be verbose and print some additional info to the output\n"
					"-d enabled the dir-mode. it will threat input-file as a directory and analyzes all filesas different hosts\n"
					"-t threshold sets the threshold until which the components are treated as they have no significant load\n"
					"-c cluster set the number of groups of similar samples that should be compared as the proportion of samples (1/cluster of all samples)\n"
					"-t top show the top x differences when comparing different Samples (and different hosts with -d)\n"
					"-h host set the hostname to compare the other data to (only relevant with -d)\n"
					"input is the input file");
}

// parse the given arguments
static void parse_args(int argc, char** argv, struct program_options* options, metric** metric_array) {
	int pc = 0;	// count the number of arguments encountered which could be treated like the program to measure
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {

			if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
				i++;
				options->output = argv[i];
			} else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
				options->dir = ON;
			} else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threshold") == 0) {
				i++;
				sscanf(argv[i], "%lf", &options->threshold);
			} else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cluster") == 0) {
				i++;
				options->clustering_factor = (int) strtol(argv[i], 0, 10);
			} else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
				options->verbose = ON;
			} else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--top") == 0) {
				i++;
				options->top_x = (int) strtol(argv[i], 0, 10);
			} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) {
				i++;
				options->hostname = argv[i];
			} else if (strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "--help") == 0) {
				help_string();
				options->input = NULL;	// close program
				return;
			}

		} else {	// not starting with -
			options->input = argv[i];
			pc++;
		}
	}
	if (pc != 1 || options->input == NULL) {
		printf("Error: no single input file given\n\n");
		options->input = NULL;
		help_string();
	}
	if (options->output == NULL) {
		// output = stdout
	}
}

void report_comparison(double** centers, int a, int b, int metrics_count, char** names, struct program_options* options,
		FILE* output, metric** metric_array) {
	double* buffer_a = malloc(metrics_count * sizeof(double));
	double* buffer_b = malloc(metrics_count * sizeof(double));
	double* buffer_dist = malloc(metrics_count * sizeof(double));
	char** name_buffer = malloc(metrics_count * sizeof(char*));

	int i;
	for (i = 0; i < metrics_count; ++i) {
		buffer_a[i] = centers[a][i];
		buffer_b[i] = centers[b][i];
		buffer_dist[i] = abs(buffer_a[i] - buffer_b[i]);	// absolute for sorting
		name_buffer[i] = names[i];
	}

// sorting with bubblesort:
	int n = metrics_count;
	int newn = 1;
	double tmpA, tmpB, tmpD;
	char* tmpC;
	do {
		newn = 1;
		for (i = 0; i < n - 1; ++i) {
			if (buffer_dist[i] < buffer_dist[i + 1]) {
				//swap:
				tmpD = buffer_dist[i];
				tmpA = buffer_a[i];
				tmpB = buffer_b[i];
				tmpC = name_buffer[i];
				buffer_dist[i] = buffer_dist[i + 1];
				buffer_a[i] = buffer_a[i + 1];
				buffer_b[i] = buffer_b[i + 1];
				name_buffer[i] = name_buffer[i + 1];
				buffer_dist[i + 1] = tmpD;
				buffer_a[i + 1] = tmpA;
				buffer_b[i + 1] = tmpB;
				name_buffer[i + 1] = tmpC;

				newn = i + 1;
			} // ende if
		} // ende for
		n = newn;
	} while (n > 1);

	fprintf(output, "\nCompare %c and %c\n", 'A' + a, 'A' + b);
	for (i = 0; i < options->top_x; ++i) {
		if (buffer_a[i] > buffer_b[i]) {
			fprintf(output, "Samples of group %c have on average %f more %s than samples of group %c\n", 'A' + a,
					buffer_dist[i], name_buffer[i], 'A' + b);
		} else {
			fprintf(output, "Samples of group %c have on average %f more %s than samples of group %c\n", 'A' + b,
					buffer_dist[i], name_buffer[i], 'A' + a);
		}

	}

	free(buffer_a);
	free(buffer_b);
	free(buffer_dist);
	free(name_buffer);
}

// much duplicate code with report_comparison

void report_host_comparison(double* buffer_a, char* name_a, double* buffer_b, char* name_b, int metrics_count,
		char** name_buffer, struct program_options* options, FILE* output, metric** metric_array) {
	double* buffer_dist = malloc(metrics_count * sizeof(double));

	int i;
	for (i = 0; i < metrics_count; ++i) {
		buffer_dist[i] = abs(buffer_a[i] - buffer_b[i]);	// absolute for sorting
	}

	// sorting with bubblesort:
	int n = metrics_count;
	int newn = 1;
	double tmpA, tmpB, tmpD;
	char* tmpC;
	do {
		newn = 1;
		for (i = 0; i < n - 1; ++i) {
			if (buffer_dist[i] < buffer_dist[i + 1]) {
				//swap:
				tmpD = buffer_dist[i];
				tmpA = buffer_a[i];
				tmpB = buffer_b[i];
				tmpC = name_buffer[i];
				buffer_dist[i] = buffer_dist[i + 1];
				buffer_a[i] = buffer_a[i + 1];
				buffer_b[i] = buffer_b[i + 1];
				name_buffer[i] = name_buffer[i + 1];
				buffer_dist[i + 1] = tmpD;
				buffer_a[i + 1] = tmpA;
				buffer_b[i + 1] = tmpB;
				name_buffer[i + 1] = tmpC;

				newn = i + 1;
			} // ende if
		} // ende for
		n = newn;
	} while (n > 1);

	// is reported in parent function
	//fprintf(output, "\nCompare %c and %c\n", 'A' + a, 'A' + b);
	for (i = 0; i < options->top_x; ++i) {
		if (buffer_a[i] > buffer_b[i]) {
			fprintf(output, "%s has on average %f more %s than %s\n", name_a, buffer_dist[i], name_buffer[i], name_b);
		} else {
			fprintf(output, "%s has on average %f more %s than %s\n", name_b, buffer_dist[i], name_buffer[i], name_a);
		}

	}

	free(buffer_dist);
}

static void get_sample_buffer(int* output_buffer_size, double** output_buffer, char*** output_name_buffer, int sample,
		double** data, struct program_options* options, FILE* output, metric** metric_array) {

	int i, pos, PRINTstr_len;
	int buffer_size = 1 + GLOBAL_metric_ids.number_disk_devices + GLOBAL_metric_ids.number_net_devices * 2
			+ GLOBAL_metric_ids.number_additional_metrics;
	// one for CPU average
	if (GLOBAL_metric_ids.cacheMetrics != -1) {
		++buffer_size;
	}
	if (GLOBAL_metric_ids.branceMetrics != -1) {
		++buffer_size;
	}

	*output_buffer_size = buffer_size;
	double* buffer = malloc(buffer_size * sizeof(double));
	char** name_buffer = NULL;
	if (output_name_buffer != NULL) {
		name_buffer = malloc(buffer_size * sizeof(double));
	}

	if (buffer == NULL || (output_name_buffer != NULL && name_buffer == NULL)) {
		printf("Error allocating memory");
		return;
	}

	// checks CPU usage and compute mean cpu utilization
	buffer[0] = check_cpu_usage(sample, data, output, options, metric_array);
	if (output_name_buffer != NULL) {
		PRINTstr(name_buffer[0], "%s", "Average CPU utilization");
	}
	pos = 1;

	for (i = 0; i < GLOBAL_metric_ids.number_disk_devices; ++i) {
		buffer[pos + i] = data[GLOBAL_metric_ids.diskMetrics + i * 3][sample];
		//is in percentage
		if (output_name_buffer != NULL) {
			char* device_name = malloc(MAX_DEVICE_NAME_LENGTH * sizeof(char));
			sscanf(metric_array[GLOBAL_metric_ids.diskMetrics + i * 3]->name,
					"IO Time device %s"MAX_DEVICE_NAME_LENGTH_S, device_name);
			PRINTstr(name_buffer[pos + i], "Utilization of disk %s", device_name);
			free(device_name);
		}
	}

	pos += GLOBAL_metric_ids.number_disk_devices;
	for (i = 0; i < GLOBAL_metric_ids.number_net_devices; ++i) {

		double send = data[GLOBAL_metric_ids.netMetrics + i * 2][sample];
		double recv = send = data[GLOBAL_metric_ids.netMetrics + i * 2 + 1][sample];
		buffer[pos + i] = (send / metric_array[GLOBAL_metric_ids.netMetrics + i * 2]->maximum) * 100;
		buffer[pos + i + 1] = (recv / metric_array[GLOBAL_metric_ids.netMetrics + i * 2 + 1]->maximum) * 100;

		if (output_name_buffer != NULL) {
			char* device_name = malloc(MAX_DEVICE_NAME_LENGTH * sizeof(char));
			sscanf(metric_array[GLOBAL_metric_ids.netMetrics + i * 2]->name,
					"Bytes send by network device %s"MAX_DEVICE_NAME_LENGTH_S, device_name);
			PRINTstr(name_buffer[pos + i], "Utilization of network bandwidth for sending with device %s", device_name);
			PRINTstr(name_buffer[pos + i + 1], "Utilization of network bandwidth for receiving with device %s",
					device_name);
			free(device_name);
		}
	}

	pos += GLOBAL_metric_ids.number_net_devices * 2;
	if (GLOBAL_metric_ids.cacheMetrics != -1) {
		buffer[pos] = data[GLOBAL_metric_ids.cacheMetrics][sample];
		// is in percentage
		if (output_name_buffer != NULL) {
			PRINTstr(name_buffer[pos], "%s", metric_array[GLOBAL_metric_ids.cacheMetrics]->name);
		}
		++pos;
	}
	if (GLOBAL_metric_ids.branceMetrics != -1) {
		buffer[pos] = data[GLOBAL_metric_ids.branceMetrics][sample];
		// is in percentage
		if (output_name_buffer != NULL) {
			PRINTstr(name_buffer[pos], "%s", metric_array[GLOBAL_metric_ids.branceMetrics]->name);
		}
		++pos;
	}
	if (GLOBAL_metric_ids.additional_metrics != -1) {
		for (i = 0; i < GLOBAL_metric_ids.number_additional_metrics; ++i) {

			buffer[pos + i] = data[GLOBAL_metric_ids.additional_metrics + i][sample]
					/ metric_array[GLOBAL_metric_ids.additional_metrics + i]->maximum * 100;
			if (output_name_buffer != NULL) {
				PRINTstr(name_buffer[pos], "%s", metric_array[GLOBAL_metric_ids.additional_metrics + i]->name);
			}
		}
	}

	double positive_nan = strtod("NaN", NULL);
	double negative_nan = strtod("-NaN", NULL);
	double positive_inf = strtod("Inf", NULL);
	double negative_inf = strtod("-Inf", NULL);
	for (i = 0; i < buffer_size; ++i) {
		if (buffer[i] == positive_inf || buffer[i] == negative_inf || buffer[i] == positive_nan
				|| buffer[i] == negative_nan || buffer[i] != buffer[i]) {
			// x!=x => x is NaN
			if (options->verbose) {
				if (output_name_buffer != NULL) {
					printf("Warning: value for %s is %f in sample %i\n", name_buffer[i], buffer[i], sample);
				} else {
					printf("Warning: value for metric %i is %f in sample %i\n", i, buffer[i], sample);
				}
			}
			buffer[i] = -1;
		}
	}

	*output_buffer = buffer;
	if (output_name_buffer != NULL) {
		*output_name_buffer = name_buffer;
	}
}

// get threshold when two sample buffers are considered very close

double get_threshold(struct program_options* options, metric** metric_array) {
	int i;
	double** data = malloc(GLOBAL_metric_number * sizeof(double*));

	for (i = 0; i < GLOBAL_metric_number; ++i) {
		data[i] = malloc(2 * sizeof(double));
		data[i][0] = 0;
		data[i][1] = metric_array[i]->margin;
	}

	int buffer_count = 0;
	double* buffer_a;
	double* buffer_b;
	FILE* dev_null = fopen("/dev/null", "w");
	get_sample_buffer(&buffer_count, &buffer_a, NULL, 0, data, options, dev_null, metric_array);
	get_sample_buffer(&buffer_count, &buffer_b, NULL, 1, data, options, dev_null, metric_array);
	fclose(dev_null);

	double dist = 0;
	dist = euclid_dist_2(buffer_count, buffer_a, buffer_b);
	// it is squared distance as all other distances are squared as well
	free(buffer_a);
	free(buffer_b);
	for (i = 0; i < GLOBAL_metric_number; ++i) {
		free(data[i]);
	}
	free(data);
	return dist;
}

// transpose the data and fill missing spot with -1

double** transpose_and_fill(int* metrics_count, int sample_count, double** data, struct program_options* options) {

	double** result = malloc(sample_count * sizeof(double*));
	if (result == NULL) {
		printf("Error allocating memory");
		return NULL;
	}

	int i;

	// check for some wired conditions
	double a_nan = strtod("NaN", NULL);
	double a_inf = strtod("Inf", NULL);

	// count all measured metrics
	for (i = 0; i < GLOBAL_metric_number; ++i) {
		if (options->measure_metrics[i] == ON) {
			++*metrics_count;
		}
	}

	// for each sample:
	for (i = 0; i < sample_count; ++i) {
		result[i] = malloc(*metrics_count * sizeof(double));
		if (result[i] == NULL) {
			printf("Error allocating memory");
			return NULL;
		}

		int j, k;
		j = 0;
		k = 0;
		//for each metric
		while (j < *metrics_count && k < GLOBAL_metric_number) {
			if (options->measure_metrics[k] == ON) {
				if (data[k] != NULL) {
					result[i][j] = data[k][i];

					if (result[i][j] == a_inf || result[i][j] == a_nan || result[i][j] != result[i][j]) {
						// x!=x => x is NaN
						if (options->verbose) {
							printf("Warning: value for metric %i is %f in sample %i\n", k, result[i][j], i);
						}
						result[i][j] = -1;

					}
				} else {
					result[i][j] = -1;
				}
				++j;
			}
			++k;
		}

	}

	return result;
}

static void compare_hosts(int host_count, int sample_count, double*** all_data, char** hostnames,
		struct program_options* options, FILE* output, metric** metric_array) {

	fprintf(output, "Comparison of the %i most significant differences between the hosts:\n", options->top_x);

	int i, j, k, n;
	// store the distances of two hosts
	struct dist {
		double d;
		int sample;
		int a;
		int b;
	};
	FILE* dev_null = fopen("/dev/null", "w");

	int data_buffer_size;
	double* data_buffer_a;
	double* data_buffer_b;
	// Borrows code from compare_samples
	int combinations = sample_count * (host_count * (host_count - 1)) / 2;
	struct dist** buffer = malloc(combinations * sizeof(struct dist*));
	n = 0;
	for (i = 0; i < sample_count; ++i) {
		for (j = 0; j < host_count; ++j) {
			get_sample_buffer(&data_buffer_size, &data_buffer_a, NULL, i, all_data[j], options, dev_null, metric_array);
			for (k = j + 1; k < host_count; ++k) {
				get_sample_buffer(&data_buffer_size, &data_buffer_b, NULL, i, all_data[k], options, dev_null,
						metric_array);
				struct dist* d = malloc(sizeof(struct dist));
				d->d = euclid_dist_2(data_buffer_size, data_buffer_a, data_buffer_b);
				d->sample = i;
				d->a = j;
				d->b = k;
				buffer[n] = d;
				++n;
				free(data_buffer_b);
			}
			free(data_buffer_a);
		}
	}

	if (n != combinations) {
		printf(
				"Warning: some error while comparing the samples. Combinations that should be considered: %i combination that where actually considered: %i\n",
				combinations, n);
		combinations = n;
	}
	// sorting bubblesort:
	int newn = 1;
	struct dist* tmp;
	do {
		newn = 1;
		for (i = 0; i < n - 1; ++i) {
			if (buffer[i]->d < buffer[i + 1]->d) {
				//swap:
				tmp = buffer[i];
				buffer[i] = buffer[i + 1];
				buffer[i + 1] = tmp;
				newn = i + 1;
			} // ende if
		} // ende for
		n = newn;
	} while (n > 1);

	int count = 0;
	double threshold = get_threshold(options, metric_array);
	for (i = 0; i < options->top_x && i < combinations; ++i) {
		if (buffer[i]->d > threshold) {
			fprintf(output, "\nCompare sample %i from %s and %s\n", buffer[i]->sample, hostnames[buffer[i]->a],
					hostnames[buffer[i]->b]);
			char** metric_name_buffer;
			get_sample_buffer(&data_buffer_size, &data_buffer_a, &metric_name_buffer, buffer[i]->sample,
					all_data[buffer[i]->a], options, dev_null, metric_array);
			get_sample_buffer(&data_buffer_size, &data_buffer_b, NULL, buffer[i]->sample, all_data[buffer[i]->b],
					options, dev_null, metric_array);

			report_host_comparison(data_buffer_a, hostnames[buffer[i]->a], data_buffer_b, hostnames[buffer[i]->b],
					data_buffer_size, metric_name_buffer, options, output, metric_array);
			++count;
			free(data_buffer_a);
			free(data_buffer_b);
			for (j = 0; j < data_buffer_size; ++j) {
				free(metric_name_buffer[j]);
			}
			free(metric_name_buffer);
		}
	}
	if (count == 0) {
		fprintf(output, "No significant differences found\n");
	}

	for (i = 0; i < combinations; ++i) {
		free(buffer[i]);
	}
	free(buffer);

	fclose(dev_null);
}

static void compare_samples(int sample_count, double** data, struct program_options* options, FILE* output,
		metric** metric_array) {

	int metrics_count;
	int i, j, n, count;

	FILE* dev_null = fopen("/dev/null", "w");
	int buffer_len;
	char** name_buffer;

	double** trans = malloc(sample_count * sizeof(double*));

	if (trans == NULL) {
		return;
	}

	i = 0;
	get_sample_buffer(&metrics_count, &trans[i], &name_buffer, i, data, options, dev_null, metric_array);
	for (i = 1; i < sample_count; ++i) {
		get_sample_buffer(&buffer_len, &trans[i], NULL, i, data, options, dev_null, metric_array);
		if (buffer_len != metrics_count) {
			printf("Warning: different number of metrics in sample %i\n", i);
		}
	}
	fclose(dev_null);

	int k = sample_count / options->clustering_factor;
	if (k < 3 && sample_count >= 3) {
		k = 3;
	}
	int* membership = malloc(sample_count * sizeof(int));
	if (membership == NULL) {
		printf("Error allocating memory");
		return;
	}

//	/*----< kmeans_clustering() >------------------------------------------------*/
//	/* return an array of cluster centers of size [numClusters][numCoords]       */
//	float** omp_kmeans(int is_perform_atomic, /* in: */
//	float **objects, /* in: [numObjs][numCoords] */
//	int numCoords, /* no. coordinates */
//	int numObjs, /* no. objects */
//	int numClusters, /* no. clusters */
//	float threshold, /* % objects change membership */
//	int *membership)
//	/* out: [numObjs] */
	double** clusters = omp_kmeans(1, trans, metrics_count, sample_count, k, 0.01, membership);

// store the distances of two cluster centers
	struct dist {
		double d;
		int a;
		int b;
	};

// compute all distances and sort them
// as these dists are used for comparison it is enough to compute the square
	int combinations = (k * (k - 1)) / 2;
	struct dist** buffer = malloc(combinations * sizeof(struct dist*));
	n = 0;

	for (i = 0; i < k; ++i) {
		for (j = i + 1; j < k; ++j) {
			struct dist* d = malloc(sizeof(struct dist));
			d->d = euclid_dist_2(metrics_count, clusters[i], clusters[j]);
			d->a = i;
			d->b = j;
			buffer[n] = d;
			++n;
		}
	}

	/*//DEBUG:
	 for (i = 0; i < sample_count; ++i) {
	 printf("data %i (in cluster %i):\n", i, membership[i]);
	 for (j = 0; j < metrics_count; ++j) {
	 printf("%f ", clusters[i][j]);
	 }
	 printf("\n", i);
	 }

	 for (i = 0; i < k; ++i) {
	 printf("cluster %i:\n", i);
	 for (j = 0; j < metrics_count; ++j) {
	 printf("%f ", clusters[i][j]);
	 }
	 printf("\n", i);
	 }
	 */

	if (n != combinations) {
		printf(
				"Warning: some error while comparing the samples. combinations that should be considered: %i combination that where actually considered: %i\n",
				combinations, n);
		combinations = n;
	}

// sorting bubblesort:
	int newn = 1;
	struct dist* tmp;
	do {
		newn = 1;
		for (i = 0; i < n - 1; ++i) {
			if (buffer[i]->d < buffer[i + 1]->d) {
				//swap:
				tmp = buffer[i];
				buffer[i] = buffer[i + 1];
				buffer[i + 1] = tmp;
				newn = i + 1;
			} // ende if
		} // ende for
		n = newn;
	} while (n > 1);

// report a random sample from each cluster:
// for each cluster
	for (i = 0; i < k; ++i) {
		count = 0;
		for (j = 0; j < sample_count; ++j) {
			if (membership[j] == i) {
				++count;
			}
		}

		if (count == 0) {
			printf("Warning: Error in clustering\n");
			++count;
		}
		j = 0;
		int random = rand() % count;
		while (random > 0) {
			++j;
			if (membership[j] == i) {
				--random;
			}
		}

		fprintf(output, "Report for sample %i of %i (there where found %i similar samples in group %c):\n", j + 1,
				sample_count, count, 'A' + i);
		report_sample(j, data, output, options, metric_array);

	}

	fprintf(output, "Comparison of the most significant differences between different sample groups:\n");
	fprintf(output, "Samples are clustered in different groups of similar Samples:\n");
	for (i = 0; i < sample_count; ++i) {
		fprintf(output, "%c", 'A' + membership[i]);
	}
	fprintf(output, "\n");

// report the main differences found:
	double threshold = get_threshold(options, metric_array);
	count = 0;
	for (i = 0; i < options->top_x && buffer[i]->d > threshold; ++i) {
		report_comparison(clusters, buffer[i]->a, buffer[i]->b, metrics_count, name_buffer, options, output,
				metric_array);
		++count;
	}
	if (count == 0) {
		fprintf(output, "No significant differences found\n");
	}
	fprintf(output, "\n");

	for (i = 0; i < combinations; ++i) {
		free(buffer[i]);
	}
	free(buffer);

	free(clusters[0]);
	free(clusters);

	for (i = 0; i < sample_count; ++i) {
		free(trans[i]);
	}
	free(trans);
	for (i = 0; i < metrics_count; ++i) {
		free(name_buffer[i]);
	}
	free(name_buffer);
	free(membership);

}

// checks cpu usage and returns avg cpu utilization
static double check_cpu_usage(int sample, double** data, FILE* output, struct program_options* options,
		metric** metric_array) {

	double result = 0;
	int pos;
	int i;
	double margin = 0;

	int cpu_buffer_size = GLOBAL_metric_ids.number_cpu;
	double* cpu_buffer = malloc(cpu_buffer_size * sizeof(double));

	for (i = 0; i < cpu_buffer_size; ++i) {
		pos = GLOBAL_metric_ids.cpuMetrics + 3 + (i * 4);
		// points to position of CPUi idle

		if (data[pos] == NULL || metric_array[pos]->maximum == -1) {
			printf("Error reading values for CPU%i", i);
			cpu_buffer[i] = 0;

		} else {
			cpu_buffer[i] = (1 - (data[pos][sample] / metric_array[pos]->maximum)) * 100;
			// save time cpu is NOT idle
			margin = margin > metric_array[pos]->margin ? margin : metric_array[pos]->margin; //max
		}
		result += cpu_buffer[i];
	}
	result = result / cpu_buffer_size;	// compute mean

	//check CPU usage

	double max_usage = 0;
	int count = 0;
	double sum = 0;

	for (i = 0; i < cpu_buffer_size; ++i) {
		max_usage = max_usage > cpu_buffer[i] ? max_usage : cpu_buffer[i];
	}
	int different = 0;
	for (i = 0; i < cpu_buffer_size; ++i) {
		if (max_usage - margin > cpu_buffer[i]) {
			different = 1;
			break;
		}
	}

	if (different == 1) {
		fprintf(output,
				"\nCPU utilization is different between the CPUs you may investigate the possibility to parallel your program:\n");
		fprintf(output, "CPU ");
		for (i = 0; i < cpu_buffer_size; ++i) {
			if (max_usage - margin <= cpu_buffer[i]) {
				sum += cpu_buffer[i];
				++count;
				fprintf(output, "%i ", i);
			}
		}
		fprintf(output, "are utilized %f on average\n", sum / count);
		sum = 0;
		count = 0;
		fprintf(output, "while CPU ");
		for (i = 0; i < cpu_buffer_size; ++i) {
			if (max_usage - margin > cpu_buffer[i]) {
				sum += cpu_buffer[i];
				++count;
				fprintf(output, "%i ", i);
			}
		}

		fprintf(output, "are only utilized %f on average\n", sum / count);
	} else {
		//CPU Are used quite similar
		if (options->verbose) {
			fprintf(output, "All CPUs are utilized at similar load (average %f)\n", result);
		}
	}

	free(cpu_buffer);
	return result;
}

static void check_cache_and_brances(int sample, double** data, FILE* output, metric** metric_array) {

	if (GLOBAL_metric_ids.cacheMetrics != -1
			&& data[GLOBAL_metric_ids.cacheMetrics][sample] > metric_array[GLOBAL_metric_ids.cacheMetrics]->margin) {
		fprintf(output,
				"\nYou may enhance your applications cache Performance. currently there is a cache miss ratio of %f\n",
				data[GLOBAL_metric_ids.cacheMetrics][sample]);
	}

	if (GLOBAL_metric_ids.branceMetrics != -1
			&& data[GLOBAL_metric_ids.branceMetrics][sample] > metric_array[GLOBAL_metric_ids.branceMetrics]->margin) {
		fprintf(output,
				"\nYou may went to optimize your applications branches. currently there is a brance miss ratio of %f\n",
				data[GLOBAL_metric_ids.branceMetrics][sample]);
	}

// here it will check other user defined metric for the same pattern:

	for (int i = 0; i < GLOBAL_metric_ids.number_additional_metrics; ++i) {
		// if high value is not positive to performance
		if (metric_array[i + GLOBAL_metric_ids.additional_metrics]->positive == 0
				&& data[i + GLOBAL_metric_ids.additional_metrics][sample]
						> metric_array[i + GLOBAL_metric_ids.additional_metrics]->margin) {
			fprintf(output, "\nMetric %s has a relatively high value of %f you may want to optimize here\n",
					metric_array[i + GLOBAL_metric_ids.additional_metrics]->name,
					data[i + GLOBAL_metric_ids.additional_metrics][sample]);
		}
	}

}

static void check_IO_wait(int sample, double** data, FILE* output, metric** metric_array) {

	double max_iowait = 0;
	double avg_iowait = 0;
	int cpu = 0;
	int pos, i;

	for (i = 0; i < GLOBAL_metric_ids.number_cpu; ++i) {
		pos = GLOBAL_metric_ids.cpuMetrics + 2 + (i * 4);
		// points to position of CPUi idle

		if (data[pos] == NULL || metric_array[pos]->maximum == -1) {
			printf("Error reading values for CPU%i", i);
			printf("THIS SHOULD NEVER HAPPEN!\n");
			// then there was a failure in setting up the number of CPU

		} else {
			avg_iowait += data[pos][sample];
			if (data[pos][sample] > metric_array[pos]->margin) {
				if (max_iowait < data[pos][sample]) {
					max_iowait = data[pos][sample];
					cpu = i;
				}
			}
		}

	}
	avg_iowait = avg_iowait / GLOBAL_metric_ids.number_cpu;

	if (max_iowait > 0) {
		fprintf(output,
				"\n Your Application seems quite IO heavy. CPU %i uses %f percent performing IO operations. Average for all CPUs: %f\n",
				cpu, max_iowait, avg_iowait);
		if (GLOBAL_metric_ids.diskMetrics != -1) {
			// if there exist disk-metrics
			double avg = 0;
			double disk_util = 0;

			for (i = 0; i < GLOBAL_metric_ids.number_disk_devices; ++i) {
				pos = GLOBAL_metric_ids.diskMetrics + 0 + (i * 3);
				disk_util = data[pos][sample];		// already in percentage
				avg += disk_util;
				char* device_name = malloc(MAX_DEVICE_NAME_LENGTH * sizeof(char));
				sscanf(metric_array[pos]->name, "IO Time device %s"MAX_DEVICE_NAME_LENGTH_S, device_name);
				// if disk load is full:
				if (disk_util > metric_array[pos]->maximum - metric_array[pos]->margin) {
					fprintf(output,
							"Disk %s seems to be under full load. it uses %f percent of the time to perform IO. Maybe one can optimize the IO pattern.\n",
							device_name, disk_util);
				}
				// if disk load is not full:
				if (disk_util < metric_array[pos]->maximum - metric_array[pos]->margin) {
					fprintf(output,
							"But Disk %s seems not saturated. It uses %f percent of the time to perform IO. Maybe one can optimize the IO pattern.\n",
							device_name, disk_util);
				}

				free(device_name);

			}
			avg = avg / GLOBAL_metric_ids.number_disk_devices;
			fprintf(output, "On average the Disks are Busy %f percent of the time\n", avg);

		}
	}

}

// report für ein sample
static void report_sample(int sample, double** data, FILE* output, struct program_options* options,
		metric** metric_array) {

	int i;
	int buffer_size;
	double* buffer;
	char** name_buffer;

	get_sample_buffer(&buffer_size, &buffer, &name_buffer, sample, data, options, output, metric_array);

	// empfehlungen :
	check_cache_and_brances(sample, data, output, metric_array);
	check_IO_wait(sample, data, output, metric_array);

	// sort:
	int n = buffer_size;
	int newn = 1;
	double tmpD;
	char* tmpC;
	do {
		newn = 1;
		for (i = 0; i < n - 1; ++i) {
			if (buffer[i] < buffer[i + 1]) {
				//swap:
				tmpD = buffer[i];
				tmpC = name_buffer[i];
				buffer[i] = buffer[i + 1];
				name_buffer[i] = name_buffer[i + 1];
				buffer[i + 1] = tmpD;
				name_buffer[i + 1] = tmpC;

				newn = i + 1;
			} // ende if
		} // ende for
		n = newn;
	} while (n > 1);
	// bubblesort because we have to sort the id buffer accordingly so that it still matches the values
	// and this is easier to implement in bubblesort :)

	fprintf(output, "\nMost used System components:\n");

	// print all which are greater then the threshold
	for (i = 0; i < buffer_size && buffer[i] > options->threshold; ++i) {
		fprintf(output, "%s %f%%\n", name_buffer[i], buffer[i]);

	}

	fprintf(output, "least used System components:\n");

	// print all which are smaller then the threshold
	for (i = buffer_size - 1; i >= 0 && buffer[i] < options->threshold; --i) {
		fprintf(output, "%s %f%%\n", name_buffer[i], buffer[i]);

	}

	free(buffer);
	for (i = 0; i < buffer_size; ++i) {
		free(name_buffer[i]);

	}
	free(name_buffer);

	fprintf(output, "\n\n");

}

// init the options with default params
static struct program_options* init_options() {
	struct program_options* options = malloc(sizeof(struct program_options));

	options->dir = OFF;
	options->threshold = 50;	// 50 %
	options->clustering_factor = 10;
	options->top_x = 3;
	options->verbose = OFF;
	options->hostname = "";

	// is initialized in parse params;
	options->input = NULL;
	options->output = NULL;

	options->measure_metrics = malloc(sizeof(int) * GLOBAL_metric_number);
	if (options->measure_metrics == NULL) {
		return 0;
	}
	for (int i = 0; i < GLOBAL_metric_number; ++i) {
		options->measure_metrics[i] = OFF;
	}

	return options;
}

int main(int argc, char** argv) {

	int i;

	srand(time(NULL));
	metric** metric_array = init_metrics();
	if (metric_array == NULL) {
		printf("Error allocating memory");
		return -1;
	}
	// do a sanity check:
	if (check_metrics(metric_array) != 0) {
		return -1;
	}
	// initialize options:
	struct program_options* options = init_options();
	if (options == NULL) {
		printf("Error allocating memory");
		return -1;
	}

	parse_args(argc, argv, options, metric_array);
	if (options->input == NULL) {
		return -1;
	}

	int sample_count;
	double** data = NULL;
	int host_count = 1;
	double*** all_data;
	char** hostnames = NULL;
	if (options->dir) {
		all_data = get_all_input_data(options, &hostnames, &sample_count, &host_count);
		if (all_data == NULL) {
			return -1;
		}
		if (host_count == 1) {
			data = all_data[0];
			free(hostnames[0]);
			free(hostnames);
		}
	} else {
		data = get_input_data(options, options->input, &sample_count);
		if (data == NULL) {
			return -1;
		}
		all_data = malloc(1 * sizeof(double*));
		all_data[0] = data;
	}

	init_metric_ids(options, metric_array);

	FILE* output;
	if (options->output == NULL) {
		output = stdout;
	} else {
		output = fopen(options->output, "w");
	}

	char* hostname;
	if (host_count > 1) {
		data = all_data[0];
		hostname = hostnames[0];
		for (int i = 0; i < host_count; ++i) {
			if (strcmp(options->hostname, hostnames[i]) == 0) {
				data = all_data[i];
				hostname = hostnames[i];
			}
		}
		fprintf(output, "Analysis of the data for %s:\n\n", hostname);
	}

	if (sample_count > 1) {
		if (options->verbose) {
			//report for every sample
			for (int i = 0; i < sample_count; ++i) {
				fprintf(output, "Report for sample %i of %i:\n", i + 1, sample_count);
				report_sample(i, data, output, options, metric_array);

				fprintf(output, "\n");
			}
		}

		compare_samples(sample_count, data, options, output, metric_array);

		// mean value as first data
		for (i = 0; i < GLOBAL_metric_number; ++i) {
			if (data[i] != NULL) {
				double sum = 0;
				for (int j = 0; j < sample_count; ++j) {
					sum = sum + data[i][j];
				}
				data[i][0] = sum / sample_count;
			}
		}

	}

	// zusammenfassung mit den mittelwerten
	fprintf(output, "Summary for mean values:\n");
	report_sample(0, data, output, options, metric_array);

	if (host_count > 1) {
		compare_hosts(host_count, sample_count, all_data, hostnames, options, output, metric_array);

		for (i = 0; i < host_count; ++i) {
			free(hostnames[i]);
		}
		free(hostnames);
	}

	if (options->output != NULL) {
		fclose(output);
	}

	for (i = 0; i < host_count; ++i) {
		for (int j = 0; j < GLOBAL_metric_number; ++j) {
			if (all_data[i][j] != NULL) {
				free(all_data[i][j]);
			}
		}
		free(all_data[i]);
	}
	free(all_data);

	free_all_Metrics(metric_array);
	free(options->measure_metrics);
	free(options);

	printf("done\n");
	return 0;
}
