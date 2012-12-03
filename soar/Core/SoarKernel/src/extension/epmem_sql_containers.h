#ifndef EPMEM_SQL_CONTAINERS_H_
#define EPMEM_SQL_CONTAINERS_H_

//////////////////////////////////////////////////////////
// EpMem Statements
//////////////////////////////////////////////////////////

#include "soar_module.h"
#include "soar_db.h"

// E587: AM: Statement container for the master processors (keeps the working memory graph)
class epmem_master_statement_container: public soar_module::sqlite_statement_container
{
public:
	soar_module::sqlite_statement *add_time;

	// node_unique management
	soar_module::sqlite_statement *add_node_unique;
	soar_module::sqlite_statement *find_node_unique;
	soar_module::sqlite_statement *get_node_unique;

	// edge_unique management
	soar_module::sqlite_statement *add_edge_unique;
	soar_module::sqlite_statement *find_edge_unique;
	soar_module::sqlite_statement *find_edge_unique_shared;
	soar_module::sqlite_statement *get_edge_unique;
	
	// get descriptions by id
	soar_module::sqlite_statement *get_node_desc;
	soar_module::sqlite_statement *get_edge_desc;

	// LTI and promotion management
	soar_module::sqlite_statement *promote_id;
	soar_module::sqlite_statement *find_lti;
	soar_module::sqlite_statement *find_lti_promotion_time;

	// Hash get/set
	soar_module::sqlite_statement *hash_get;
	soar_module::sqlite_statement *hash_add;
	
	// Variables get/set
	soar_module::sqlite_statement *var_get;
	soar_module::sqlite_statement *var_set;

	// Database management
	soar_module::sqlite_statement *begin;
	soar_module::sqlite_statement *commit;
	soar_module::sqlite_statement *rollback;

	// Go between episodes
	soar_module::sqlite_statement *valid_episode;
	soar_module::sqlite_statement *next_episode;
	soar_module::sqlite_statement *prev_episode;


	//
	epmem_master_statement_container( agent *new_agent );
};

class epmem_common_statement_container: public soar_module::sqlite_statement_container
{
	public:
//		soar_module::sqlite_statement *begin;
//		soar_module::sqlite_statement *commit;
//		soar_module::sqlite_statement *rollback;

		// E587: AM: Get/Set Variables
		soar_module::sqlite_statement *var_get;
		soar_module::sqlite_statement *var_set;

		// E587: AM: Get/Add Symbol Hash
		//soar_module::sqlite_statement *hash_get;
		//soar_module::sqlite_statement *hash_add;

		// RIT stuff
		soar_module::sqlite_statement *rit_add_left;
		soar_module::sqlite_statement *rit_truncate_left;
		soar_module::sqlite_statement *rit_add_right;
		soar_module::sqlite_statement *rit_truncate_right;


		epmem_common_statement_container( soar_module::sqlite_database *new_db );
};

class epmem_graph_statement_container: public soar_module::sqlite_statement_container
{
	public:
		soar_module::sqlite_statement *add_time;
		//

		soar_module::sqlite_statement *add_node_now;
		soar_module::sqlite_statement *delete_node_now;
		soar_module::sqlite_statement *add_node_point;
		soar_module::sqlite_statement *add_node_range;

		soar_module::sqlite_statement *add_node_unique;
		soar_module::sqlite_statement *add_node_unique_with_id;
		soar_module::sqlite_statement *find_node_unique;
		soar_module::sqlite_statement *get_node_unique;

		//

		soar_module::sqlite_statement *add_edge_now;
		soar_module::sqlite_statement *delete_edge_now;
		soar_module::sqlite_statement *add_edge_point;
		soar_module::sqlite_statement *add_edge_range;

		soar_module::sqlite_statement *add_edge_unique;
		soar_module::sqlite_statement *add_edge_unique_with_id;
		soar_module::sqlite_statement *find_edge_unique;
		soar_module::sqlite_statement *find_edge_unique_shared;
		soar_module::sqlite_statement *get_edge_unique;


		soar_module::sqlite_statement *get_node_ids;
		soar_module::sqlite_statement *get_edge_ids;

		//

		soar_module::sqlite_statement *update_edge_unique_last;
		
	// LTI and promotion management
	//soar_module::sqlite_statement *promote_id;
	//soar_module::sqlite_statement *find_lti;
	//soar_module::sqlite_statement *find_lti_promotion_time;
		//

		soar_module::sqlite_statement_pool *pool_find_edge_queries[2][2];
		soar_module::sqlite_statement_pool *pool_find_interval_queries[2][2][3];
		soar_module::sqlite_statement_pool *pool_find_lti_queries[2][3];
		soar_module::sqlite_statement_pool *pool_dummy;

		//
		
		epmem_graph_statement_container( soar_module::sqlite_database *new_db );
};

#endif //EPMEM_SQL_CONTAINERS_H_