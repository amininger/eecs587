

#ifndef EPMEM_WORKER_H_
#define EPMEM_WORKER_H_

class epmem_worker;

#include "../soar_module.h"
#include "../episodic_memory.h"
#include "epmem_episode_diff.h"
#include "epmem_sql_containers.h"
#include "epmem_episode.h"
#include "epmem_query.h"

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
	epmem_graph_statement_container *epmem_stmts_graph;

	// Constructor
	epmem_worker();

	// Initialize the worker (create database, etc.)
	void initialize(epmem_param_container* epmem_params);

	// Add a new episode to the collection of episodes the worker currently has
	void add_epmem_episode_diff(epmem_episode_diff* episode);

	// Removes the oldest episode the agent has and returned a epmem_episode_diff diff structure for use elsewhere
	epmem_episode_diff* remove_oldest_episode();

	// Fills the given vector with all node_unique id's active at the given episode time
	void get_nodes_at_episode(epmem_time_id time, std::vector<epmem_node_id>* node_ids);

	// Fills the given vector with all edge_unique id's active at the given episode time
	void get_edges_at_episode(epmem_time_id time, std::vector<epmem_node_id>* edge_ids);

	epmem_episode* get_episode(epmem_time_id);

    query_rsp_data* epmem_perform_query(epmem_query* query);

	// Closes the worker and removes the database
	void close();

	// E587 JK
	void epmem_process_query(int64_t* data);

private:
	soar_module::sqlite_database *epmem_db;

	epmem_common_statement_container *epmem_stmts_common;

	std::map<long, int> node_now_start_times;

	std::map<long, int> edge_now_start_times;

  epmem_rit_state epmem_rit_state_graph[2];

	epmem_time_id last_removal;


	void add_new_nodes(epmem_episode_diff* episode);

	void add_new_edges(epmem_episode_diff* episode);

	void remove_old_nodes(epmem_episode_diff* episode);

	void remove_old_edges(epmem_episode_diff* episode);

	int64_t epmem_rit_fork_node( int64_t lower, int64_t upper, int64_t *step_return, epmem_rit_state *rit_state );

	void epmem_rit_clear_left_right();

	void epmem_rit_add_left(epmem_time_id min, epmem_time_id max );

	void epmem_rit_add_right( epmem_time_id id );

	void epmem_rit_prep_left_right( int64_t lower, int64_t upper, epmem_rit_state *rit_state );

	void epmem_rit_insert_interval( int64_t lower, int64_t upper, epmem_node_id id, epmem_rit_state *rit_state );
	
	bool epmem_get_variable( epmem_variable_key variable_id, int64_t *variable_value );
	
	void epmem_set_variable( epmem_variable_key variable_id, int64_t variable_value );

	// E587 JK
	//
	//memory_pool		  epmem_literal_pool;
	//memory_pool		  epmem_pedge_pool;
	//memory_pool		  epmem_uedge_pool;
	//memory_pool		  epmem_interval_pool;

	//void initialize_epmem_pools();
	
	tc_number tc;
	tc_number get_new_tc_number(){tc++;return tc;}
	
    void epmem_print_retrieval_state(epmem_wme_literal_map& literals, epmem_triple_pedge_map pedge_caches[], epmem_triple_uedge_map uedge_caches[]);

    epmem_literal* epmem_build_dnf(epmem_query* query, epmem_cue_wme* cue_wme, epmem_wme_literal_map& literal_cache, epmem_literal_set& leaf_literals, epmem_symbol_int_map& symbol_num_incoming, epmem_literal_deque& gm_ordering, int query_type, epmem_cue_symbol_set& visiting, epmem_cue_symbol_set& currents, epmem_cue_wme_set& cue_wmes);

    bool epmem_register_pedges(epmem_node_id parent, epmem_literal* literal, epmem_pedge_pq& pedge_pq, epmem_time_id after, epmem_triple_pedge_map pedge_caches[], epmem_triple_uedge_map uedge_caches[]);

    bool epmem_satisfy_literal(epmem_literal* literal, epmem_node_id parent, epmem_node_id child, double& current_score, long int& current_cardinality, epmem_symbol_node_pair_int_map& symbol_node_count, epmem_triple_uedge_map uedge_caches[], epmem_symbol_int_map& symbol_num_incoming);

    bool epmem_unsatisfy_literal(epmem_literal* literal, epmem_node_id parent, epmem_node_id child, double& current_score, long int& current_cardinality, epmem_symbol_node_pair_int_map& symbol_node_count) ;

    bool epmem_graph_match(epmem_literal_deque::iterator& dnf_iter, epmem_literal_deque::iterator& iter_end, epmem_literal_node_pair_map& bindings, epmem_node_symbol_map bound_nodes[], int depth );

};

#endif // EPMEM_WORKER_H_
