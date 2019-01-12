#ifndef DATATYPES_H_INCLUDED
#define DATATYPES_H_INCLUDED

// definition of used datatypes:

struct result_struct {
	int resultcount;
	int* id_list;
	double* result_list;
};
typedef struct result_struct rstruct;

// define function pointer types
typedef rstruct* (*profile)(char*,void*); // function for profiling mode
typedef rstruct* (*stop_monitor)(void*,void*); // function called to stop monitoring
// first arg is given from monitor_ struct second form metric_definition
// else void* refers to args given from metric_struct
typedef struct stop_monitor_struct* (*start_monitor)(char*,void*); // function pointer for monitor mode
typedef void (*clear)(void*); // function pointer for clear a metrics arg*

struct stop_monitor_struct {
	stop_monitor stop_call;
	void* args;
};

struct metric_struct {
	int id; /* id of metric   */
	int standard; /* default on or off                            */
	int call_type_flag; /* type of the calls                          */
	start_monitor monitor_function; // function pointer for monitor mode
	profile profiling_function; // function for profiling mode
	double maximum; // maximum value of metric -1 means there is no maximum given
	int margin; /* margin in percent. so 100= everything will be threated as full usage       */
	int positive; /* high value is good?            */
	// do NOT initialize the string with literals !
	// allocate them with malloc as they will get freed
	char* keyword; /* keywords to call metric                   */
	char* name;/* name of metric*/
	char* help;/* help text of metric*/
	void* arguments;/* arguments given to all functions (except of init of course) progam will never call free(arguments)*/
	clear clear_function;// function to clear the arguments. It will only be called if arguments != NULL so else you can provide NULL here

};
typedef struct metric_struct metric;

typedef metric** (*init_function)(unsigned int*,GKeyFile*,int); // function for metric_initialization

#endif
