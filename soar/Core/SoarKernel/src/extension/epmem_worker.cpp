#include "epmem_worker.h"
#include "epmem_manager.h"
//#include "../instantiations.h"
//#include "../symtab.h"
epmem_worker::epmem_worker(){
	epmem_db = new soar_module::sqlite_database();
	epmem_stmts_common = NIL;
	 epmem_stmts_graph = NIL;
	 last_removal = 0;
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

void epmem_worker::initialize(int page_size, bool optimization, char *str, bool from_thread){
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
					switch (page_size)//E587 JK
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
					// E587 JK
                    //char* str = epmem_params->cache_size->get_string();
					cache_sql.append( str );
					if (!from_thread)
						free(str);
					str = NULL;

					temp_q = new soar_module::sqlite_statement( epmem_db, cache_sql.c_str() );

					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;
				}

				// optimization
				if (optimization)//E587 JK
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
void epmem_worker::initialize(epmem_param_container* epmem_params){
	bool optimization = (epmem_params->opt->get_value() == epmem_param_container::opt_speed );
	initialize(epmem_params->page_size->get_value(), optimization, epmem_params->cache_size->get_string(), false);
			   
}

void epmem_worker::add_epmem_episode_diff(epmem_episode_diff* episode){
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

void epmem_worker::add_new_nodes(epmem_episode_diff* episode){
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

void epmem_worker::add_new_edges(epmem_episode_diff* episode){
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

void epmem_worker::remove_old_nodes(epmem_episode_diff* episode){
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

void epmem_worker::remove_old_edges(epmem_episode_diff* episode){
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

epmem_episode_diff* epmem_worker::remove_oldest_episode(){
	soar_module::sqlite_statement get_min_time(epmem_db, "SELECT MIN(id) FROM times");
	soar_module::sqlite_statement get_max_time(epmem_db, "SELECT MAX(id) FROM times");

	if(get_min_time.execute() != soar_module::row || get_max_time.execute() != soar_module::row){
		// No episodes are stored in the database
		return NIL;
	} 

	// Min and max times in the times table
	epmem_time_id min_time = get_min_time.column_int(0);
	epmem_time_id max_time = get_max_time.column_int(0);

	if(min_time == max_time){
		// No episodes are currently stored in the database
		return NIL;
	}

	// Building up the diff structure to return
	std::vector<epmem_node_unique> nodes_to_remove;
	std::vector<epmem_node_unique> nodes_to_add;
	std::vector<epmem_edge_unique> edges_to_remove;
	std::vector<epmem_edge_unique> edges_to_add;

	// node_point 
	{
		soar_module::sqlite_statement get_node_point(epmem_db, "SELECT child_id, parent_id, attrib, value FROM node_unique WHERE child_id IN (SELECT id FROM node_point WHERE start=?)");

		// Add all edge_points that are at the removed episode
		get_node_point.bind_int(1, min_time);
		while(get_node_point.execute() == soar_module::row){
			nodes_to_add.push_back(epmem_node_unique(&get_node_point));
		}
		get_node_point.reinitialize();

		// The node_points that were present in the last episode removed will need to be 
		// marked as removed in the episode diff
		get_node_point.bind_int(1, last_removal);
		while(get_node_point.execute() == soar_module::row){
			nodes_to_remove.push_back(epmem_node_unique(&get_node_point));
		}
	}
	
	// edge_point
	{
		soar_module::sqlite_statement get_edge_point(epmem_db, "SELECT parent_id, q0, w, q1 FROM edge_unique WHERE parent_id IN (SELECT id FROM edge_point WHERE start=?)");

		// Add all edge_points that are at the removed episode
		get_edge_point.bind_int(1, min_time);
		while(get_edge_point.execute() == soar_module::row){
			edges_to_add.push_back(epmem_edge_unique(&get_edge_point));
		}
		get_edge_point.reinitialize();

		// The edge_points that were present in the last episode removed will need to be 
		// marked as removed in the episode diff
		get_edge_point.bind_int(1, last_removal);
		while(get_edge_point.execute() == soar_module::row){
			edges_to_remove.push_back(epmem_edge_unique(&get_edge_point));
		}
	}

	// node_now
	{
		// Get all node_nows that start at the removed episode
		soar_module::sqlite_statement get_node_now(epmem_db, "SELECT child_id, parent_id, attrib, value FROM node_unique WHERE child_id IN (SELECT id FROM node_now WHERE start=?)");

		get_node_now.bind_int(1, min_time);
		while(get_node_now.execute() == soar_module::row){
			nodes_to_add.push_back(epmem_node_unique(&get_node_now));
		}

		// Update the starting values of all old node_nows to that of the episode being removed
		soar_module::sqlite_statement update_node_now_start(epmem_db, "UPDATE node_now SET start=? WHERE start=?");
		update_node_now_start.bind_int(1, min_time);
		update_node_now_start.bind_int(2, last_removal);
		update_node_now_start.execute();
	}

	// edge_now
	{
		// Get all edge_nows that start at the removed episode
		soar_module::sqlite_statement get_edge_now(epmem_db, "SELECT parent_id, q0, w, q1 FROM edge_unique WHERE parent_id IN (SELECT id FROM edge_now WHERE start=?)");

		get_edge_now.bind_int(1, min_time);
		while(get_edge_now.execute() == soar_module::row){
			edges_to_add.push_back(epmem_edge_unique(&get_edge_now));
		}

		// Update the starting values of all old edge_nows to that of the episode being removed
		soar_module::sqlite_statement update_edge_now_start(epmem_db, "UPDATE edge_now SET start=? WHERE start=?");
		update_edge_now_start.bind_int(1, min_time);
		update_edge_now_start.bind_int(2, last_removal);
		update_edge_now_start.execute();
	}

	// node_range
	{
		// Add all node_ranges that start on the episode being removed
		soar_module::sqlite_statement get_node_range(epmem_db, "SELECT child_id, parent_id, attrib, value FROM node_unique WHERE child_id IN (SELECT id FROM node_range WHERE start=?)");

		get_node_range.bind_int(1, min_time);
		while(get_node_range.execute() == soar_module::row){
			nodes_to_add.push_back(epmem_node_unique(&get_node_range));
		}

		// Get a list of all ranges that will collapse to a point when the episode is removed
		std::vector<epmem_node_id> new_node_points;
		soar_module::sqlite_statement get_node_range_to_remove(epmem_db, "SELECT id FROM node_range WHERE (start=? AND end=?)");

		get_node_range_to_remove.bind_int(1, last_removal);
		get_node_range_to_remove.bind_int(2, min_time);
		while(get_node_range_to_remove.execute() == soar_module::row){
			new_node_points.push_back(get_node_range_to_remove.column_int(0));
		}

		// Delete the ranges and add node_points
		soar_module::sqlite_statement remove_node_range(epmem_db, "DELETE FROM node_range WHERE (start=? AND end=?)");
		remove_node_range.bind_int(1, last_removal);
		remove_node_range.bind_int(2, min_time);
		remove_node_range.execute();

		for(int i = 0; i < new_node_points.size(); i++){
			epmem_stmts_graph->add_node_point->bind_int(1, new_node_points[i]);
			epmem_stmts_graph->add_node_point->bind_int(2, min_time);
			epmem_stmts_graph->add_node_point->execute(soar_module::op_reinit);
		}
		
		// Update the starting values of all node_ranges with start values in the last_removal
		soar_module::sqlite_statement update_node_range_start(epmem_db, "UPDATE node_range SET start=? WHERE start=?");
		update_node_range_start.bind_int(1, min_time);
		update_node_range_start.bind_int(2, last_removal);
		update_node_range_start.execute();
	}

		// edge_range
	{
		// Add all edge_ranges that start on the episode being removed
		soar_module::sqlite_statement get_edge_range(epmem_db, "SELECT parent_id, q0, w, q1 FROM edge_unique WHERE parent_id IN (SELECT id FROM edge_range WHERE start=?)");

		get_edge_range.bind_int(1, min_time);
		while(get_edge_range.execute() == soar_module::row){
			edges_to_add.push_back(epmem_edge_unique(&get_edge_range));
		}

		// Get a list of all ranges that will collapse to a point when the episode is removed
		std::vector<epmem_node_id> new_edge_points;
		soar_module::sqlite_statement get_edge_range_to_remove(epmem_db, "SELECT id FROM edge_range WHERE (start=? AND end=?)");

		get_edge_range_to_remove.bind_int(1, last_removal);
		get_edge_range_to_remove.bind_int(2, min_time);
		while(get_edge_range_to_remove.execute() == soar_module::row){
			new_edge_points.push_back(get_edge_range_to_remove.column_int(0));
		}

		// Delete the ranges and add edge_points
		soar_module::sqlite_statement remove_edge_range(epmem_db, "DELETE FROM edge_range WHERE (start=? AND end=?)");
		remove_edge_range.bind_int(1, last_removal);
		remove_edge_range.bind_int(2, min_time);
		remove_edge_range.execute();

		for(int i = 0; i < new_edge_points.size(); i++){
			epmem_stmts_graph->add_edge_point->bind_int(1, new_edge_points[i]);
			epmem_stmts_graph->add_edge_point->bind_int(2, min_time);
			epmem_stmts_graph->add_edge_point->execute(soar_module::op_reinit);
		}
		
		// Update the starting values of all edge_ranges with start values in the last_removal
		soar_module::sqlite_statement update_edge_range_start(epmem_db, "UPDATE edge_range SET start=? WHERE start=?");
		update_edge_range_start.bind_int(1, min_time);
		update_edge_range_start.bind_int(2, last_removal);
		update_edge_range_start.execute();
	}

	epmem_episode_diff* episode = new epmem_episode_diff(min_time, nodes_to_add.size(), nodes_to_remove.size(), edges_to_add.size(), edges_to_remove.size());

	// nodes_to_add
	{
		std::copy(nodes_to_add.begin(), nodes_to_add.end(), episode->added_nodes);
	}
	
	// edges_to_add
	{
		std::copy(edges_to_add.begin(), edges_to_add.end(), episode->added_edges);
	}

	// nodes_to_remove
	{
		std::copy(nodes_to_remove.begin(), nodes_to_remove.end(), episode->removed_nodes);

		// Check to see if the node_unique still exists somewhere, if not, delete it
		soar_module::sqlite_statement remove_unused_nodes(epmem_db, "DELETE FROM node_unique WHERE child_id NOT IN (SELECT id FROM node_now UNION SELECT id FROM node_range UNION SELECT id FROM node_point)");
		remove_unused_nodes.execute();
	}

	// edges_to_remove
	{
		std::copy(edges_to_remove.begin(), edges_to_remove.begin() + episode->num_removed_edges, episode->removed_edges);

		// Check to see if the edge_unique still exists somewhere, if not, delete it
		soar_module::sqlite_statement remove_unused_edges(epmem_db, "DELETE FROM edge_unique WHERE parent_id NOT IN (SELECT id FROM edge_now UNION SELECT id FROM edge_range UNION SELECT id FROM edge_point)");
		remove_unused_edges.execute();
	}
		
	// update times table
	{
		last_removal = min_time;
		soar_module::sqlite_statement remove_time(epmem_db, "DELETE FROM times WHERE id=?");
		remove_time.bind_int(1, min_time);
		remove_time.execute();
	}

	return episode;
}

epmem_episode* epmem_worker::get_episode(epmem_time_id time){
	std::vector<epmem_node_id> node_ids;
	std::vector<epmem_node_id> edge_ids;

	get_nodes_at_episode(time, &node_ids);
	get_edges_at_episode(time, &edge_ids);

	epmem_episode* episode = new epmem_episode(time, node_ids.size(), edge_ids.size());
	std::copy(node_ids.begin(), node_ids.end(), episode->nodes);
	std::copy(edge_ids.begin(), edge_ids.end(), episode->edges);

	return episode;
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

//#ifdef E587JK
// E587 JK initializes epmem pools
/*
void epmem_worker::initialize_epmem_pools()
{
    //todo this mem_pool calls expect agent* need to write own pool handlers
    init_memory_pool(this, &epmem_literal_pool, 
		     sizeof( epmem_literal), "epmem_literals");
    init_memory_pool(this, &epmem_pedge_pool, 
		     sizeof( epmem_pedge ), "epmem_pedges");
    init_memory_pool(this, &epmem_uedge_pool, 
		     sizeof( epmem_uedge ), "epmem_uedges" );
    init_memory_pool(this, &epmem_interval_pool, 
		     sizeof( epmem_interval ), "epmem_intervals" );
}
*/
// qqqq
// E587: JK: epmem_worker should process queries

void epmem_worker::epmem_process_query(int64_t* datac)
{
    query_data* data = (query_data*) datac;
    Symbol *pos_query = &data->pos_query;
    Symbol *neg_query  = &data->neg_query;
    epmem_time_list prohibits; 
    epmem_time_id before = data->before;
    epmem_time_id after = data->after;
    double balance = data->balance;
    epmem_symbol_set currents; 
    soar_module::wme_set cue_wmes;
    
    bool do_graph_match = data->graph_match;
    bool before_time = data->before_time;
    epmem_param_container::gm_ordering_choices gm_order = data->gm_order;
    
    soar_module::symbol_triple_list meta_wmes; 
    soar_module::symbol_triple_list retrieval_wmes;
    int level=3;
    
    // a query must contain a positive cue
    if (pos_query == NULL) {
	/*TODO error
	epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_status, my_agent->epmem_sym_bad_cmd);
	*/
	return;
    }

    // before and after, if specified, must be valid relative to each other
    if (before != EPMEM_MEMID_NONE && after != EPMEM_MEMID_NONE && before <= after) {
	/*TODO error
	epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_status, my_agent->epmem_sym_bad_cmd);
	*/
	return;
    }

    if (QUERY_DEBUG >= 1) {
	std::cout << std::endl << "==========================" << std::endl << std::endl;
    }

    //my_agent->epmem_timers->query->start(); //E587 JK

    // sort probibit's
    if (!prohibits.empty()) {
	std::sort(prohibits.begin(), prohibits.end());
    }

    // epmem options
    //E587 JK
    //bool do_graph_match = (my_agent->epmem_params->graph_match->get_value() == soar_module::on);
    //epmem_param_container::gm_ordering_choices gm_order = my_agent->epmem_params->gm_ordering->get_value();

    // variables needed for cleanup
    epmem_wme_literal_map literal_cache;
    epmem_triple_pedge_map pedge_caches[2];
// E587 JK try not using mem_pools here
#ifdef USE_MEM_POOL_ALLOCATORS
    epmem_triple_uedge_map uedge_caches[2] = {
	epmem_triple_uedge_map(std::less<epmem_triple>(), soar_module::soar_memory_pool_allocator<std::pair<const epmem_triple, epmem_uedge*> >(my_agent)),
	epmem_triple_uedge_map(std::less<epmem_triple>(), soar_module::soar_memory_pool_allocator<std::pair<const epmem_triple, epmem_uedge*> >(my_agent))
    };
    epmem_interval_set interval_cleanup = epmem_interval_set(std::less<epmem_interval*>(), soar_module::soar_memory_pool_allocator<epmem_interval*>(my_agent));
#else
    epmem_triple_uedge_map uedge_caches[2] = {epmem_triple_uedge_map(), epmem_triple_uedge_map()};
    epmem_interval_set interval_cleanup = epmem_interval_set();
#endif

    // TODO additional indices

    // variables needed for building the DNF
    
    epmem_literal* root_literal = (epmem_literal*) malloc(sizeof(epmem_literal)); //E587 JK
    //allocate_with_pool(this, &(epmem_literal_pool), &root_literal);
    epmem_literal_set leaf_literals;

    // priority queues for interval walk
    epmem_pedge_pq pedge_pq;
    epmem_interval_pq interval_pq;

    // variables needed to track satisfiability
    epmem_symbol_int_map symbol_num_incoming;                 // number of literals with a certain symbol as its value
    epmem_symbol_node_pair_int_map symbol_node_count;         // number of times a symbol is matched by a node

    // various things about the current and the best episodes
    epmem_time_id best_episode = EPMEM_MEMID_NONE;
    double best_score = 0;
    bool best_graph_matched = false;
    long int best_cardinality = 0;
    epmem_literal_node_pair_map best_bindings;
    double current_score = 0;
    long int current_cardinality = 0;

    // variables needed for graphmatch
    epmem_literal_deque gm_ordering;

    if (level > 1) {
	// build the DNF graph while checking for leaf WMEs
	{
	    //my_agent->epmem_timers->query_dnf->start(); E587 JK
	    root_literal->id_sym = NULL;
	    root_literal->value_sym = pos_query;
	    root_literal->is_neg_q = EPMEM_NODE_POS;
	    root_literal->value_is_id = EPMEM_RIT_STATE_EDGE;
	    root_literal->is_leaf = false;
	    root_literal->is_current = false;
	    root_literal->w = EPMEM_NODEID_BAD;
	    root_literal->q1 = EPMEM_NODEID_ROOT;
	    root_literal->weight = 0.0;
	    new(&(root_literal->parents)) epmem_literal_set();
	    new(&(root_literal->children)) epmem_literal_set();
// E587 JK don't use mem_pool_allocators ?
#ifdef USE_MEM_POOL_ALLOCATORS
	    new(&(root_literal->matches)) epmem_node_pair_set(std::less<epmem_node_pair>(), soar_module::soar_memory_pool_allocator<epmem_node_pair>(my_agent));
#else
	    new(&(root_literal->matches)) epmem_node_pair_set();
#endif
	    new(&(root_literal->values)) epmem_node_int_map();
	    symbol_num_incoming[pos_query] = 1;
	    literal_cache[NULL] = root_literal;

	    std::set<Symbol*> visiting;
	    visiting.insert(pos_query);
	    visiting.insert(neg_query);
	    for (int query_type = EPMEM_NODE_POS; query_type <= EPMEM_NODE_NEG; query_type++) {
		Symbol* query_root = NULL;
		switch (query_type) {
		case EPMEM_NODE_POS:
		    query_root = pos_query;
		    break;
		case EPMEM_NODE_NEG:
		    query_root = neg_query;
		    break;
		}
		if (!query_root) {
		    continue;
		}
		epmem_wme_list* children = epmem_get_augs_of_id(query_root, get_new_tc_number());//my_agent)); E587 JK
		// for each first level WME, build up a DNF
		for (epmem_wme_list::iterator wme_iter = children->begin(); wme_iter != children->end(); wme_iter++) {
		    epmem_literal* child = epmem_build_dnf(*wme_iter, literal_cache, leaf_literals, symbol_num_incoming, gm_ordering, currents, query_type, visiting, cue_wmes,balance);
		    if (child) {
			// force all first level literals to have the same id symbol
			child->id_sym = pos_query;
			child->parents.insert(root_literal);
			root_literal->children.insert(child);
		    }
		}
		delete children;
	    }
	    //my_agent->epmem_timers->query_dnf->stop(); E587 JK
	}

	// calculate the highest possible score and cardinality score
	double perfect_score = 0;
	int perfect_cardinality = 0;
	for (epmem_literal_set::iterator iter = leaf_literals.begin(); iter != leaf_literals.end(); iter++) {
	    if (!(*iter)->is_neg_q) {
		perfect_score += (*iter)->weight;
		perfect_cardinality++;
	    }
	}

	// set default values for before and after
	if (before == EPMEM_MEMID_NONE) {
	    // E587 JK
	    before = before_time;//my_agent->epmem_stats->time->get_value() - 1; 
	} else {
	    before = before - 1; // since before's are strict
	}
	if (after == EPMEM_MEMID_NONE) {
	    after = EPMEM_MEMID_NONE;
	}
	epmem_time_id current_episode = before;
	epmem_time_id next_episode;

	// create dummy edges and intervals
	{
	    // insert dummy unique edge and interval end point queries for DNF root
	    // we make an SQL statement just so we don't have to do anything special at cleanup
	    epmem_triple triple = {EPMEM_NODEID_BAD, EPMEM_NODEID_BAD, EPMEM_NODEID_ROOT};
	    epmem_pedge* root_pedge = (epmem_pedge*) malloc(sizeof(epmem_pedge));
	    //allocate_with_pool(my_agent, &(my_agent->epmem_pedge_pool), &root_pedge);// E587 JK allocate in worker 
	    root_pedge->triple = triple;
	    root_pedge->value_is_id = EPMEM_RIT_STATE_EDGE;
	    root_pedge->has_noncurrent = false;
	    new(&(root_pedge->literals)) epmem_literal_set();
	    root_pedge->literals.insert(root_literal);
	    // E587: AM: XXX: Need to remove this dependency
	    // E587 JK direct access	    
//	    root_pedge->sql = my_agent->epmem_worker_p->epmem_stmts_graph->pool_dummy->request();
	    root_pedge->sql = epmem_stmts_graph->pool_dummy->request();
	    
	    root_pedge->sql->prepare();
	    root_pedge->sql->bind_int(1, LLONG_MAX);
	    root_pedge->sql->execute( soar_module::op_reinit );
	    root_pedge->time = LLONG_MAX;
	    pedge_pq.push(root_pedge);
	    pedge_caches[EPMEM_RIT_STATE_EDGE][triple] = root_pedge;

	    epmem_uedge* root_uedge = (epmem_uedge*) malloc(sizeof(epmem_uedge)); 
	    //allocate_with_pool(my_agent, &(my_agent->epmem_uedge_pool), &root_uedge); // E587 JK allocate in worker
	    root_uedge->triple = triple;
	    root_uedge->value_is_id = EPMEM_RIT_STATE_EDGE;
	    root_uedge->has_noncurrent = false;
	    root_uedge->activation_count = 0;
	    new(&(root_uedge->pedges)) epmem_pedge_set();
	    root_uedge->intervals = 1;
	    root_uedge->activated = false;
	    uedge_caches[EPMEM_RIT_STATE_EDGE][triple] = root_uedge;

	    epmem_interval* root_interval = (epmem_interval*) malloc(sizeof(epmem_interval));
	    //allocate_with_pool(my_agent, &(my_agent->epmem_interval_pool), &root_interval); // E587 JK allocate in worker
	    root_interval->uedge = root_uedge;
	    root_interval->is_end_point = true;
	    // E587: AM: XXX: Need to remove this dependency
	    // E587 JK direct access	    
	    //root_interval->sql = my_agent->epmem_worker_p->epmem_stmts_graph->pool_dummy->request();
	    root_interval->sql = epmem_stmts_graph->pool_dummy->request();
	    
	    root_interval->sql->prepare();
	    root_interval->sql->bind_int(1, before);
	    root_interval->sql->execute( soar_module::op_reinit );
	    root_interval->time = before;
	    interval_pq.push(root_interval);
	    interval_cleanup.insert(root_interval);
	}

	if (QUERY_DEBUG >= 1) {
	    epmem_print_retrieval_state(literal_cache, pedge_caches, uedge_caches);
	}

#ifdef EPMEM_EXPERIMENT
	epmem_episodes_searched = 0;
#endif

	// main loop of interval walk
	//my_agent->epmem_timers->query_walk->start();  E587 JK
	while (pedge_pq.size() && current_episode > after) {
	    epmem_time_id next_edge;
	    epmem_time_id next_interval;

	    bool changed_score = false;

	    //my_agent->epmem_timers->query_walk_edge->start(); E587 JK
	    next_edge = pedge_pq.top()->time;

	    // process all edges which were last used at this time point
	    while (pedge_pq.size() && (pedge_pq.top()->time == next_edge || pedge_pq.top()->time >= current_episode)) {
		epmem_pedge* pedge = pedge_pq.top();
		pedge_pq.pop();
		epmem_triple triple = pedge->triple;
		triple.q1 = pedge->sql->column_int(1);

		if (QUERY_DEBUG >= 1) {
		    std::cout << "	EDGE " << triple.q0 << "-" << triple.w << "-" << triple.q1 << std::endl;
		}

		// create queries for the unique edge children of this partial edge
		if (pedge->value_is_id) {
		    bool created = false;
		    for (epmem_literal_set::iterator literal_iter = pedge->literals.begin(); literal_iter != pedge->literals.end(); literal_iter++) {
			epmem_literal* literal = *literal_iter;
			for (epmem_literal_set::iterator child_iter = literal->children.begin(); child_iter != literal->children.end(); child_iter++) {
			    //created |= epmem_register_pedges(triple.q1, *child_iter, pedge_pq, after, pedge_caches, uedge_caches); TEMPTEMP TODO 
			}
		    }
		}
		// TODO what I want to do here is, if there is no children which leads to a leaf, retract everything
		// I'm not sure how to properly test for this though

		// look for uedge with triple; if none exist, create one
		// otherwise, link up the uedge with the pedge and consider score changes
		epmem_triple_uedge_map* uedge_cache = &uedge_caches[pedge->value_is_id];
		epmem_triple_uedge_map::iterator uedge_iter = uedge_cache->find(triple);
		if (uedge_iter == uedge_cache->end()) {
		    // create a uedge for this
		    epmem_uedge* uedge = (epmem_uedge*) malloc(sizeof(epmem_uedge));
		    //allocate_with_pool(my_agent, &(my_agent->epmem_uedge_pool), &uedge); // E587 JK allocate in worker
		    uedge->triple = triple;
		    uedge->value_is_id = pedge->value_is_id;
		    uedge->has_noncurrent = pedge->has_noncurrent;
		    uedge->activation_count = 0;
		    new(&(uedge->pedges)) epmem_pedge_set();
		    uedge->intervals = 0;
		    uedge->activated = false;
		    // create interval queries for this partial edge
		    bool created = false;
		    int64_t edge_id = pedge->sql->column_int(0);
		    epmem_time_id promo_time = EPMEM_MEMID_NONE;
		    bool is_lti = (pedge->value_is_id && pedge->triple.q1 != EPMEM_NODEID_BAD && pedge->triple.q1 != EPMEM_NODEID_ROOT);
		    /* E587 JK ignoring LTI 
		    if (is_lti) {
			// find the promotion time of the LTI
			// E587: AM:
			my_agent->epmem_stmts_master->find_lti_promotion_time->bind_int(1, triple.q1);
			my_agent->epmem_stmts_master->find_lti_promotion_time->execute();
			promo_time = my_agent->epmem_stmts_master->find_lti_promotion_time->column_int(0);
			my_agent->epmem_stmts_master->find_lti_promotion_time->reinitialize();
			
		    }
		    */
		    for (int interval_type = EPMEM_RANGE_EP; interval_type <= EPMEM_RANGE_POINT; interval_type++) {
			for (int point_type = EPMEM_RANGE_START; point_type <= EPMEM_RANGE_END; point_type++) {
			    // pick a timer (any timer)
			    /* E587 JK do not need sql_timer
			    soar_module::timer* sql_timer = NULL;
			    switch (interval_type) {
			    case EPMEM_RANGE_EP:
				if (point_type == EPMEM_RANGE_START) {
				    sql_timer = my_agent->epmem_timers->query_sql_start_ep;
				} else {
				    sql_timer = my_agent->epmem_timers->query_sql_end_ep;
				}
				break;
			    case EPMEM_RANGE_NOW:
				if (point_type == EPMEM_RANGE_START) {
				    sql_timer = my_agent->epmem_timers->query_sql_start_now;
				} else {
				    sql_timer = my_agent->epmem_timers->query_sql_end_now;
				}
				break;
			    case EPMEM_RANGE_POINT:
				if (point_type == EPMEM_RANGE_START) {
				    sql_timer = my_agent->epmem_timers->query_sql_start_point;
				} else {
				    sql_timer = my_agent->epmem_timers->query_sql_end_point;
				}
				break;
			    }
			    */
			    // create the SQL query and bind it
			    // try to find an existing query first; if none exist, allocate a new one from the memory pools
			    soar_module::pooled_sqlite_statement* interval_sql = NULL;
			    // E587: AM: XXX: Need to remove these dependencies
			    // E587 JK now direct access to stmts graph
			    if (is_lti) {				
				//interval_sql = my_agent->epmem_worker_p->epmem_stmts_graph->pool_find_lti_queries[point_type][interval_type]->request(sql_timer);
				// E587 JK do not need sql_timer
				interval_sql = epmem_stmts_graph->pool_find_lti_queries[point_type][interval_type]->request();//sql_timer);
			    } else {
				//interval_sql = my_agent->epmem_worker_p->epmem_stmts_graph->pool_find_interval_queries[pedge->value_is_id][point_type][interval_type]->request(sql_timer);
				// E587 JK do not need sql_timer
				interval_sql = epmem_stmts_graph->pool_find_interval_queries[pedge->value_is_id][point_type][interval_type]->request();//sql_timer);
			    }
			    int bind_pos = 1;
			    if (point_type == EPMEM_RANGE_END && interval_type == EPMEM_RANGE_NOW) {
				interval_sql->bind_int(bind_pos++, current_episode);
			    }
			    interval_sql->bind_int(bind_pos++, edge_id);
			    if (is_lti) {
				// find the promotion time of the LTI, and use that as an after constraint
				interval_sql->bind_int(bind_pos++, promo_time);
			    }
			    interval_sql->bind_int(bind_pos++, current_episode);
			    if (interval_sql->execute() == soar_module::row) {
				epmem_interval* interval = (epmem_interval*) malloc(sizeof(epmem_interval));
				//allocate_with_pool(my_agent, &(my_agent->epmem_interval_pool), &interval); // E587 JK allocate in worker
				interval->is_end_point = point_type;
				interval->uedge = uedge;
				interval->time = interval_sql->column_int(0);
				interval->sql = interval_sql;
				interval_pq.push(interval);
				interval_cleanup.insert(interval);
				uedge->intervals++;
				created = true;
			    } else {
				interval_sql->get_pool()->release(interval_sql);
			    }
			}
		    }
		    if (created) {
			if (is_lti) {
			    // insert a dummy promo time start for LTIs
			    epmem_interval* start_interval = (epmem_interval*) malloc(sizeof(epmem_interval));
			    //allocate_with_pool(my_agent, &(my_agent->epmem_interval_pool), &start_interval); // E587 JK allocate in worker
			    start_interval->uedge = uedge;
			    start_interval->is_end_point = EPMEM_RANGE_START;
			    start_interval->time = promo_time - 1;
			    start_interval->sql = NULL;
			    interval_pq.push(start_interval);
			    interval_cleanup.insert(start_interval);
			}
			uedge->pedges.insert(pedge);
			uedge_cache->insert(std::make_pair(triple, uedge));
		    } else {
			uedge->pedges.~epmem_pedge_set();
			//E587 JK not using pools
			free(uedge);
                        //free_with_pool(&(my_agent->epmem_uedge_pool), uedge);
		    }
		} else {
		    epmem_uedge* uedge = (*uedge_iter).second;
		    uedge->pedges.insert(pedge);
		    if (uedge->activated) {
			for (epmem_literal_set::iterator lit_iter = pedge->literals.begin(); lit_iter != pedge->literals.end(); lit_iter++) {
			    epmem_literal* literal = (*lit_iter);
			    if (!literal->is_current || uedge->activation_count == 1) {
				changed_score |= epmem_satisfy_literal(literal, triple.q0, triple.q1, current_score, current_cardinality, symbol_node_count, uedge_caches, symbol_num_incoming);
			    }
			}
		    }
		}

		// put the partial edge query back into the queue if there's more
		// otherwise, reinitialize the query and put it in a pool
		if (pedge->sql && pedge->sql->execute() == soar_module::row) {
		    pedge->time = pedge->sql->column_int(2);
		    pedge_pq.push(pedge);
		} else if (pedge->sql) {
		    pedge->sql->get_pool()->release(pedge->sql);
		    pedge->sql = NULL;
		}
	    }
	    next_edge = (pedge_pq.empty() ? after : pedge_pq.top()->time);
	    //my_agent->epmem_timers->query_walk_edge->stop(); //E587 JK no timers

	    // process all intervals before the next edge arrives
	    //my_agent->epmem_timers->query_walk_interval->start(); //E587 JK no timers
	    while (interval_pq.size() && interval_pq.top()->time > next_edge && current_episode > after) {
		if (QUERY_DEBUG >= 1) {
		    std::cout << "EPISODE " << current_episode << std::endl;
		}
		// process all interval endpoints at this time step
		while (interval_pq.size() && interval_pq.top()->time >= current_episode) {
		    epmem_interval* interval = interval_pq.top();
		    interval_pq.pop();
		    epmem_uedge* uedge = interval->uedge;
		    epmem_triple triple = uedge->triple;
		    if (QUERY_DEBUG >= 1) {
			std::cout << "	INTERVAL (" << (interval->is_end_point ? "end" : "start") << "): " << triple.q0 << "-" << triple.w << "-" << triple.q1 << std::endl;
		    }
		    if (interval->is_end_point) {
			uedge->activated = true;
			uedge->activation_count++;
			for (epmem_pedge_set::iterator pedge_iter = uedge->pedges.begin(); pedge_iter != uedge->pedges.end(); pedge_iter++) {
			    epmem_pedge* pedge = *pedge_iter;
			    for (epmem_literal_set::iterator lit_iter = pedge->literals.begin(); lit_iter != pedge->literals.end(); lit_iter++) {
				epmem_literal* literal = *lit_iter;
				if (!literal->is_current || uedge->activation_count == 1) {
				    changed_score |= epmem_satisfy_literal(literal, triple.q0, triple.q1, current_score, current_cardinality, symbol_node_count, uedge_caches, symbol_num_incoming);
				}
			    }
			}
		    } else {
			uedge->activated = false;
			for (epmem_pedge_set::iterator pedge_iter = uedge->pedges.begin(); pedge_iter != uedge->pedges.end(); pedge_iter++) {
			    epmem_pedge* pedge = *pedge_iter;
			    for (epmem_literal_set::iterator lit_iter = pedge->literals.begin(); lit_iter != pedge->literals.end(); lit_iter++) {
				changed_score |= epmem_unsatisfy_literal(*lit_iter, triple.q0, triple.q1, current_score, current_cardinality, symbol_node_count);
			    }
			}
		    }
		    // put the interval query back into the queue if there's more and some literal cares
		    // otherwise, reinitialize the query and put it in a pool
		    if (interval->uedge->has_noncurrent && interval->sql && interval->sql->execute() == soar_module::row) {
			interval->time = interval->sql->column_int(0);
			interval_pq.push(interval);
		    } else if (interval->sql) {
			interval->sql->get_pool()->release(interval->sql);
			interval->sql = NULL;
			uedge->intervals--;
			if (uedge->intervals) {
			    interval_cleanup.erase(interval);
			    //E587 JK 
			    free(interval);
			    //free_with_pool(&(my_agent->epmem_interval_pool), interval);
			} else {
			    // TODO retract intervals
			}
		    }
		}
		next_interval = (interval_pq.empty() ? after : interval_pq.top()->time);
		next_episode = (next_edge > next_interval ? next_edge : next_interval);

		// update the prohibits list to catch up
		while (prohibits.size() && prohibits.back() > current_episode) {
		    prohibits.pop_back();
		}
		// ignore the episode if it is prohibited
		while (prohibits.size() && current_episode > next_episode && current_episode == prohibits.back()) {
		    current_episode--;
		    prohibits.pop_back();
		}

		if (QUERY_DEBUG >= 2) {
		    epmem_print_retrieval_state(literal_cache, pedge_caches, uedge_caches);
		}
		/* E587 JK ignore for now
		if (my_agent->sysparams[TRACE_EPMEM_SYSPARAM]) {
		    char buf[256];
		    SNPRINTF(buf, 254, "CONSIDERING EPISODE (time, cardinality, score) (%lld, %ld, %f)\n", static_cast<long long int>(current_episode), current_cardinality, current_score);
		    print(my_agent, buf);
		    xml_generate_warning(my_agent, buf);
		}
		*/
#ifdef EPMEM_EXPERIMENT
		epmem_episodes_searched++;
#endif

		// if
		// * the current time is still before any new intervals
		// * and the score was changed in this period
		// * and the new score is higher than the best score
		// then save the current time as the best one
		if (current_episode > next_episode && changed_score && (best_episode == EPMEM_MEMID_NONE || current_score > best_score || (do_graph_match && current_score == best_score && !best_graph_matched))) {
		    bool new_king = false;
		    if (best_episode == EPMEM_MEMID_NONE || current_score > best_score) {
			best_episode = current_episode;
			best_score = current_score;
			best_cardinality = current_cardinality;
			new_king = true;
		    }
		    // we should graph match if the option is set and all leaf literals are satisfied
		    if (current_cardinality == perfect_cardinality) {
			bool graph_matched = false;
			if (do_graph_match) {
			    if (gm_order == epmem_param_container::gm_order_undefined) {
				std::sort(gm_ordering.begin(), gm_ordering.end());
			    } else if (gm_order == epmem_param_container::gm_order_mcv) {
				std::sort(gm_ordering.begin(), gm_ordering.end(), epmem_gm_mcv_comparator);
			    }
			    epmem_literal_deque::iterator begin = gm_ordering.begin();
			    epmem_literal_deque::iterator end = gm_ordering.end();
			    best_bindings.clear();
			    epmem_node_symbol_map bound_nodes[2];
			    if (QUERY_DEBUG >= 1) {
				std::cout << "	GRAPH MATCH" << std::endl;
				epmem_print_retrieval_state(literal_cache, pedge_caches, uedge_caches);
			    }
			    //my_agent->epmem_timers->query_graph_match->start(); //E587JK
			    graph_matched = epmem_graph_match(begin, end, best_bindings, bound_nodes, 2);
			    //my_agent->epmem_timers->query_graph_match->stop(); //E587JK
			}
			if (!do_graph_match || graph_matched) {
			    best_episode = current_episode;
			    best_graph_matched = true;
			    current_episode = EPMEM_MEMID_NONE;
			    new_king = true;
			}
		    }
		    /* TODO E587 JK remove for now
		    if (new_king && my_agent->sysparams[TRACE_EPMEM_SYSPARAM]) {
			char buf[256];
			SNPRINTF(buf, 254, "NEW KING (perfect, graph-match): (%s, %s)\n", (current_cardinality == perfect_cardinality ? "true" : "false"), (best_graph_matched ? "true" : "false"));
			print(my_agent, buf);
			xml_generate_warning(my_agent, buf);
		    }
		    */
		}

		if (current_episode == EPMEM_MEMID_NONE) {
		    break;
		} else {
		    current_episode = next_episode;
		}
	    }
	    //my_agent->epmem_timers->query_walk_interval->stop();//E587 JK no timer
	}
	//my_agent->epmem_timers->query_walk->stop();//E587 JK no timer

	// if the best episode is the default, fail
	// otherwise, put the episode in working memory
	// ggggg
	// E587 JK at this point have found best episode and should message manager


	int buffSize = sizeof(query_rsp_data) + sizeof(int)*2 + 
	    sizeof(EPMEM_MSG_TYPE);
	epmem_msg * msg = (epmem_msg*) malloc(buffSize);
	query_rsp_data* rsp = (query_rsp_data*)msg->data;
	rsp->best_episode = best_episode;
	memcpy(&rsp->pos_query, pos_query, sizeof(Symbol));
	memcpy(&rsp->neg_query, neg_query, sizeof(Symbol));
	rsp->leaf_literals_size = (int) leaf_literals.size();
	rsp->best_score = best_score;
	rsp->best_graph_matched = best_graph_matched;
	rsp->best_cardinality = best_cardinality;
	rsp->perfect_score = perfect_score;
	rsp->do_graph_match = do_graph_match;

	msg->type = SEARCH_RESULT;
	msg->size = buffSize;
	msg->source = MPI::COMM_WORLD.Get_rank();
	//TODO serialize maps
	//msg master
	MPI::COMM_WORLD.Send(msg, buffSize, MPI::CHAR, MANAGER_ID, 1);
       
	/*
	if (best_episode == EPMEM_MEMID_NONE) {
	    epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_failure, pos_query);
	    if (neg_query) {
		epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_failure, neg_query);
	    }
	} else {
	    my_agent->epmem_timers->query_result->start();
	    Symbol* temp_sym;
	    epmem_id_mapping node_map_map;
	    epmem_id_mapping node_mem_map;
	    // cue size
	    temp_sym = make_int_constant(my_agent, leaf_literals.size());
	    epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_cue_size, temp_sym);
	    symbol_remove_ref(my_agent, temp_sym);
	    // match cardinality
	    temp_sym = make_int_constant(my_agent, best_cardinality);
	    epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_match_cardinality, temp_sym);
	    symbol_remove_ref(my_agent, temp_sym);
	    // match score
	    temp_sym = make_float_constant(my_agent, best_score);
	    epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_match_score, temp_sym);
	    symbol_remove_ref(my_agent, temp_sym);
	    // normalized match score
	    temp_sym = make_float_constant(my_agent, best_score / perfect_score);
	    epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_normalized_match_score, temp_sym);
	    symbol_remove_ref(my_agent, temp_sym);
	    // status
	    epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_success, pos_query);
	    if (neg_query) {
		epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_success, neg_query);
	    }
	    // give more metadata if graph match is turned on
	    if (do_graph_match) {
		// graph match
		temp_sym = make_int_constant(my_agent, (best_graph_matched ? 1 : 0));
		epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_graph_match, temp_sym);
		symbol_remove_ref(my_agent, temp_sym);

		// mapping
		if (best_graph_matched) {
		    goal_stack_level level = state->id.epmem_result_header->id.level;
		    // mapping identifier
		    Symbol* mapping = make_new_identifier(my_agent, 'M', level);
		    epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_graph_match_mapping, mapping);
		    symbol_remove_ref(my_agent, mapping);

		    for (epmem_literal_node_pair_map::iterator iter = best_bindings.begin(); iter != best_bindings.end(); iter++) {
			if ((*iter).first->value_is_id) {
			    // create the node
			    temp_sym = make_new_identifier(my_agent, 'N', level);
			    epmem_buffer_add_wme(meta_wmes, mapping, my_agent->epmem_sym_graph_match_mapping_node, temp_sym);
			    symbol_remove_ref(my_agent, temp_sym);
			    // point to the cue identifier
			    epmem_buffer_add_wme(meta_wmes, temp_sym, my_agent->epmem_sym_graph_match_mapping_cue, (*iter).first->value_sym);
			    // save the mapping point for the episode
			    node_map_map[(*iter).second.second] = temp_sym;
			    node_mem_map[(*iter).second.second] = NULL;
			}
		    }
		}
	    }
	    // reconstruct the actual episode
	    if (level > 2) {
		epmem_install_memory(my_agent, state, best_episode, meta_wmes, retrieval_wmes, &node_mem_map);
	    }
	    if (best_graph_matched) {
		for (epmem_id_mapping::iterator iter = node_mem_map.begin(); iter != node_mem_map.end(); iter++) {
		    epmem_id_mapping::iterator map_iter = node_map_map.find((*iter).first);
		    if (map_iter != node_map_map.end() && (*iter).second) {
			epmem_buffer_add_wme(meta_wmes, (*map_iter).second, my_agent->epmem_sym_retrieved, (*iter).second);
		    }
		}
	    }
	    my_agent->epmem_timers->query_result->stop();
	}
	*/
    }
// EEEEE	
    // cleanup
    //my_agent->epmem_timers->query_cleanup->start(); //E587 JK no timer
    for (epmem_interval_set::iterator iter = interval_cleanup.begin(); iter != interval_cleanup.end(); iter++) {
	epmem_interval* interval = *iter;
	if (interval->sql) {
	    interval->sql->get_pool()->release(interval->sql);
	}
	// E587 JK 
	free(interval);
	//free_with_pool(&(my_agent->epmem_interval_pool), interval);
    }
    for (int type = EPMEM_RIT_STATE_NODE; type <= EPMEM_RIT_STATE_EDGE; type++) {
	for (epmem_triple_pedge_map::iterator iter = pedge_caches[type].begin(); iter != pedge_caches[type].end(); iter++) {
	    epmem_pedge* pedge = (*iter).second;
	    if (pedge->sql) {
		pedge->sql->get_pool()->release(pedge->sql);
	    }
	    pedge->literals.~epmem_literal_set();
	    // E587 JK
	    free(pedge);
	    //free_with_pool(&(my_agent->epmem_pedge_pool), pedge);
	}
	for (epmem_triple_uedge_map::iterator iter = uedge_caches[type].begin(); iter != uedge_caches[type].end(); iter++) {
	    epmem_uedge* uedge = (*iter).second;
	    uedge->pedges.~epmem_pedge_set();
	    // E587 JK
	    free(uedge);
	    //free_with_pool(&(my_agent->epmem_uedge_pool), uedge);
	}
    }
    for (epmem_wme_literal_map::iterator iter = literal_cache.begin(); iter != literal_cache.end(); iter++) {
	epmem_literal* literal = (*iter).second;
	literal->parents.~epmem_literal_set();
	literal->children.~epmem_literal_set();
	literal->matches.~epmem_node_pair_set();
	literal->values.~epmem_node_int_map();
	// E587 JK
	free(literal);
	//free_with_pool(&(my_agent->epmem_literal_pool), literal);
    }
    //my_agent->epmem_timers->query_cleanup->stop(); //E587 JK no timer

    //my_agent->epmem_timers->query->stop(); //E587 JK no timer
}


// E587 JK epmem build DNF
epmem_literal* epmem_worker::epmem_build_dnf(
    wme* cue_wme, epmem_wme_literal_map& literal_cache, 
    epmem_literal_set& leaf_literals, epmem_symbol_int_map& symbol_num_incoming, 
    epmem_literal_deque& gm_ordering, epmem_symbol_set& currents, int query_type, 
    std::set<Symbol*>& visiting, soar_module::wme_set& cue_wmes, double balance) {
    // if the value is being visited, this is part of a loop; return NULL
    // remove this check (and in fact, the entire visiting parameter) if cyclic cues are allowed
    if (visiting.count(cue_wme->value)) {
	return NULL;
    }
    // if the value is an identifier and we've been here before, we can return the previous literal
    if (literal_cache.count(cue_wme)) {
	return literal_cache[cue_wme];
    }

    cue_wmes.insert(cue_wme);
    Symbol* value = cue_wme->value;
    epmem_literal* literal = (epmem_literal*) malloc(sizeof(epmem_literal)); //E587 JK
    //allocate_with_pool(my_agent, &(my_agent->epmem_literal_pool), &literal);
    new(&(literal->parents)) epmem_literal_set();
    new(&(literal->children)) epmem_literal_set();

    if (value->common.symbol_type != IDENTIFIER_SYMBOL_TYPE) { // WME is a value
	literal->value_is_id = EPMEM_RIT_STATE_NODE;
	literal->is_leaf = true;
	// E587 JK TODO need to get temporal_hash
	//literal->q1 = epmem_temporal_hash(my_agent, value); //E587 fix
	leaf_literals.insert(literal);
    /* E587 JK ignoring LTI 
    } else if (value->id.smem_lti) { // WME is an LTI
	// E587: AM:
	// if we can find the LTI node id, cache it; otherwise, return failure
	my_agent->epmem_stmts_master->find_lti->bind_int(1, static_cast<uint64_t>(value->id.name_letter));
	my_agent->epmem_stmts_master->find_lti->bind_int(2, static_cast<uint64_t>(value->id.name_number));
	if (my_agent->epmem_stmts_master->find_lti->execute() == soar_module::row) {
	    literal->value_is_id = EPMEM_RIT_STATE_EDGE;
	    literal->is_leaf = true;
	    literal->q1 = my_agent->epmem_stmts_master->find_lti->column_int(0);
	    my_agent->epmem_stmts_master->find_lti->reinitialize();
	    leaf_literals.insert(literal);
	} else {
	    my_agent->epmem_stmts_master->find_lti->reinitialize();
	    literal->parents.~epmem_literal_set();
	    literal->children.~epmem_literal_set();
	    // E587 JK
	    free(literal);
	    //free_with_pool(&(my_agent->epmem_literal_pool), literal);
	    return NULL;
	}
    */
    } else { // WME is a normal identifier
	// we determine whether it is a leaf by checking for children
	epmem_wme_list* children = epmem_get_augs_of_id(value, get_new_tc_number());//my_agent)); //E587 JK
	literal->value_is_id = EPMEM_RIT_STATE_EDGE;
	literal->q1 = EPMEM_NODEID_BAD;

	// if the WME has no children, then it's a leaf
	// otherwise, we recurse for all children
	if (children->empty()) {
	    literal->is_leaf = true;
	    leaf_literals.insert(literal);
	    delete children;
	} else {
	    bool cycle = false;
	    visiting.insert(cue_wme->value);
	    for (epmem_wme_list::iterator wme_iter = children->begin(); wme_iter != children->end(); wme_iter++) {
		// check to see if this child forms a cycle
		// if it does, we skip over it
		epmem_literal* child = epmem_build_dnf(*wme_iter, literal_cache, leaf_literals, symbol_num_incoming, gm_ordering, currents, query_type, visiting, cue_wmes, balance);
		if (child) {
		    child->parents.insert(literal);
		    literal->children.insert(child);
		} else {
		    cycle = true;
		}
	    }
	    delete children;
	    visiting.erase(cue_wme->value);
	    // if all children of this WME lead to cycles, then we don't need to walk this path
	    // in essence, this forces the DNF graph to be acyclic
	    // this results in savings in not walking edges and intervals
	    if (cycle && literal->children.empty()) {
		literal->parents.~epmem_literal_set();
		literal->children.~epmem_literal_set();
		// E587 JK
		free(literal);
		//free_with_pool(&(my_agent->epmem_literal_pool), literal);
		return NULL;
	    }
	    literal->is_leaf = false;
	    epmem_symbol_int_map::iterator rem_iter = symbol_num_incoming.find(value);
	    if (rem_iter == symbol_num_incoming.end()) {
		symbol_num_incoming[value] = 1;
	    } else {
		(*rem_iter).second++;
	    }
	}
    }

    if (!query_type) {
	gm_ordering.push_front(literal);
    }

    literal->id_sym = cue_wme->id;
    literal->value_sym = cue_wme->value;
    literal->is_current = (currents.count(value) > 0);
    // E587 JK TODO need to get temporal_hash
    //literal->w = epmem_temporal_hash(my_agent, cue_wme->attr); // E587 JK fix
    literal->is_neg_q = query_type;
    // E587 JK TODO need wme_activation
    //literal->weight = (literal->is_neg_q ? -1 : 1) * (balance >= 1.0 - 1.0e-8 ? 1.0 : wma_get_wme_activation(my_agent, cue_wme, true));
    //literal->weight = (literal->is_neg_q ? -1 : 1) * (my_agent->epmem_params->balance->get_value() >= 1.0 - 1.0e-8 ? 1.0 : wma_get_wme_activation(my_agent, cue_wme, true));
#ifdef USE_MEM_POOL_ALLOCATORS
    new(&(literal->matches)) epmem_node_pair_set(std::less<epmem_node_pair>(), soar_module::soar_memory_pool_allocator<epmem_node_pair>(my_agent));
#else
    new(&(literal->matches)) epmem_node_pair_set();
#endif
    new(&(literal->values)) epmem_node_int_map();

    literal_cache[cue_wme] = literal;
    return literal;
}

#ifdef asdfadsfasd
bool epmem_register_pedges(
    epmem_node_id parent, epmem_literal* literal, epmem_pedge_pq& pedge_pq, 
    epmem_time_id after, epmem_triple_pedge_map pedge_caches[], 
    std::map<epmem_triple, epmem_uedge*> uedge_caches[]) {
    // we don't need to keep track of visited literals/nodes because the literals are guaranteed to be acyclic
    // that is, the expansion to the literal's children will eventually bottom out
    // select the query
    epmem_triple triple = {parent, literal->w, literal->q1};
    int is_edge = literal->value_is_id;
    if (QUERY_DEBUG >= 1) {
		std::cout << "		RECURSING ON " << parent << " " << literal << std::endl;
    }
    // if the unique edge does not exist, create a new unique edge query
    // otherwse, if the pedge has not been registered with this literal
    epmem_triple_pedge_map* pedge_cache = &(pedge_caches[is_edge]);
    epmem_triple_pedge_map::iterator pedge_iter = pedge_cache->find(triple);
    epmem_pedge* child_pedge = NULL;
    if(pedge_iter != pedge_cache->end()){
		child_pedge = (*pedge_iter).second;
    }
    if (pedge_iter == pedge_cache->end() || (*pedge_iter).second == NULL) {
	int has_value = (literal->q1 != EPMEM_NODEID_BAD ? 1 : 0);
	// E587: AM: XXX: Need to remove this link
	// E587 direct access?
	//soar_module::pooled_sqlite_statement* pedge_sql = my_agent->epmem_worker_p->epmem_stmts_graph->pool_find_edge_queries[is_edge][has_value]->request(my_agent->epmem_timers->query_sql_edge);

	// E587 JK TODO need query_sql_edge?
	soar_module::pooled_sqlite_statement* pedge_sql = NULL;//epmem_stmts_graph->pool_find_edge_queries[is_edge][has_value]->request(my_agent->epmem_timers->query_sql_edge); //E587 todo epmem_timers
	int bind_pos = 1;
	if (!is_edge) {
	    pedge_sql->bind_int(bind_pos++, LLONG_MAX);
	}
	pedge_sql->bind_int(bind_pos++, triple.q0);
	pedge_sql->bind_int(bind_pos++, triple.w);
	if (has_value) {
	    pedge_sql->bind_int(bind_pos++, triple.q1);
	}
	if (is_edge) {
	    pedge_sql->bind_int(bind_pos++, after);
	}
	if (pedge_sql->execute() == soar_module::row) {
	    epmem_pedge* child_pedge = (epmem_pedge*) malloc(sizeof(epmem_pedge)); //E587 JK
	    //allocate_with_pool(my_agent, &(my_agent->epmem_pedge_pool), &child_pedge);
	    child_pedge->triple = triple;
	    child_pedge->value_is_id = literal->value_is_id;
	    child_pedge->has_noncurrent = !literal->is_current;
	    child_pedge->sql = pedge_sql;
	    new(&(child_pedge->literals)) epmem_literal_set();
	    child_pedge->literals.insert(literal);
	    child_pedge->time = child_pedge->sql->column_int(2);
	    pedge_pq.push(child_pedge);
	    (*pedge_cache)[triple] = child_pedge;
	    return true;
	} else {
	    pedge_sql->get_pool()->release(pedge_sql);
	    return false;
	}
    } else if (!child_pedge->literals.count(literal)) {
	child_pedge->literals.insert(literal);
	if (!literal->is_current) {
	    child_pedge->has_noncurrent = true;
	}
	// if the literal is an edge with no specified value, add the literal to all potential pedges
	if (!literal->is_leaf && literal->q1 == EPMEM_NODEID_BAD) {
	    bool created = false;
	    epmem_triple_uedge_map* uedge_cache = &uedge_caches[is_edge];
	    for (epmem_triple_uedge_map::iterator uedge_iter = uedge_cache->lower_bound(triple); uedge_iter != uedge_cache->end(); uedge_iter++) {
		epmem_triple child_triple = (*uedge_iter).first;
		// make sure we're still looking at the right edge(s)
		if (child_triple.q0 != triple.q0 || child_triple.w != triple.w) {
		    break;
		}
		epmem_uedge* child_uedge = (*uedge_iter).second;
		if (child_triple.q1 != EPMEM_NODEID_BAD && child_uedge->value_is_id) {
		    for (epmem_literal_set::iterator child_iter = literal->children.begin(); child_iter != literal->children.end(); child_iter++) {
			created |= epmem_register_pedges(child_triple.q1, *child_iter, pedge_pq, after, pedge_caches, uedge_caches);
		    }
		}
	    }
	    return created;
	}
    }
    return true;
}
#endif



bool epmem_worker::epmem_graph_match(
    epmem_literal_deque::iterator& dnf_iter, epmem_literal_deque::iterator& iter_end, 
    epmem_literal_node_pair_map& bindings, epmem_node_symbol_map bound_nodes[], 
    int depth = 0) 
{
    if (dnf_iter == iter_end) {
	return true;
    }
    epmem_literal* literal = *dnf_iter;
    if (bindings.count(literal)) {
	return false;
    }
    epmem_literal_deque::iterator next_iter = dnf_iter;
    next_iter++;
// E587 JK try without mem pool allocators?
#ifdef USE_MEM_POOL_ALLOCATORS
    epmem_node_set failed_parents = epmem_node_set(std::less<epmem_node_id>(), soar_module::soar_memory_pool_allocator<epmem_node_id>(my_agent));
    epmem_node_set failed_children = epmem_node_set(std::less<epmem_node_id>(), soar_module::soar_memory_pool_allocator<epmem_node_id>(my_agent));
#else
    epmem_node_set failed_parents;
    epmem_node_set failed_children;
#endif
    // go through the list of matches, binding each one to this literal in turn
    for (epmem_node_pair_set::iterator match_iter = literal->matches.begin(); match_iter != literal->matches.end(); match_iter++) {
	epmem_node_id q0 = (*match_iter).first;
	epmem_node_id q1 = (*match_iter).second;
	if (failed_parents.count(q0)) {
	    continue;
	}
	if (QUERY_DEBUG >= 2) {
	    for (int i = 0; i < depth; i++) {
		std::cout << "\t";
	    }
	    std::cout << "TRYING " << literal << " " << q0 << std::endl;
	}
	bool relations_okay = true;
	// for all parents
	for (epmem_literal_set::iterator parent_iter = literal->parents.begin(); relations_okay && parent_iter != literal->parents.end(); parent_iter++) {
	    epmem_literal* parent = *parent_iter;
	    epmem_literal_node_pair_map::iterator bind_iter = bindings.find(parent);
	    if (bind_iter != bindings.end() && (*bind_iter).second.second != q0) {
		relations_okay = false;
	    }
	}
	if (!relations_okay) {
	    if (QUERY_DEBUG >= 2) {
		for (int i = 0; i < depth; i++) {
		    std::cout << "\t";
		}
		std::cout << "PARENT CONSTRAINT FAIL" << std::endl;
	    }
	    failed_parents.insert(q0);
	    continue;
	}
	// if the node has already been bound, make sure it's bound to the same thing
	epmem_node_symbol_map::iterator binder = bound_nodes[literal->value_is_id].find(q1);
	if (binder != bound_nodes[literal->value_is_id].end() && (*binder).second != literal->value_sym) {
	    failed_children.insert(q1);
	    continue;
	}
	if (QUERY_DEBUG >= 2) {
	    for (int i = 0; i < depth; i++) {
		std::cout << "\t";
	    }
	    std::cout << "TRYING " << literal << " " << q0 << " " << q1 << std::endl;
	}
	if (literal->q1 != EPMEM_NODEID_BAD && literal->q1 != q1) {
	    relations_okay = false;
	}
	// for all children
	for (epmem_literal_set::iterator child_iter = literal->children.begin(); relations_okay && child_iter != literal->children.end(); child_iter++) {
	    epmem_literal* child = *child_iter;
	    epmem_literal_node_pair_map::iterator bind_iter = bindings.find(child);
	    if (bind_iter != bindings.end() && (*bind_iter).second.first != q1) {
		relations_okay = false;
	    }
	}
	if (!relations_okay) {
	    if (QUERY_DEBUG >= 2) {
		for (int i = 0; i < depth; i++) {
		    std::cout << "\t";
		}
		std::cout << "CHILD CONSTRAINT FAIL" << std::endl;
	    }
	    failed_children.insert(q1);
	    continue;
	}
	if (QUERY_DEBUG >= 2) {
	    for (int i = 0; i < depth; i++) {
		std::cout << "\t";
	    }
	    std::cout << literal << " " << q0 << " " << q1 << std::endl;
	}
	// temporarily modify the bindings and bound nodes
	bindings[literal] = std::make_pair(q0, q1);
	bound_nodes[literal->value_is_id][q1] = literal->value_sym;
	// recurse on the rest of the list
	bool list_satisfied = epmem_graph_match(next_iter, iter_end, bindings, bound_nodes, depth + 1);
	// if the rest of the list matched, we've succeeded
	// otherwise, undo the temporarily modifications and try again
	if (list_satisfied) {
	    return true;
	} else {
	    bindings.erase(literal);
	    bound_nodes[literal->value_is_id].erase(q1);
	}
    }
    // this means we've tried everything and this whole exercise was a waste of time
    // EPIC FAIL
    if (QUERY_DEBUG >= 2) {
	for (int i = 0; i < depth; i++) {
	    std::cout << "\t";
	}
	std::cout << "EPIC FAIL" << std::endl;
    }
    return false;
}
//#endif
