

#ifndef EPMEM_WORKER_H_
#define EPMEM_WORKER_H_

class epmem_worker;

#include "../soar_module.h"
#include "../episodic_memory.h"
#include "new_episode.h"
#include "epmem_sql_containers.h"

#define EPMEM_RIT_ROOT								0
#define EPMEM_RIT_OFFSET_INIT						-1

#define EPMEM_RIT_STATE_NODE						0
#define EPMEM_RIT_STATE_EDGE						1


// keeping state for multiple RIT's
typedef struct epmem_rit_state_param_struct
{
	soar_module::integer_stat *stat;
	epmem_variable_key var_key;
} epmem_rit_state_param;

typedef struct epmem_rit_state_struct
{
	epmem_rit_state_param offset;
	epmem_rit_state_param leftroot;
	epmem_rit_state_param rightroot;
	epmem_rit_state_param minstep;

	soar_module::timer *timer;
	soar_module::sqlite_statement *add_query;	
} epmem_rit_state;

class epmem_worker{
public:
	soar_module::sqlite_database *epmem_db;
	epmem_common_statement_container *epmem_stmts_common;
	epmem_graph_statement_container *epmem_stmts_graph;

	epmem_worker();

	void initialize(epmem_param_container* epmem_params);

	void add_new_episode(new_episode* episode);

	new_episode* remove_oldest_episode();

	void prep_rit(epmem_time_id time1, epmem_time_id time2, int structure_type);

	void clear_rit();

private:
	void add_new_nodes(new_episode* episode);

	void add_new_edges(new_episode* episode);

	void remove_old_nodes(new_episode* episode);

	void remove_old_edges(new_episode* episode);

	std::map<long, int> node_now_start_times;

	std::map<long, int> edge_now_start_times;

  epmem_rit_state epmem_rit_state_graph[2];

	int64_t epmem_rit_fork_node( int64_t lower, int64_t upper, int64_t *step_return, epmem_rit_state *rit_state );

	void epmem_rit_clear_left_right();

	void epmem_rit_add_left(epmem_time_id min, epmem_time_id max );

	void epmem_rit_add_right( epmem_time_id id );

	void epmem_rit_prep_left_right( int64_t lower, int64_t upper, epmem_rit_state *rit_state );

	void epmem_rit_insert_interval( int64_t lower, int64_t upper, epmem_node_id id, epmem_rit_state *rit_state );
	
	bool epmem_get_variable( epmem_variable_key variable_id, int64_t *variable_value );
	
	void epmem_set_variable( epmem_variable_key variable_id, int64_t variable_value );

};

#endif // EPMEM_WORKER_H_
