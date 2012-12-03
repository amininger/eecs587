/*************************************************************************
 *
 *  file:  epmem_data.h
 *
 * =======================================================================
 *  
 *  
 *  
 *  
 *  
 *  
 * =======================================================================
 */

#ifndef EPMEM_DATA_PARALLEL_H
#define EPMEM_DATA_PARALLEL_H

#include "kernel.h" 
#include "soar_module.h"
#include "episodic_memory.h"

typedef struct epmem_data_parallel_struct {
    // Memory pools
    
    // memory_pool		  epmem_wmes_pool;
    //memory_pool		  epmem_info_pool;
    
    memory_pool		  epmem_literal_pool;
    memory_pool		  epmem_pedge_pool;
    memory_pool		  epmem_uedge_pool;
    memory_pool		  epmem_interval_pool;
    //epmem_sym** ?
    
    soar_module::sqlite_database *epmem_db;
    //everything under here...

    epmem_rit_state epmem_rit_state_graph[2]; //??

} epmem_data_parallel;

void init_epmem_db(epmem_data_parallel* epData);
void initialize(epmem_data_parallel* epData);

#endif
