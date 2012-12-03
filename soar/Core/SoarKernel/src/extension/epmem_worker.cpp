#include "epmem_worker.h"


epmem_worker::epmem_worker(){
	 epmem_db = new soar_module::sqlite_database();
	 epmem_stmts_common = NIL;
	 epmem_stmts_graph = NIL;
}

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// RIT Functions (epmem::rit)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_rit_fork_node
 * Author		: Nate Derbinsky
 * Notes		: Implements the forkNode function of RIT
 **************************************************************************/
int64_t epmem_worker::epmem_rit_fork_node( int64_t lower, int64_t upper, int64_t *step_return, epmem_rit_state *rit_state )
{
	// never called
	/*if ( !bounds_offset )
	  {
	  int64_t offset = rit_state->offset.stat->get_value();

	  lower = ( lower - offset );
	  upper = ( upper - offset );
	  }*/

	// descend the tree down to the fork node
	int64_t node = EPMEM_RIT_ROOT;
	if ( upper < EPMEM_RIT_ROOT )
	{
		node = rit_state->leftroot.stat->get_value();
	}
	else if ( lower > EPMEM_RIT_ROOT )
	{
		node = rit_state->rightroot.stat->get_value();
	}

	int64_t step;
	for ( step = ( ( ( node >= 0 )?( node ):( -1 * node ) ) / 2 ); step >= 1; step /= 2 )
	{
		if ( upper < node )
		{
			node -= step;
		}
		else if ( node < lower )
		{
			node += step;
		}
		else
		{
			break;
		}
	}

	// never used
	// if ( step_return != NULL )
	{
		(*step_return) = step;
	}

	return node;
}

/***************************************************************************
 * Function     : epmem_rit_clear_left_right
 * Author		: Nate Derbinsky
 * Notes		: Clears the left/right relations populated during prep
 **************************************************************************/
void epmem_worker::epmem_rit_clear_left_right()
{
	// E587: AM:
	epmem_stmts_common->rit_truncate_left->execute( soar_module::op_reinit );
	epmem_stmts_common->rit_truncate_right->execute( soar_module::op_reinit );
}

/***************************************************************************
 * Function     : epmem_rit_add_left
 * Author		: Nate Derbinsky
 * Notes		: Adds a range to the left relation
 **************************************************************************/
void epmem_worker::epmem_rit_add_left(epmem_time_id min, epmem_time_id max )
{
	// E587: AM:
	epmem_stmts_common->rit_add_left->bind_int( 1, min );
	epmem_stmts_common->rit_add_left->bind_int( 2, max );
	epmem_stmts_common->rit_add_left->execute( soar_module::op_reinit );
}

/***************************************************************************
 * Function     : epmem_rit_add_right
 * Author		: Nate Derbinsky
 * Notes		: Adds a node to the to the right relation
 **************************************************************************/
void epmem_worker::epmem_rit_add_right( epmem_time_id id )
{
	// E587: AM:
	epmem_stmts_common->rit_add_right->bind_int( 1, id );
	epmem_stmts_common->rit_add_right->execute( soar_module::op_reinit );
}

/***************************************************************************
 * Function     : epmem_rit_prep_left_right
 * Author		: Nate Derbinsky
 * Notes		: Implements the computational components of the RIT
 * 				  query algorithm
 **************************************************************************/
void epmem_worker::epmem_rit_prep_left_right( int64_t lower, int64_t upper, epmem_rit_state *rit_state )
{

	int64_t offset = rit_state->offset.stat->get_value();
	int64_t node, step;
	int64_t left_node, left_step;
	int64_t right_node, right_step;

	lower = ( lower - offset );
	upper = ( upper - offset );

	// auto add good range
	epmem_rit_add_left( lower, upper );

	// go to fork
	node = EPMEM_RIT_ROOT;
	step = 0;
	if ( ( lower > node ) || (upper < node ) )
	{
		if ( lower > node )
		{
			node = rit_state->rightroot.stat->get_value();
			epmem_rit_add_left( EPMEM_RIT_ROOT, EPMEM_RIT_ROOT );
		}
		else
		{
			node = rit_state->leftroot.stat->get_value();
			epmem_rit_add_right( EPMEM_RIT_ROOT );
		}

		for ( step = ( ( ( node >= 0 )?( node ):( -1 * node ) ) / 2 ); step >= 1; step /= 2 )
		{
			if ( lower > node )
			{
				epmem_rit_add_left( node, node );
				node += step;
			}
			else if ( upper < node )
			{
				epmem_rit_add_right( node );
				node -= step;
			}
			else
			{
				break;
			}
		}
	}

	// go left
	left_node = node - step;
	for ( left_step = ( step / 2 ); left_step >= 1; left_step /= 2 )
	{
		if ( lower == left_node )
		{
			break;
		}
		else if ( lower > left_node )
		{
			epmem_rit_add_left( left_node, left_node );
			left_node += left_step;
		}
		else
		{
			left_node -= left_step;
		}
	}

	// go right
	right_node = node + step;
	for ( right_step = ( step / 2 ); right_step >= 1; right_step /= 2 )
	{
		if ( upper == right_node )
		{
			break;
		}
		else if ( upper < right_node )
		{
			epmem_rit_add_right( right_node );
			right_node -= right_step;
		}
		else
		{
			right_node += right_step;
		}
	}
}

/***************************************************************************
 * Function     : epmem_rit_insert_interval
 * Author		: Nate Derbinsky
 * Notes		: Inserts an interval in the RIT
 **************************************************************************/
void epmem_worker::epmem_rit_insert_interval( int64_t lower, int64_t upper, epmem_node_id id, epmem_rit_state *rit_state )
{
	// initialize offset
	int64_t offset = rit_state->offset.stat->get_value();
	if ( offset == EPMEM_RIT_OFFSET_INIT )
	{
		offset = lower;

		// update database
		epmem_set_variable( rit_state->offset.var_key, offset );

		// update stat
		rit_state->offset.stat->set_value( offset );
	}

	// get node
	int64_t node;
	{
		int64_t left_root = rit_state->leftroot.stat->get_value();
		int64_t right_root = rit_state->rightroot.stat->get_value();
		int64_t min_step = rit_state->minstep.stat->get_value();

		// shift interval
		int64_t l = ( lower - offset );
		int64_t u = ( upper - offset );

		// update left_root
		if ( ( u < EPMEM_RIT_ROOT ) && ( l <= ( 2 * left_root ) ) )
		{
			left_root = static_cast<int64_t>( pow( -2.0, floor( log( static_cast<double>( -l ) ) / EPMEM_LN_2 ) ) );

			// update database
			epmem_set_variable( rit_state->leftroot.var_key, left_root );

			// update stat
			rit_state->leftroot.stat->set_value( left_root );
		}

		// update right_root
		if ( ( l > EPMEM_RIT_ROOT ) && ( u >= ( 2 * right_root ) ) )
		{
			right_root = static_cast<int64_t>( pow( 2.0, floor( log( static_cast<double>( u ) ) / EPMEM_LN_2 ) ) );

			// update database
			epmem_set_variable( rit_state->rightroot.var_key, right_root );

			// update stat
			rit_state->rightroot.stat->set_value( right_root );
		}

		// update min_step
		int64_t step;
		node = epmem_rit_fork_node( l, u, &step, rit_state );

		if ( ( node != EPMEM_RIT_ROOT ) && ( step < min_step ) )
		{
			min_step = step;

			// update database
			epmem_set_variable( rit_state->minstep.var_key, min_step );

			// update stat
			rit_state->minstep.stat->set_value( min_step );
		}
	}

	// perform insert
	// ( node, start, end, id )
	rit_state->add_query->bind_int( 1, node );
	rit_state->add_query->bind_int( 2, lower );
	rit_state->add_query->bind_int( 3, upper );
	rit_state->add_query->bind_int( 4, id );
	rit_state->add_query->execute( soar_module::op_reinit );
}

new_episode* epmem_worker::remove_oldest_episode(){
	return new new_episode();
}

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Variable Functions (epmem::var)
//
// Variables are key-value pairs stored in the database
// that are necessary to maintain a store between
// multiple runs of Soar.
//
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_get_variable
 * Author		: Nate Derbinsky
 * Notes		: Gets an EpMem variable from the database
 **************************************************************************/
bool epmem_worker::epmem_get_variable( epmem_variable_key variable_id, int64_t *variable_value )
{
	soar_module::exec_result status;
	// E587: AM:
	soar_module::sqlite_statement *var_get = epmem_stmts_common->var_get;

	var_get->bind_int( 1, variable_id );
	status = var_get->execute();

	if ( status == soar_module::row )
	{
		(*variable_value) = var_get->column_int( 0 );
	}

	var_get->reinitialize();

	return ( status == soar_module::row );
}

/***************************************************************************
 * Function     : epmem_set_variable
 * Author		: Nate Derbinsky
 * Notes		: Sets an EpMem variable in the database
 **************************************************************************/
void epmem_worker::epmem_set_variable( epmem_variable_key variable_id, int64_t variable_value )
{
	// E587: AM:
	soar_module::sqlite_statement *var_set = epmem_stmts_common->var_set;

	var_set->bind_int( 1, variable_id );
	var_set->bind_int( 2, variable_value );

	var_set->execute( soar_module::op_reinit );
}

void epmem_worker::initialize(epmem_param_container* epmem_params){
	{

		

		
	/////////////////////////////
	// connect to rit state
	/////////////////////////////

	//// graph
	//my_agent->epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].timer = ncb_node_rit;
	//my_agent->epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].timer = ncb_edge_rit;

			// rit-offset-1
	soar_module::integer_stat* rit_offset_1 = new soar_module::integer_stat( "rit-offset-1", 0, new soar_module::predicate<int64_t>() );

	// rit-left-root-1
	soar_module::integer_stat* rit_left_root_1 = new soar_module::integer_stat( "rit-left-root-1", 0, new soar_module::predicate<int64_t>() );

	// rit-right-root-1
	soar_module::integer_stat* rit_right_root_1 = new soar_module::integer_stat( "rit-right-root-1", 0, new soar_module::predicate<int64_t>() );

	// rit-min-step-1
	soar_module::integer_stat* rit_min_step_1 = new soar_module::integer_stat( "rit-min-step-1", 0, new soar_module::predicate<int64_t>() );

	// rit-offset-2
	soar_module::integer_stat* rit_offset_2 = new soar_module::integer_stat( "rit-offset-2", 0, new soar_module::predicate<int64_t>() );

	// rit-left-root-2
	soar_module::integer_stat* rit_left_root_2 = new soar_module::integer_stat( "rit-left-root-2", 0, new soar_module::predicate<int64_t>() );

	// rit-right-root-2
	soar_module::integer_stat* rit_right_root_2 = new soar_module::integer_stat( "rit-right-root-2", 0, new soar_module::predicate<int64_t>() );

	// rit-min-step-2
	soar_module::integer_stat* rit_min_step_2 = new soar_module::integer_stat( "rit-min-step-2", 0, new soar_module::predicate<int64_t>() );

	// graph
	epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].offset.stat = rit_offset_1;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].offset.var_key = var_rit_offset_1;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].leftroot.stat = rit_left_root_1;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].leftroot.var_key = var_rit_leftroot_1;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].rightroot.stat =rit_right_root_1;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].rightroot.var_key = var_rit_rightroot_1;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].minstep.stat = rit_min_step_1;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].minstep.var_key = var_rit_minstep_1;

	epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].offset.stat = rit_offset_2;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].offset.var_key = var_rit_offset_2;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].leftroot.stat = rit_left_root_2;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].leftroot.var_key = var_rit_leftroot_2;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].rightroot.stat = rit_right_root_2;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].rightroot.var_key = var_rit_rightroot_2;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].minstep.stat = rit_min_step_2;
	epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].minstep.var_key = var_rit_minstep_2;

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
			epmem_stmts_common = new epmem_common_statement_container( epmem_db );
			epmem_stmts_common->structure();
			epmem_stmts_common->prepare();

			// setup graph structures/queries
			epmem_stmts_graph = new epmem_graph_statement_container( epmem_db );
			epmem_stmts_graph->structure();
			epmem_stmts_graph->prepare();

			

			// initialize rit state
			for ( int i=EPMEM_RIT_STATE_NODE; i<=EPMEM_RIT_STATE_EDGE; i++ )
			{
				//epmem_rit_state_graph[ i ].offset.stat->set_value( EPMEM_RIT_OFFSET_INIT );
				//epmem_rit_state_graph[ i ].leftroot.stat->set_value( 0 );
				//epmem_rit_state_graph[ i ].rightroot.stat->set_value( 1 );
				//epmem_rit_state_graph[ i ].minstep.stat->set_value( LONG_MAX );
			}

			// E587: AM:
			epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ].add_query = epmem_stmts_graph->add_node_range;
			epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ].add_query = epmem_stmts_graph->add_edge_range;

			////

			// get/set RIT variables
			{
				int64_t var_val = NIL;

				for ( int i=EPMEM_RIT_STATE_NODE; i<=EPMEM_RIT_STATE_EDGE; i++ )
				{
					// offset
					if ( epmem_get_variable( epmem_rit_state_graph[ i ].offset.var_key, &var_val ) )
					{
						epmem_rit_state_graph[ i ].offset.stat->set_value( var_val );
					}
					else
					{
						epmem_set_variable( epmem_rit_state_graph[ i ].offset.var_key, epmem_rit_state_graph[ i ].offset.stat->get_value() );
					}

					// leftroot
					if ( epmem_get_variable( epmem_rit_state_graph[ i ].leftroot.var_key, &var_val ) )
					{
						epmem_rit_state_graph[ i ].leftroot.stat->set_value( var_val );
					}
					else
					{
						epmem_set_variable( epmem_rit_state_graph[ i ].leftroot.var_key, epmem_rit_state_graph[ i ].leftroot.stat->get_value() );
					}

					// rightroot
					if ( epmem_get_variable( epmem_rit_state_graph[ i ].rightroot.var_key, &var_val ) )
					{
						epmem_rit_state_graph[ i ].rightroot.stat->set_value( var_val );
					}
					else
					{
						epmem_set_variable( epmem_rit_state_graph[ i ].rightroot.var_key, epmem_rit_state_graph[ i ].rightroot.stat->get_value() );
					}

					// minstep
					if ( epmem_get_variable( epmem_rit_state_graph[ i ].minstep.var_key, &var_val ) )
					{
						epmem_rit_state_graph[ i ].minstep.stat->set_value( var_val );
					}
					else
					{
						epmem_set_variable( epmem_rit_state_graph[ i ].minstep.var_key, epmem_rit_state_graph[ i ].minstep.stat->get_value() );
					}
				}
			}
		}
	}
}

void epmem_worker::add_new_episode(new_episode* episode){
	// Add the time to the database
	epmem_stmts_graph->add_time->bind_int(1, episode->time);
	epmem_stmts_graph->add_time->execute(soar_module::op_reinit);

	// Add new nodes/edges
	add_new_nodes(episode);
	add_new_edges(episode);
	
	// Remove old nodes/edges
	remove_old_nodes(episode);
	remove_old_edges(episode);
}

void epmem_worker::close(){
		// de-allocate statement pools
		{
			int j, k, m;

			for ( j=EPMEM_RIT_STATE_NODE; j<=EPMEM_RIT_STATE_EDGE; j++ )
			{
				for ( k=0; k<=1; k++ )
				{
					// E587: AM:
					delete epmem_stmts_graph->pool_find_edge_queries[ j ][ k ];
				}
			}

			for ( j=EPMEM_RIT_STATE_NODE; j<=EPMEM_RIT_STATE_EDGE; j++ )
			{
				for ( k=EPMEM_RANGE_START; k<=EPMEM_RANGE_END; k++ )
				{
					for( m=EPMEM_RANGE_EP; m<=EPMEM_RANGE_POINT; m++ )
					{
						// E587: AM:
						delete epmem_stmts_graph->pool_find_interval_queries[ j ][ k ][ m ];
					}
				}
			}

			for ( k=EPMEM_RANGE_START; k<=EPMEM_RANGE_END; k++ )
			{
				for( m=EPMEM_RANGE_EP; m<=EPMEM_RANGE_POINT; m++ )
				{
					delete epmem_stmts_graph->pool_find_lti_queries[ k ][ m ];
				}
			}
			delete epmem_stmts_graph->pool_dummy;
		}


		delete epmem_stmts_common;
		delete epmem_stmts_graph;

		if(epmem_db->get_status() == soar_module::connected){
			epmem_db->disconnect();
		}
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

void epmem_worker::remove_old_nodes(new_episode* episode){
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
			epmem_rit_insert_interval( range_start, range_end, node->id, &( epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ] ) );
		}
	}
}

void epmem_worker::remove_old_edges(new_episode* episode){
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
			epmem_rit_insert_interval( range_start, range_end, edge->id, &( epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ] ) );
		}
	}
}


void epmem_worker::get_nodes_at_episode(epmem_time_id time, std::vector<epmem_node_id>* node_ids){
	epmem_rit_prep_left_right(time, time, &epmem_rit_state_graph[EPMEM_RIT_STATE_NODE]);

	epmem_stmts_graph->get_node_ids->bind_int(1, time);
	epmem_stmts_graph->get_node_ids->bind_int(2, time);
	epmem_stmts_graph->get_node_ids->bind_int(3, time);
	epmem_stmts_graph->get_node_ids->bind_int(4, time);

	while(epmem_stmts_graph->get_node_ids->execute() == soar_module::row){
		node_ids->push_back(epmem_stmts_graph->get_node_ids->column_int(0));
	}
	epmem_stmts_graph->get_node_ids->reinitialize();

	epmem_rit_clear_left_right();
}

void epmem_worker::get_edges_at_episode(epmem_time_id time, std::vector<epmem_node_id>* edge_ids){
	epmem_rit_prep_left_right(time, time, &epmem_rit_state_graph[EPMEM_RIT_STATE_EDGE]);

	epmem_stmts_graph->get_edge_ids->bind_int(1, time);
	epmem_stmts_graph->get_edge_ids->bind_int(2, time);
	epmem_stmts_graph->get_edge_ids->bind_int(3, time);
	epmem_stmts_graph->get_edge_ids->bind_int(4, time);

	while(epmem_stmts_graph->get_edge_ids->execute() == soar_module::row){
		edge_ids->push_back(epmem_stmts_graph->get_edge_ids->column_int(0));
	}
	epmem_stmts_graph->get_edge_ids->reinitialize();

	epmem_rit_clear_left_right();
}