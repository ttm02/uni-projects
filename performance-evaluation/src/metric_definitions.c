#include "measure.h"
#include <string.h>

extern int GLOBAL_metric_number;

struct metric_list_elem {
	unsigned int metric_count;
	metric** metric_list;
	struct metric_list_elem* next;
};

void fetch_metric_definition(init_function init_call, int* current_position, struct metric_list_elem** current_elem,
		GKeyFile* config_file) {
	struct metric_list_elem* new_elem = malloc(sizeof(struct metric_list_elem));
	new_elem->next = NULL;
	new_elem->metric_list = init_call(&new_elem->metric_count, config_file, *current_position);
	*current_position += new_elem->metric_count;
	if (*current_elem != NULL) {
		current_elem[0]->next = new_elem;
	}
	*current_elem = new_elem;
}

// initialize all metrics
metric** init_metrics() {

	// no need to chance initialization in next 4 lines:
	GKeyFile* config_file = g_key_file_new();
	g_key_file_load_from_file(config_file, CONFIG_FILE_NAME, G_KEY_FILE_NONE, NULL);
	struct metric_list_elem* current_elem = NULL;
	struct metric_list_elem* head_elem = NULL;
	int current_position = 0;

	//all metric definitions are initialized here:
	fetch_metric_definition(&initCPU_metrics, &current_position, &head_elem, config_file);
	current_elem = head_elem; // only do this after first call
	fetch_metric_definition(&initDisk_metrics, &current_position, &current_elem, config_file);
	fetch_metric_definition(&initNet_metrics, &current_position, &current_elem, config_file);
	fetch_metric_definition(&init_cache_Metric, &current_position, &current_elem, config_file);
	//fetch_metric_definition(&init_cacheTEST_Metric, &current_position, &current_elem, config_file);
	//Enter your new metrics here:
	fetch_metric_definition(&initNEW_metrics, &current_position, &current_elem, config_file);

	// no need to change this:
	GLOBAL_metric_number = current_position;
	metric** metric_array = malloc(sizeof(metric*) * GLOBAL_metric_number);
	if (metric_array != NULL) { // if memory was allocated:
		current_position = 0;
		struct metric_list_elem* next_elem = NULL;
		while (head_elem != NULL) {
			next_elem = head_elem->next;
			memcpy(&metric_array[current_position], head_elem->metric_list, head_elem->metric_count * sizeof(metric*));
			current_position += head_elem->metric_count;
			free(head_elem->metric_list);
			free(head_elem);
			head_elem = next_elem;
		}

	}

	g_key_file_free(config_file);
	return metric_array;
}

// free all metrics
// no need to change this
void free_all_Metrics(metric** metric_array) {

	for (int i = 0; i < GLOBAL_metric_number; i++) {
		if (metric_array[i] != NULL) {
			if (metric_array[i]->arguments != NULL) {
				metric_array[i]->clear_function(metric_array[i]->arguments);
			}
			free(metric_array[i]->name);
			free(metric_array[i]->keyword);
			free(metric_array[i]->help);
			free(metric_array[i]);
		}
	}
	free(metric_array);
}

// do a sanity check for all metrics
// no need to change this
int check_metrics(metric** metric_array) {
	int sane = 0;

	for (int i = 0; i < GLOBAL_metric_number; i++) {

		if (metric_array[i] == NULL) {
			printf("Error, no metric definition for metric %i \n", i);
			sane++;
		} else {

			//sanity check:
			if (metric_array[i]->id != i) {
				printf("Error, id of metric %i is wrong (get ID=%i) \n", i, metric_array[i]->id);
				sane++;
			}
			if (metric_array[i]->name == NULL) {
				printf("Error, no name for metric %i \n", i);
				sane++;
			}
			if (metric_array[i]->keyword == NULL) {
				printf("Error, no keyword for metric %i \n", i);
				sane++;
			}
			if (metric_array[i]->help == NULL) {
				printf("Error, no help for metric %i \n", i);
				sane++;
			}

			if (metric_array[i]->call_type_flag != CALL_TYPE_MONITOR
					&& metric_array[i]->call_type_flag != CALL_TYPE_PROFILE
					&& metric_array[i]->call_type_flag != CALL_TYPE_BOTH) {
				printf("Error, bad call_type_flag for metric %i\n", i);
				sane++;
			}
			if (metric_array[i]->call_type_flag == CALL_TYPE_MONITOR && metric_array[i]->monitor_function == NULL) {
				printf("Error, bad call function for metric %i\n", i);
				sane++;
			}
			if ((metric_array[i]->call_type_flag == CALL_TYPE_PROFILE) && metric_array[i]->profiling_function == NULL) {
				printf("Error, bad call function for metric %i\n", i);
				sane++;
			}
			if (metric_array[i]->call_type_flag == CALL_TYPE_BOTH && metric_array[i]->profiling_function == NULL
					&& metric_array[i]->monitor_function == NULL) {
				printf("Error, bad call function for metric %i\n", i);
				sane++;
			}

			if (metric_array[i]->margin < 0 || metric_array[i]->margin > 100) {
				printf("Error, bad margin for metric %i\n", i);
				sane++;
			}

		}
	}
	if (sane != 0) {
		printf("There were %i Errors in the metric definitions\n", sane);
	}
	return sane;
}

