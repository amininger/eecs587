#include "epmem_worker.h"


epmem_worker::epmem_worker(){
	 epmem_db = new soar_module::sqlite_database();
	 epmem_stmts_common = NIL;
	 epmem_stmts_graph = NIL;
}

void epmem_worker::initialize(epmem_param_container* epmem_params, agent* my_agent){
	{
		// E587: AM:
		if ( epmem_db->get_status() != soar_module::disconnected )
		{
			return;
		}


		const char *db_path = ":memory:";

		// attempt connection
		epmem_db->connect( db_path );

		if ( epmem_db->get_status() == soar_module::problem )
		{
			char buf[256];
			//SNPRINTF( buf, 254, "DB ERROR: %s", epmem_db->get_errmsg() );
		}
		else
		{
			epmem_time_id time_max;
			soar_module::sqlite_statement *temp_q = NULL;
			soar_module::sqlite_statement *temp_q2 = NULL;

			// apply performance options
			{
				// page_size
				{
					switch ( epmem_params->page_size->get_value() )
					{
						case ( epmem_param_container::page_1k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 1024" );
							break;

						case ( epmem_param_container::page_2k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 2048" );
							break;

						case ( epmem_param_container::page_4k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 4096" );
							break;

						case ( epmem_param_container::page_8k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 8192" );
							break;

						case ( epmem_param_container::page_16k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 16384" );
							break;

						case ( epmem_param_container::page_32k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 32768" );
							break;

						case ( epmem_param_container::page_64k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 65536" );
							break;
					}

					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;
				}

				// cache_size
				{
					std::string cache_sql( "PRAGMA cache_size = " );
					char* str = epmem_params->cache_size->get_string();
					cache_sql.append( str );
					free(str);
					str = NULL;

					temp_q = new soar_module::sqlite_statement( epmem_db, cache_sql.c_str() );

					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;
				}

				// optimization
				if ( epmem_params->opt->get_value() == epmem_param_container::opt_speed )
				{
					// synchronous - don't wait for writes to complete (can corrupt the db in case unexpected crash during transaction)
					temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA synchronous = OFF" );
					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;

					// journal_mode - no atomic transactions (can result in database corruption if crash during transaction)
					temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA journal_mode = OFF" );
					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;

					// locking_mode - no one else can view the database after our first write
					temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA locking_mode = EXCLUSIVE" );
					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;
				}
			}

			// setup common structures/queries
			epmem_stmts_common = new epmem_common_statement_container( my_agent );
			epmem_stmts_common->structure();
			epmem_stmts_common->prepare();

			// setup graph structures/queries
			epmem_stmts_graph = new epmem_graph_statement_container( my_agent );
			epmem_stmts_graph->structure();
			epmem_stmts_graph->prepare();
		}
	}
}

void epmem_worker::add_new_episode(new_episode* episode, agent* my_agent){
	// Add the time to the database
	epmem_stmts_graph->add_time->bind_int(1, episode->time);
	epmem_stmts_graph->add_time->execute(soar_module::op_reinit);

	// Add new nodes/edges
	add_new_nodes(episode);
	add_new_edges(episode);
	
	// Remove old nodes/edges
	remove_old_nodes(episode, my_agent);
	remove_old_edges(episode, my_agent);
}

void epmem_worker::add_new_nodes(new_episode* episode){
	// Add all the new nodes to the database
	for(int i = 0; i < episode->num_added_nodes; i++){
		epmem_node_unique* node = &episode->added_nodes[i];
		
		// If the id is not in the node_unique table then add it
		epmem_stmts_graph->get_node_unique->bind_int( 1, node->id );
		if ( epmem_stmts_graph->get_node_unique->execute() != soar_module::row ) {
					// insert (child_id,parent_id,attr,value)
					epmem_stmts_graph->add_node_unique_with_id->bind_int( 1, node->id);
					epmem_stmts_graph->add_node_unique_with_id->bind_int( 2, node->parent_id );
					epmem_stmts_graph->add_node_unique_with_id->bind_int( 3, node->attribute );
					epmem_stmts_graph->add_node_unique_with_id->bind_int( 4, node->value );
					epmem_stmts_graph->add_node_unique_with_id->execute( soar_module::op_reinit );
		}
		epmem_stmts_graph->get_node_unique->reinitialize();

		// Add the node to the node_now table (id, start)
		epmem_stmts_graph->add_node_now->bind_int( 1, node->id );
		epmem_stmts_graph->add_node_now->bind_int( 2, episode->time );
		epmem_stmts_graph->add_node_now->execute( soar_module::op_reinit );

		node_now_start_times.insert(std::pair<long, int>(node->id, episode->time));
	}
}

void epmem_worker::add_new_edges(new_episode* episode){
	// Add all the new edges to the database
	for(int i = 0; i < episode->num_added_edges; i++){
		epmem_edge_unique* edge = &episode->added_edges[i];
		
		// If the id is not in the node_unique table then add it
		epmem_stmts_graph->get_edge_unique->bind_int( 1, edge->id );
		if ( epmem_stmts_graph->get_edge_unique->execute() != soar_module::row ) {
					// insert (child_id,q0,w,q1,last)
					epmem_stmts_graph->add_edge_unique_with_id->bind_int( 1, edge->id);
					epmem_stmts_graph->add_edge_unique_with_id->bind_int( 2, edge->parent_id );
					epmem_stmts_graph->add_edge_unique_with_id->bind_int( 3, edge->attribute );
					epmem_stmts_graph->add_edge_unique_with_id->bind_int( 4, edge->child_id );
					epmem_stmts_graph->add_edge_unique_with_id->bind_int( 5, LLONG_MAX );
					epmem_stmts_graph->add_edge_unique_with_id->execute( soar_module::op_reinit );
		}
		epmem_stmts_graph->get_edge_unique->reinitialize();

		// Add the node to the node_now table (id, start)
		epmem_stmts_graph->add_edge_now->bind_int( 1, edge->id );
		epmem_stmts_graph->add_edge_now->bind_int( 2, episode->time );
		epmem_stmts_graph->add_edge_now->execute( soar_module::op_reinit );

		edge_now_start_times.insert(std::pair<long, int>(edge->id, episode->time));

		// updates the last times for all edge_unique's
		epmem_stmts_graph->update_edge_unique_last->bind_int( 1, LLONG_MAX );
		epmem_stmts_graph->update_edge_unique_last->bind_int( 2, edge->id );
		epmem_stmts_graph->update_edge_unique_last->execute( soar_module::op_reinit );
	}
}

void epmem_worker::remove_old_nodes(new_episode* episode, agent* my_agent){
	// Remove nodes from the current episode
	for(int i = 0; i < episode->num_removed_nodes; i++){
		epmem_node_unique* node = &episode->removed_nodes[i];

		// Remove node_now row
		epmem_stmts_graph->delete_node_now->bind_int( 1, node->id );
		epmem_stmts_graph->delete_node_now->execute( soar_module::op_reinit );
		
		// Get the episode number the node started at
		int range_start;
		std::map<long, int>::iterator node_start_itr = node_now_start_times.find(node->id);
		if(node_start_itr != node_now_start_times.end()){
			range_start = node_start_itr->second;
			node_now_start_times.erase(node_start_itr);
		} else {
			range_start = 1;
		}

		// Check to see if going to add the node as a point or range
		int range_end = episode->time - 1;
		if(range_start == range_end){
			// insert node_point
			epmem_stmts_graph->add_node_point->bind_int( 1, node->id );
			epmem_stmts_graph->add_node_point->bind_int( 2, range_start );
			epmem_stmts_graph->add_node_point->execute( soar_module::op_reinit );
		} else {
			// insert edge_range
			epmem_rit_insert_interval( my_agent, range_start, range_end, node->id, &( my_agent->epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ] ) );
		}
	}
}

void epmem_worker::remove_old_edges(new_episode* episode, agent* my_agent){
		// Remove episodes from the current episode
	for(int i = 0; i < episode->num_removed_edges; i++){
		epmem_edge_unique* edge = &episode->removed_edges[i];

		// Remove node_now row
		epmem_stmts_graph->delete_edge_now->bind_int( 1, edge->id );
		epmem_stmts_graph->delete_edge_now->execute( soar_module::op_reinit );
		
		// Get the episode number the node started at
		int range_start;
		std::map<long, int>::iterator edge_start_itr = edge_now_start_times.find(edge->id);
		if(edge_start_itr != edge_now_start_times.end()){
			range_start = edge_start_itr->second;
			edge_now_start_times.erase(edge_start_itr);
		} else {
			range_start = 1;
		}

		// Check to see if going to add the node as a point or range
		int range_end = episode->time - 1;
		if(range_start == range_end){
			// insert edge_point
			epmem_stmts_graph->add_edge_point->bind_int( 1, edge->id );
			epmem_stmts_graph->add_edge_point->bind_int( 2, range_start );
			epmem_stmts_graph->add_edge_point->execute( soar_module::op_reinit );
		} else {
			// insert edge_range
			epmem_rit_insert_interval( my_agent, range_start, range_end, edge->id, &( my_agent->epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ] ) );
		}
		}
}

new_episode* epmem_worker::remove_oldest_episode(){
	return new new_episode();
}
