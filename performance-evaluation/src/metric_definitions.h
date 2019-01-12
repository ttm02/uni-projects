#ifndef METRICS_DEFINITIONS_H_INCLUDED
#define METRICS_DEFINITIONS_H_INCLUDED

// define all calls needed to initialize all metrics here

//initialize all metrics for a device class:
metric** initCPU_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID);
metric** initDisk_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID);
metric** initNet_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID);
metric** init_cache_Metric(unsigned int* result_count, GKeyFile* config_file, int start_ID);
//metric** init_cacheTEST_Metric(unsigned int* result_count, GKeyFile* config_file, int start_ID);
metric** initNet_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID);
metric** initNEW_metrics(unsigned int* result_count, GKeyFile* config_file, int start_ID);

// clear additional info for metric for a device class if needed:
void clearCPU_metrics();
void clearDisk_metrics();
void clearNet_metrics();


#endif
