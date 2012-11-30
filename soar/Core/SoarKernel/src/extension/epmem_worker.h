

#ifndef EPMEM_WORKER_H_
#define EPMEM_WORKER_H_

class epmem_worker;

#include "../soar_module.h"
#include "../episodic_memory.h"
#include "new_episode.h"

class epmem_worker{
public:
	soar_module::sqlite_database *epmem_db;
	epmem_common_statement_container *epmem_stmts_common;
	epmem_graph_statement_container *epmem_stmts_graph;

	epmem_worker();

	void initialize(epmem_param_container* epmem_params, agent* my_agent);

	void add_new_episode(new_episode* episode);

	new_episode* remove_oldest_episode();


};




#endif // EPMEM_WORKER_H_
