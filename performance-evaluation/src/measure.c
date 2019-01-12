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
#include <sys/wait.h>

#ifdef USE_MPI
#include <mpi.h>
#endif

struct program_options {
	char* output; //output-file
	char* program; // program to measure
	int number; //number of measurements to take
	int max_tools; //maximum number of tools to use simultaneously
	int* measure_metrics; // list of metrics to measure
	int metrics_count; // count the number of metrics requested to measure
	int dir; // directory-mode
	int additional; // print all values
	double samplingrate; // samplingrate in Hz -1 for no sampling
	pid_t pid; // used for sampling
};

struct list_elem {
	double value;
	struct list_elem* next;
};

int GLOBAL_metric_number = 0;

#ifdef USE_MPI
struct mpi_info {
	MPI_Request recv_stop_message;
	char buffer; // buffer for receiving stop message
	int execute_command;// true for master false for all others
	// master stores the numranks here
	int* other_ranks;
// master stores the list of other processes here
};
struct mpi_info GLOBAL_mpi_info;
#endif

// frees only the metrics that are not needed anymore
static void free_unused_Metrics(struct program_options* options,
		metric** metric_array) {
	for (int i = 0; i < GLOBAL_metric_number; i++) {
		if (options->measure_metrics[i] == OFF) {
			if (metric_array[i] != NULL) {
				if (metric_array[i]->arguments != NULL) {
					metric_array[i]->clear_function(metric_array[i]->arguments);
				}
				free(metric_array[i]->name);
				free(metric_array[i]->keyword);
				free(metric_array[i]->help);
			}
			free(metric_array[i]);
			metric_array[i] = NULL;
		}

	}

}

// evaluate if the param enables/disables a metric
static void eval_param(char* param, struct program_options* options,
		metric** metric_array) {
	int fits = 0;
	int i = 0;
	char* t = NULL;
	char* f = NULL;
	int PRINTstr_len;

	for (i = 0; i < GLOBAL_metric_number; ++i) {

		PRINTstr(t, "--%s=true", metric_array[i]->keyword);
		PRINTstr(f, "--%s=false", metric_array[i]->keyword);

		// if this param fits a metric we do not need to check the other metrics
		if (strcmp(t, param) == 0) {
			options->measure_metrics[i] = ON;
			if (metric_array[i]->standard == OFF) {
				options->metrics_count++;
			}
			fits++;
			break;
		}
		if (strcmp(f, param) == 0) {
			options->measure_metrics[i] = OFF;
			if (metric_array[i]->standard == ON) {
				options->metrics_count--;
			}
			fits++;
			break;
		}
		free(t);
		free(f);
	}
	free(t);
	free(f);

	if (fits == 0) {
		printf("ignoring unrecognized argument %s\n", param);
	}
}

// print the help
static void help_string(metric** metric_array) {
	printf(
			"usage: ./measurement [-o output -a -d -m max -n number -s samplingrate --metric1=true --metric2=false ...] program \n"
					"-a enables the output of all collected data additional to the mean values\n"
					"-o output sets the output file\n"
					"-d enabled the dir-mode. it will threat output-file as a directory to write the output into. use it when analyzing an application across multiple nodes\n"
					"-m max sets the maximum number of tools to use concurrently (standard: measure all concurrently)\n"
					"-n number sets the number of measurements (standard 1)\n"
					"-s samplingrate switches on sampling with the given samplingrate in Hz (it is possible to give a float)"
					"--metric1=true switches on the measurement of metric1\n"
					"--metric2=false switches on the measurement of metric2\n"
					"program is the program to be measured\n\n"
					"The program name should not start with \"-\" as it  would be treated like a metric name\n\n"
					"currently available are %i metrics:\n",
			GLOBAL_metric_number);

	for (int i = 0; i < GLOBAL_metric_number; ++i) {
		printf("%s (--%s ", metric_array[i]->name, metric_array[i]->keyword);
		if (metric_array[i]->standard == ON) {
			printf("default=true");
		} else {
			printf("default=false");

		}
		printf("): %s\n", metric_array[i]->help);

	}
}

// parse the given arguments
static void parse_args(int argc, char** argv, struct program_options* options,
		metric** metric_array) {
	int pc = 0;	// count the number of arguments encountered which could be treated like the program to measure
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {

			if (strcmp(argv[i], "-o") == 0
					|| strcmp(argv[i], "--output") == 0) {
				i++;
				options->output = argv[i];
			} else if (strcmp(argv[i], "-m") == 0
					|| strcmp(argv[i], "--max") == 0) {
				i++;
				options->max_tools = (int) strtol(argv[i], 0, 10);
			} else if (strcmp(argv[i], "-n") == 0
					|| strcmp(argv[i], "--number") == 0) {
				i++;
				options->number = (int) strtol(argv[i], 0, 10);
			} else if (strcmp(argv[i], "-s") == 0
					|| strcmp(argv[i], "--sampling") == 0) {
				i++;
				sscanf(argv[i], "%lf", &options->samplingrate);
			} else if (strcmp(argv[i], "-d") == 0
					|| strcmp(argv[i], "--dir") == 0) {
				options->dir = ON;
			} else if (strcmp(argv[i], "-a") == 0
					|| strcmp(argv[i], "--additional") == 0) {
				options->additional = ON;
			} else if (strcmp(argv[i], "-?") == 0
					|| strcmp(argv[i], "--help") == 0) {
				help_string(metric_array);
				options->program = 0;	// close program
				return;
			} else {
				eval_param(argv[i], options, metric_array);
			}

		} else {	// not starting with -
			//TODO stop parsing args and concat the following args as they are supposed for the programm
			options->program = argv[i];
			pc++;
		}
	}
	if (pc != 1 || options->program == 0) {
		printf("Error: no single program to measure\n\n");
		options->program = 0;
		help_string(metric_array);
	}
	if (options->output == 0) {
		options->output = "measurement.out";
	}

}

static void set_up_sampling(pid_t pid, struct program_options* options,
		metric** metric_array) {
	if (pid == 0) {	// child the process who execute the command
		// it do not need any mem
		free_all_Metrics(metric_array);
		free(options->measure_metrics);

	} else {
		//parent: the process who measures
		for (int i = 0; i < GLOBAL_metric_number; ++i) {
			if (metric_array[i]->call_type_flag == CALL_TYPE_PROFILE)
				options->measure_metrics[i] = OFF;
		}
		// switch of all profile calls as they will not be supported in sampling

		options->number = 10 * options->samplingrate;
		// allocate enough buffer for the first 10 seconds minimum 10
		options->number = options->number < 10 ? 10 : options->number;

		// it will use realloc if necessary
		options->pid = pid;
	}

}

// measure all necessary metrics

static rstruct** measure_single(int* result_count,
		struct program_options* options, metric** metric_array) {
	int res_count = 0;
	int i, n;	// loop variables
	int max_result_count = options->number * options->metrics_count;
	// allocate a buffer for all measured values
	rstruct** result_buffer = calloc(max_result_count, sizeof(rstruct*));
	if (result_buffer == NULL) {
		printf("Error allocating memory\n");
		return 0;
	}

	int seconds = 0;
	unsigned int sample_time = 1000000 * pow(options->samplingrate, -1);
	if (options->samplingrate != -1) {
		unsigned long int long_sample_time = 1000000
				* pow(options->samplingrate, -1);
		if (long_sample_time > 1000000) {
			seconds = 1;	// use sleep instead of usleep
			sample_time = (unsigned int) (long_sample_time / 1000000);
		}
	}

	for (n = 0; n < options->number; n++) {
		for (i = 0; i < GLOBAL_metric_number; i++) {

			if (options->measure_metrics[i] == ON) {
				//true only if it was not measured yet and is requested by the user
				rstruct* result = 0;
				if (metric_array[i]->call_type_flag == CALL_TYPE_PROFILE
						|| metric_array[i]->call_type_flag == CALL_TYPE_BOTH) {
					result = metric_array[i]->profiling_function(
							options->program, metric_array[i]->arguments);
					// no sampling with profiling
				}
				if (metric_array[i]->call_type_flag == CALL_TYPE_MONITOR) {
					struct stop_monitor_struct* stop_function_call =
							metric_array[i]->monitor_function(options->program,
									metric_array[i]->arguments);
					if (options->samplingrate != -1) {
						if (seconds) {
							sleep(sample_time);
						} else {
							usleep(sample_time);
						}
					} else {	// no sampling
						system(options->program);	// execute the program
					}
					result = stop_function_call->stop_call(
							stop_function_call->args,
							metric_array[i]->arguments);
					free(stop_function_call);

				}
				if (result != NULL) {
					result_buffer[res_count] = result;
					res_count++;
					// do not measure values again
					for (int j = 0; j < result->resultcount; j++) {
						int metricID = result->id_list[j];
						if (options->measure_metrics[metricID] == ON) {
							options->measure_metrics[metricID] =
							ALREADY_MEASURED;
						}
					}
				}
			}
		}	// end measured all metrics
		for (i = 0; i < GLOBAL_metric_number; i++) {
			if (options->measure_metrics[i] == ALREADY_MEASURED) {
				options->measure_metrics[i] = ON;// set it to measure it again in the next measurement phase
			}
		}
		if (options->samplingrate != -1) {
			--n;	// do not stop until other process finished
			int exit = 0;
#ifdef USE_MPI
			if (GLOBAL_mpi_info.execute_command) {
#endif
			if (waitpid(options->pid, NULL, WNOHANG) != 0) {//check if process not exist anymore
				exit = 1;
				// exit sampling
#ifdef USE_MPI
				GLOBAL_mpi_info.buffer = '1';
				for (int t = 1; t < GLOBAL_mpi_info.execute_command; ++t) {
					if (GLOBAL_mpi_info.other_ranks[t]) {
						MPI_Send(&GLOBAL_mpi_info.buffer, 1, MPI_CHAR, t, 0, MPI_COMM_WORLD);
						// send stop message if rank t is active
					}
				}
#endif
			}
#ifdef USE_MPI
		} else {
			//test if received stop message
			MPI_Test(&GLOBAL_mpi_info.recv_stop_message, &exit, MPI_STATUS_IGNORE);
		}
#endif
			if (exit) {
				options->number = n;
			} else {
				if (res_count == max_result_count) {
					max_result_count = max_result_count * 2;
					result_buffer = realloc(result_buffer,
							max_result_count * sizeof(rstruct*));
					if (result_buffer == NULL) {
						printf("Error allocating memory");
						return NULL;
					}
				}
			}
		}
	}	// end for n

	*result_count = res_count;
	return result_buffer;
}

static rstruct** measure_multi(int* result_count,
		struct program_options* options, metric** metric_array) {

	// measure_single with more effort to measure multiple monitoring calls at once
	int res_count = 0;
	int i, j, n, m;	// loop variables
	int max = options->max_tools;
	int chunk_count = 0;

	int max_result_count = options->number * options->metrics_count;
	// allocate a buffer for all measured values
	rstruct** result_buffer = calloc(max_result_count, sizeof(rstruct*));
	if (result_buffer == NULL) {
		printf("Error allocating memory\n");
		return 0;
	}

	int seconds = 0;
	unsigned int sample_time = 1000000 * pow(options->samplingrate, -1);
	if (options->samplingrate != -1) {
		unsigned long int long_sample_time = 1000000
				* pow(options->samplingrate, -1);
		if (long_sample_time > 1000000) {
			seconds = 1;	// use sleep instead of usleep
			sample_time = (unsigned int) (long_sample_time / 1000000);
		}
	}

	start_monitor* start_func_buffer = malloc(max * sizeof(start_monitor*));
	void** argument_buffer = malloc(max * sizeof(void*));
	struct stop_monitor_struct** stop_call_buffer = malloc(
			max * sizeof(struct stop_monitor_struct*));

	int monitor_count = 0;
	int profiling_count = 0;
	int current_monitor = 0;
	int current_profile = 0;

	for (i = 0; i < GLOBAL_metric_number; ++i) {
		if (options->measure_metrics[i] == ON) {
			if (metric_array[i]->call_type_flag == CALL_TYPE_PROFILE) {

				profiling_count++;
			} else {	//use monitor call

				monitor_count++;
			}
		}
	}

	int* monitor_metric_list = malloc(monitor_count * sizeof(int));
	int* profiling_metric_list = malloc(profiling_count * sizeof(int));

	// check if all alloced mem is ready to use
	if (profiling_metric_list == NULL || monitor_metric_list == NULL
			|| start_func_buffer == NULL || start_func_buffer == NULL) {
		printf("Error allocating memory\n");
		return 0;
	}
	current_monitor = 0;
	current_profile = 0;

	for (i = 0; i < GLOBAL_metric_number; ++i) {
		if (options->measure_metrics[i] == ON) {
			if (metric_array[i]->call_type_flag == CALL_TYPE_PROFILE) {
				profiling_metric_list[current_profile] = i;
				current_profile++;
			} else {	//use monitor call
				monitor_metric_list[current_monitor] = i;
				current_monitor++;
			}
		}
	}

	for (n = 0; n < options->number; ++n) {
		current_monitor = 0;
		current_profile = 0;
		while (current_monitor < monitor_count
				|| current_profile < profiling_count) {
			// as long as there are not all metrics measured
			chunk_count = 0;
			// initialize the chunk of metrics to monitor in next run
			for (m = 0; m < max - 1; ++m) {
				while (current_monitor < monitor_count) {
					if (options->measure_metrics[monitor_metric_list[current_monitor]]
							== ON) {	// only if it was not already measured

						//skip equal call functions
						int already_called = 0;
						for (i = 0; i < m; ++i) {
							if (metric_array[monitor_metric_list[current_monitor]]->monitor_function
									== start_func_buffer[i])
								already_called++;
						}
						if (already_called == 0) {
							start_func_buffer[m] =
									metric_array[monitor_metric_list[current_monitor]]->monitor_function;
							argument_buffer[m] =
									metric_array[monitor_metric_list[current_monitor]]->arguments;

							chunk_count++;
							current_monitor++;
							break;	// end loop because a metric was found
						}
					}

					current_monitor++;
				}

			}

			if (current_profile >= profiling_count) {// initialize an additional metric to monitor
				while (current_monitor < monitor_count) {
					if (options->measure_metrics[monitor_metric_list[current_monitor]]
							== ON) {	// only if it was not already measured

						//skip equal call functions
						int already_called = 0;
						for (i = 0; i < m; ++i) {
							if (metric_array[monitor_metric_list[current_monitor]]->monitor_function
									== start_func_buffer[i])
								already_called++;
						}
						if (already_called == 0) {
							start_func_buffer[m] =
									metric_array[monitor_metric_list[current_monitor]]->monitor_function;
							argument_buffer[m] =
									metric_array[monitor_metric_list[current_monitor]]->arguments;

							chunk_count++;
							current_monitor++;
							break;	// end loop because a metric was found
						}
					}

					current_monitor++;
				}
			}

			//setup profiling call
			while (current_profile < profiling_count
					&& options->measure_metrics[profiling_metric_list[current_profile]]
							!= ON) {
				// go to next metric with profiling call if current is already measured
				current_profile++;
			}

			if (current_profile < profiling_count) { // with profiling tool at the end
				res_count++;
				// MEASURE NOW: performance is critically here
				for (m = 0; m < chunk_count; ++m) { // start monitoring tools
					stop_call_buffer[m] = start_func_buffer[m](options->program,
							argument_buffer[m]);
				}
				// execute a profiling command
				result_buffer[res_count - 1] =
						metric_array[profiling_metric_list[current_profile]]->profiling_function(
								options->program,
								metric_array[profiling_metric_list[current_profile]]->arguments);
				// no sampling with profiling
				for (m = 0; m < chunk_count; ++m) { // stop monitoring tools
					result_buffer[res_count + m] =
							stop_call_buffer[m]->stop_call(
									stop_call_buffer[m]->args,
									argument_buffer[m]);
				}
				//END MEASUREMENT
				for (m = 0; m < chunk_count; ++m) {	//free the buffer content (call_structs)
					free(stop_call_buffer[m]);
				}
				current_profile++;
				if (result_buffer[res_count - 1] != NULL) {
					// mark the metrics form the profiler al being measured
					for (i = 0; i < result_buffer[res_count - 1]->resultcount;
							++i) {
						options->measure_metrics[result_buffer[res_count - 1]->id_list[i]] =
						ALREADY_MEASURED;
						//set the metric to be already measured
					}
				}

			} else {					// without profiling tool
				// MEASURE NOW: performance is critically here
				for (m = 0; m < chunk_count; ++m) {	//start monitoring tools
					stop_call_buffer[m] = start_func_buffer[m](options->program,
							argument_buffer[m]);
				}
				if (options->samplingrate != -1) {
					if (seconds) {
						sleep(sample_time);
					} else {
						usleep(sample_time);
					}
				} else {	// no sampling
					system(options->program);	// execute the program
				}
				for (m = 0; m < chunk_count; ++m) {	//stop monitoring tools
					result_buffer[res_count + m] =
							stop_call_buffer[m]->stop_call(
									stop_call_buffer[m]->args,
									argument_buffer[m]);
				}
				//END MEASUREMENT
				for (m = 0; m < chunk_count; ++m) {	//free the buffer content (call_structs)
					free(stop_call_buffer[m]);
				}
			}
			// mark the metrics already measured
			for (j = 0; j < chunk_count; ++j) {
				if (result_buffer[res_count + j] != NULL) {
					for (i = 0; i < result_buffer[res_count + j]->resultcount;
							++i) {
						options->measure_metrics[result_buffer[res_count + j]->id_list[i]] =
						ALREADY_MEASURED;
					}
				}
			}
			res_count = res_count + chunk_count;

		}
		// all metrics are measured
		for (i = 0; i < GLOBAL_metric_number; i++) {
			if (options->measure_metrics[i] == ALREADY_MEASURED) {
				options->measure_metrics[i] = ON;// set it to measure it again in the next measurement phase
			}
		}
		if (options->samplingrate != -1) {
			--n;	// do not stop until other process finished
			int exit = 0;
#ifdef USE_MPI
			if (GLOBAL_mpi_info.execute_command) {
#endif
			if (waitpid(options->pid, NULL, WNOHANG) != 0) {//check if process not exist anymore
				exit = 1;
				// exit sampling
#ifdef USE_MPI
				GLOBAL_mpi_info.buffer = '1';
				for (int t = 1; t < GLOBAL_mpi_info.execute_command; ++t) {
					if (GLOBAL_mpi_info.other_ranks[t]) {
						MPI_Send(&GLOBAL_mpi_info.buffer, 1, MPI_CHAR, t, 0, MPI_COMM_WORLD);
						// send stop message if rank t is active
					}
				}
#endif
			}
#ifdef USE_MPI
		} else {
			MPI_Test(&GLOBAL_mpi_info.recv_stop_message, &exit, MPI_STATUS_IGNORE);
		}
#endif
			if (exit) {
				options->number = n;
			} else {
				if (res_count == max_result_count) {
					// check if result_buffer is full
					max_result_count = max_result_count * 2;
					result_buffer = realloc(result_buffer,
							max_result_count * sizeof(rstruct*));
					if (result_buffer == NULL) {
						printf("Error allocating memory");
						return NULL;
					}
				}
			}
		}
	}	//end for n

	free(monitor_metric_list);
	free(profiling_metric_list);
	free(start_func_buffer);
	free(stop_call_buffer);
	free(argument_buffer);

	*result_count = res_count;
	return result_buffer;
}

static double* write_additional_output(struct program_options* options,
		metric** metric_array, rstruct** result_buffer, int res_count) {

	int i, j;	//loop variables
	double* results = calloc(GLOBAL_metric_number, sizeof(double));
	struct list_elem** all_results_list = calloc(GLOBAL_metric_number,
			sizeof(struct list_elem*));
	int* counter = calloc(GLOBAL_metric_number, sizeof(int));

	if (results == NULL || counter == NULL) {
		printf("Error allocating memory\n");
		return 0;
	}
	// shares code with write_output
	char* filename;
	char* hostname = malloc(MAX_HOSTNAME_LENGTH * sizeof(char));
	if (gethostname(hostname, MAX_HOSTNAME_LENGTH) == -1) {
		printf("Error getting hostname\n");
	}

	filename = calloc(strlen(options->output) + MAX_HOSTNAME_LENGTH + 6 + 16,
			sizeof(char)); // 1 for "/" 4 for ".out" 1 for string terminal 16 for "_allMeasurements"
	if (filename == NULL) {
		printf("Error allocating memory\n");
		return 0;
	}
	filename[0] = 0; //empty_string
	if (options->dir == ON) {
		DIR* dir = opendir(options->output);
		if (!dir) { // create dir
			mkdir(options->output, 0777);
		}
		strcat(filename, options->output);
		strcat(filename, "//");
		strcat(filename, hostname);
		strcat(filename, "_allMeasurements");
		strcat(filename, ".out");
	} else {
		memcpy(filename, options->output,
				sizeof(char) * strlen(options->output));
		strcat(filename, "_allMeasurements");
	}

	FILE* fd = fopen(filename, "w");
	if (fd == 0) {
		printf("Error open File\n");
		return 0;
	}
	free(hostname);
	free(filename);

	struct list_elem* element = NULL;
	struct list_elem* next_element = NULL;

	for (i = 0; i < res_count; ++i) {
		rstruct* result = result_buffer[i];
		if (result != NULL) {
			for (j = 0; j < result->resultcount; ++j) {
				results[result->id_list[j]] += result->result_list[j]; // add value
				counter[result->id_list[j]]++;
				element = malloc(sizeof(struct list_elem));
				if (element == NULL) {
					printf("Error allocating memory\n");
					return 0;
				}
				element->value = result->result_list[j];

				// add to correct list
				element->next = all_results_list[result->id_list[j]];
				all_results_list[result->id_list[j]] = element;
			}
			free(result->result_list);
			free(result->id_list);
			free(result);
		}
	}
	free(result_buffer);

	for (i = 0; i < GLOBAL_metric_number; ++i) {
		if (metric_array[i] != NULL)
		// else metric was not measured and will be skipped
		{
			if (counter[i] != 0) {
				results[i] = results[i] / counter[i]; // compute mean
			}

			double deviation = 0.0f;

			// compute standard deviation
			element = all_results_list[i];
			while (element != NULL) {
				deviation = deviation
						+ (element->value - results[i])
								* (element->value - results[i]);
				element = element->next;
			}
			if (counter[i] != 0) {
				deviation = deviation / (counter[i] - 1);
			}
			deviation = sqrt(deviation);

			double percent = 0.0f;
			if (results[i] != 0.0f) {
				percent = (deviation / results[i]) * 100;
			}

			fprintf(fd, "%s: mean: %f relative standard deviation: %3.2f%%",
					metric_array[i]->name, results[i], percent);

			// write all data
			element = all_results_list[i];
			while (element != NULL) {
				fprintf(fd, DATA_SEPERATOR_S"%f", element->value);

				next_element = element->next;
				free(element);
				element = next_element;
			}
		} else {
			// to be shure there can be no mem-leak:
			element = all_results_list[i];
			while (element != NULL) {
				next_element = element->next;
				free(element);
				element = next_element;
			}
		}
		fprintf(fd, "\n");

	} // end for i
	fprintf(fd, "Measured with program version %s\n", VERSION);
	free(all_results_list);
	free(counter);
	fclose(fd);

	return results;
}

static double* get_normal_output(rstruct** result_buffer, int res_count) {
	int i, j;	//loop variable
	double* results = calloc(GLOBAL_metric_number, sizeof(double));
	int* counter = calloc(GLOBAL_metric_number, sizeof(int));

	if (results == NULL || counter == NULL) {
		printf("Error allocating memory\n");
		return 0;
	}
	for (i = 0; i < res_count; ++i) {
		rstruct* result = result_buffer[i];
		if (result != NULL) {
			for (j = 0; j < result->resultcount; ++j) {
				results[result->id_list[j]] += result->result_list[j]; // add value
				counter[result->id_list[j]]++;
			}
			free(result->result_list);
			free(result->id_list);
			free(result);
		}
	}
	free(result_buffer);

	for (i = 0; i < GLOBAL_metric_number; ++i) {
		if (counter[i] != 0) {
			results[i] = results[i] / counter[i]; // compute mean
		}
	}
	free(counter);

	return results;
}

static double* measure(struct program_options* options, metric** metric_array) {

	int res_count = 0;
	rstruct** result_buffer;
	if (options->max_tools == 1) {
		result_buffer = measure_single(&res_count, options, metric_array);
	} else {
		result_buffer = measure_multi(&res_count, options, metric_array);
	}

	if (options->additional == ON) {
		return write_additional_output(options, metric_array, result_buffer,
				res_count);
	} else {
		return get_normal_output(result_buffer, res_count);
	}

	return 0; // will never reach it anyway

}

void write_output(double* measurements, struct program_options* options,
		metric** metric_aray) {
	char* filename;
	char* hostname = malloc(MAX_HOSTNAME_LENGTH * sizeof(char));
	if (gethostname(hostname, MAX_HOSTNAME_LENGTH) == -1) {
		printf("Error getting hostname\n");
	}

	if (options->dir == ON) {
		DIR* dir = opendir(options->output);
		if (!dir) { // create dir
			mkdir(options->output, 0777);
		}
		filename = malloc(strlen(options->output) + MAX_HOSTNAME_LENGTH + 6); // 1 for "/" 4 for ".out" 1 for string terminal
		filename[0] = 0;
		strcat(filename, options->output);
		strcat(filename, "//");
		strcat(filename, hostname);
		strcat(filename, ".out");
	} else {
		filename = options->output;
	}
	char* timestamp = malloc(MAX_TIMESTAMP_SIZE * sizeof(char));

	struct tm* tm_info;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tm_info = localtime(&tv.tv_sec);
	strftime(timestamp, MAX_TIMESTAMP_SIZE, TIMESTAMP_FORMAT, tm_info);

	FILE* fd = fopen(filename, "w");
	if (fd == 0) {
		printf("Error open File\n");
		return;
	}

	fprintf(fd, "Metrics for %s measurement finished at %s on %s \n",
			options->program, timestamp, hostname);

	for (int i = 0; i < GLOBAL_metric_number; ++i) {
		if (metric_aray[i] != 0) {
			fprintf(fd, "%f"DATA_SEPERATOR_S"%s\n", measurements[i],
					metric_aray[i]->name);
		} else {
			fprintf(fd, "\n");
		}

	}
	fprintf(fd, "Measured with program version %s Do not delete empty lines\n",
	VERSION);

	fclose(fd);
	if (options->dir == ON) {
		free(filename);
	}
	free(timestamp);
	free(hostname);
}

#ifdef USE_MPI
// Initialization for mpi
int init_mpi(int argc, char** argv) {
	MPI_Init(&argc, &argv);

	int rank;
	int numtasks;

// Welchen rang habe ich?
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
// wie viele Tasks gibt es?
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

	char* my_hostname = malloc(MAX_HOSTNAME_LENGTH * sizeof(char));
	if (gethostname(my_hostname, MAX_HOSTNAME_LENGTH) == -1) {
		printf("Error getting hostname\n");
	}

	char* hostname_buffer = malloc(MAX_HOSTNAME_LENGTH * numtasks * sizeof(char));

	MPI_Allgather(my_hostname, MAX_HOSTNAME_LENGTH, MPI_CHAR, hostname_buffer, MAX_HOSTNAME_LENGTH, MPI_CHAR,
			MPI_COMM_WORLD);

	//printf("Hello from rank %i of %i on %s\n", rank, numtasks, my_hostname);

	if (rank != 0) {

		for (int i = 0; i < rank; ++i) {
			// check if there is another smaller rank on same host
			if (strcmp(my_hostname, &hostname_buffer[i * MAX_HOSTNAME_LENGTH]) == 0) {
				free(my_hostname);
				free(hostname_buffer);
				GLOBAL_mpi_info.execute_command = 0;
				MPI_Gather(&GLOBAL_mpi_info.execute_command, 1, MPI_INT, NULL, 1, MPI_INT, 0, MPI_COMM_WORLD);
				//printf("Rank %i does nothing\n",rank);
				MPI_Finalize();
				exit(0);
				return 0;
			}
		}
		// if passed above it is the smallest rank on a host and will perform sampling
		free(my_hostname);
		free(hostname_buffer);
		GLOBAL_mpi_info.execute_command = 0;
		int status = 1;
		MPI_Gather(&status, 1, MPI_INT, NULL, 1, MPI_INT, 0, MPI_COMM_WORLD);

		MPI_Irecv(&GLOBAL_mpi_info.buffer, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &GLOBAL_mpi_info.recv_stop_message);
		// Receive stop message when command was execute
		//printf("Rank %i will wait for stop-message\n",rank);
		return 1;

	} else {
		free(my_hostname);
		free(hostname_buffer);
		GLOBAL_mpi_info.execute_command = numtasks;
		GLOBAL_mpi_info.other_ranks = malloc(numtasks * sizeof(int));
		MPI_Gather(&GLOBAL_mpi_info.execute_command, 1, MPI_INT, GLOBAL_mpi_info.other_ranks, 1, MPI_INT, 0,
				MPI_COMM_WORLD);
		// only master will execute command
		//printf("Rank %i is master and will execute the commend\n",rank);
		return 1;
	}
	printf("ERROR THIS SHOULD NEVER HAPPEN\n");
	MPI_Finalize();
	return 0; //should never reach this
}
#endif

// init the options with default params
static struct program_options* init_options(metric** metric_array) {
	struct program_options* options = malloc(sizeof(struct program_options));
	options->max_tools = GLOBAL_metric_number;
	options->number = 1;
	options->metrics_count = 0;
	options->dir = OFF;
	options->additional = OFF;
	options->samplingrate = -1;
#ifdef USE_MPI
	options->samplingrate = 1;
	// mpi only works with sampling
	options->dir = ON;
	// enforce -d for MPI
#endif

	// is initialized in parse params;
	options->program = NULL;
	options->output = NULL;

	options->measure_metrics = malloc(sizeof(int) * GLOBAL_metric_number);
	if (options->measure_metrics == NULL) {
		return 0;
	}
	for (int i = 0; i < GLOBAL_metric_number; ++i) {
		options->measure_metrics[i] = metric_array[i]->standard;
		if (options->measure_metrics[i] == ON) {
			options->metrics_count++;
		}
	}

	return options;
}

int main(int argc, char** argv) {

#ifdef USE_MPI
	if (init_mpi(argc,argv) ==0)
	{
		return 0;
		// end all unnecessary processes
		//MPI_Finalize() was called in init_mpi
	}
#endif

	metric** metric_array = init_metrics();
	if (metric_array == NULL) {
		printf("Error allocating memory");
#ifdef USE_MPI
		MPI_Finalize();
#endif
		return -1;
	}
// do a sanity check:
	if (check_metrics(metric_array) != 0) {
#ifdef USE_MPI
		MPI_Finalize();
#endif
		return -1;
	}
// initialize options:
	struct program_options* options = init_options(metric_array);
	if (options == NULL) {
		printf("Error allocating memory");
#ifdef USE_MPI
		MPI_Finalize();
#endif
		return -1;
	}

	parse_args(argc, argv, options, metric_array);
	if (options->program == NULL) {
#ifdef USE_MPI
		MPI_Finalize();
#endif
		return -1;	// error message was given in parse_args
	}

	pid_t pid = -1;
	if (options->samplingrate != -1) {
#ifdef USE_MPI
		if (GLOBAL_mpi_info.execute_command) {
#endif
		pid = fork();
		if (pid == -1) {
			printf("Error in fork the process to be sampled\n");
#ifdef USE_MPI
			MPI_Finalize();
#endif
			return -1;
		}
#ifdef USE_MPI
	}
#endif
		set_up_sampling(pid, options, metric_array);
	}

	if (pid != 0) {	//child will not call this
		//if there is no child pid will be -1
		free_unused_Metrics(options, metric_array);
		double* measurements = measure(options, metric_array);
		if (measurements == NULL) {
#ifdef USE_MPI
			MPI_Finalize();
#endif
			return -1;	// error message was given in measure
		}

		write_output(measurements, options, metric_array);
#ifdef USE_MPI
		if(GLOBAL_mpi_info.execute_command)
		{
#endif
		printf("done\n");
#ifdef USE_MPI
	}
#endif

		free(measurements);
		free_all_Metrics(metric_array);
		free(options->measure_metrics);
		free(options);

	} else {
		// child will instead execute the program given
		system(options->program);
		// child will free all other mem in set_up_sampling
		free(options);
		exit(0);
	}

#ifdef USE_MPI
	if(GLOBAL_mpi_info.execute_command)
	{
		free(GLOBAL_mpi_info.other_ranks);
	}
	MPI_Finalize();
#endif
	return 0;
}
