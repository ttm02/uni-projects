#ifndef RTLIB_ARRAY_H_
#define RTLIB_ARRAY_H_

#include "rtlib_typedefs.h"

void log_memory_allocation(void *base_ptr, long size);

long get_array_size_from_address(void *base_ptr, size_t elem_size);

void distribute_shared_array_from_master(void *comm_info, size_t buffer_type_size, int DIM,
                                         size_t array_elem_size);

void free_distributed_shared_array_from_master(void *comm_info, size_t buffer_type_size);

void cache_shared_array_line(void *comm_info, size_t buffer_type_size, long idx);

void invlaidate_shared_array_cache(void *comm_info, size_t buffer_type_size);

void invlaidate_shared_array_cache_release_mem(void *comm_info, size_t buffer_type_size);

void store_to_shared_array_line(void *comm_info, size_t buffer_type_size, long idx,
                                void *store_address);

long get_own_lower(void *comm_info, size_t buffer_type_size);

long get_own_upper(void *comm_info, size_t buffer_type_size);

bool is_array_row_own(void *comm_info, size_t buffer_type_size, long idx);

void bcast_array_from_master(void *comm_info, size_t buffer_type_size, int DIM,
                             size_t array_elem_size);

void free_bcasted_array_from_master(void *comm_info, size_t buffer_type_size);

void store_to_bcasted_array_line(void *comm_info, size_t buffer_type_size, long idx,
                                 void *store_address);

void init_master_based_array_info(void *comm_info, size_t buffer_type_size, int DIM,
                                  size_t array_elem_size);

void free_master_based_array(void *comm_info, size_t buffer_type_size);

void store_to_master_based_array_line(void *comm_info, size_t buffer_type_size, long idx,
                                      void *store_address);

void load_from_master_based_array_line(void *comm_info, size_t buffer_type_size, long idx);

void sync_master_based_array(void *comm_info, size_t buffer_type_size);

#endif /* RTLIB_ARRAY_H_ */
