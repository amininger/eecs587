#include "epmem_worker.h"
#include <iostream>
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
    get_min_time.prepare();
    get_max_time.prepare();

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
        get_node_point.prepare();

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
        get_edge_point.prepare();

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
        get_node_now.prepare();

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
        update_edge_now_start.prepare();
		update_edge_now_start.bind_int(1, min_time);
		update_edge_now_start.bind_int(2, last_removal);
		update_edge_now_start.execute();
	}

	// node_range
	{
		// Add all node_ranges that start on the episode being removed
		soar_module::sqlite_statement get_node_range(epmem_db, "SELECT child_id, parent_id, attrib, value FROM node_unique WHERE child_id IN (SELECT id FROM node_range WHERE start=?)");
        get_node_range.prepare();

		get_node_range.bind_int(1, min_time);
		while(get_node_range.execute() == soar_module::row){
			nodes_to_add.push_back(epmem_node_unique(&get_node_range));
		}

		// Get a list of all ranges that will collapse to a point when the episode is removed
		std::vector<epmem_node_id> new_node_points;
		soar_module::sqlite_statement get_node_range_to_remove(epmem_db, "SELECT id FROM node_range WHERE (start=? AND end=?)");
        get_node_range_to_remove.prepare();

		get_node_range_to_remove.bind_int(1, last_removal);
		get_node_range_to_remove.bind_int(2, min_time);
		while(get_node_range_to_remove.execute() == soar_module::row){
			new_node_points.push_back(get_node_range_to_remove.column_int(0));
		}

		// Delete the ranges and add node_points
		soar_module::sqlite_statement remove_node_range(epmem_db, "DELETE FROM node_range WHERE (start=? AND end=?)");
        remove_node_range.prepare();
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
        update_node_range_start.prepare();
		update_node_range_start.bind_int(1, min_time);
		update_node_range_start.bind_int(2, last_removal);
		update_node_range_start.execute();
	}

		// edge_range
	{
		// Add all edge_ranges that start on the episode being removed
		soar_module::sqlite_statement get_edge_range(epmem_db, "SELECT parent_id, q0, w, q1 FROM edge_unique WHERE parent_id IN (SELECT id FROM edge_range WHERE start=?)");
        get_edge_range.prepare();

		get_edge_range.bind_int(1, min_time);
		while(get_edge_range.execute() == soar_module::row){
			edges_to_add.push_back(epmem_edge_unique(&get_edge_range));
		}

		// Get a list of all ranges that will collapse to a point when the episode is removed
		std::vector<epmem_node_id> new_edge_points;
		soar_module::sqlite_statement get_edge_range_to_remove(epmem_db, "SELECT id FROM edge_range WHERE (start=? AND end=?)");
        get_edge_range_to_remove.prepare();

		get_edge_range_to_remove.bind_int(1, last_removal);
		get_edge_range_to_remove.bind_int(2, min_time);
		while(get_edge_range_to_remove.execute() == soar_module::row){
			new_edge_points.push_back(get_edge_range_to_remove.column_int(0));
		}

		// Delete the ranges and add edge_points
		soar_module::sqlite_statement remove_edge_range(epmem_db, "DELETE FROM edge_range WHERE (start=? AND end=?)");
        remove_edge_range.prepare();
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
        update_edge_range_start.prepare();
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
        remove_unused_nodes.prepare();
        remove_unused_nodes.execute();
	}

	// edges_to_remove
	{
		std::copy(edges_to_remove.begin(), edges_to_remove.begin() + episode->num_removed_edges, episode->removed_edges);

		// Check to see if the edge_unique still exists somewhere, if not, delete it
		soar_module::sqlite_statement remove_unused_edges(epmem_db, "DELETE FROM edge_unique WHERE parent_id NOT IN (SELECT id FROM edge_now UNION SELECT id FROM edge_range UNION SELECT id FROM edge_point)");
        remove_unused_edges.prepare();
		remove_unused_edges.execute();
	}
		
	// update times table
	{
		last_removal = min_time;
		soar_module::sqlite_statement remove_time(epmem_db, "DELETE FROM times WHERE id=?");
        remove_time.prepare();
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

void epmem_worker::epmem_print_retrieval_state(epmem_wme_literal_map& literals, epmem_triple_pedge_map pedge_caches[], epmem_triple_uedge_map uedge_caches[]) {
	//std::map<epmem_node_id, std::string> tsh;
	std::cout << std::endl;
	std::cout << "digraph {" << std::endl;
	std::cout << "node [style=\"filled\"];" << std::endl;
	// LITERALS
	std::cout << "subgraph cluster_literals {" << std::endl;
	std::cout << "node [fillcolor=\"#0084D1\"];" << std::endl;
	for (epmem_wme_literal_map::iterator lit_iter = literals.begin(); lit_iter != literals.end(); lit_iter++) {
		epmem_literal* literal = (*lit_iter).second;
		if (literal->id_sym) {
			std::cout << "\"" << literal->value_sym << "\" [";
			if (literal->q1 == EPMEM_NODEID_BAD) {
				std::cout << "label=\"" << literal->value_sym << "\"";
			} else {
				std::cout << "label=\"" << literal->q1 << "\"";
			}
			if (!literal->value_is_id) {
				std::cout << ", shape=\"rect\"";
			}
			if (literal->matches.size() == 0) {
				std::cout << ", penwidth=\"2.0\"";
			}
			if (literal->is_neg_q) {
				std::cout << ", fillcolor=\"#C5000B\"";
			}
			std::cout << "];" << std::endl;
			std::cout << "\"" << literal->id_sym << "\" -> \"" << literal->value_sym << "\" [label=\"";
			if (literal->w == EPMEM_NODEID_BAD) {
				std::cout << "?";
			} else {
				std::cout << literal->w;
			}
			std::cout << "\\n" << literal << "\"];" << std::endl;
		}
	}
	std::cout << "};" << std::endl;
	// NODES / NODE->NODE
	std::cout << "subgraph cluster_uedges{" << std::endl;
	std::cout << "node [fillcolor=\"#FFD320\"];" << std::endl;
	for (int type = EPMEM_RIT_STATE_NODE; type <= EPMEM_RIT_STATE_EDGE; type++) {
		epmem_triple_uedge_map* uedge_cache = &uedge_caches[type];
		for (epmem_triple_uedge_map::iterator uedge_iter = uedge_cache->begin(); uedge_iter != uedge_cache->end(); uedge_iter++) {
			epmem_triple triple = (*uedge_iter).first;
			if (triple.q1 != EPMEM_NODEID_ROOT) {
				if (type == EPMEM_RIT_STATE_NODE) {
					std::cout << "\"n" << triple.q1 << "\" [shape=\"rect\"];" << std::endl;
				}
				std::cout << "\"e" << triple.q0 << "\" -> \"" << (type == EPMEM_RIT_STATE_NODE ? "n" : "e") << triple.q1 << "\" [label=\"" << triple.w << "\"];" << std::endl;
			}
		}
	}
	std::cout << "};" << std::endl;
	// PEDGES / LITERAL->PEDGE
	std::cout << "subgraph cluster_pedges {" << std::endl;
	std::cout << "node [fillcolor=\"#008000\"];" << std::endl;
	std::multimap<epmem_node_id, epmem_pedge*> parent_pedge_map;
	for (int type = EPMEM_RIT_STATE_NODE; type <= EPMEM_RIT_STATE_EDGE; type++) {
		for (epmem_triple_pedge_map::iterator pedge_iter = pedge_caches[type].begin(); pedge_iter != pedge_caches[type].end(); pedge_iter++) {
			epmem_triple triple = (*pedge_iter).first;
			epmem_pedge* pedge = (*pedge_iter).second;
			if (triple.w != EPMEM_NODEID_BAD) {
				std::cout << "\"" << pedge << "\" [label=\"" << pedge << "\\n(" << triple.q0 << ", " << triple.w << ", ";
				if (triple.q1 == EPMEM_NODEID_BAD) {
					std::cout << "?";
				} else {
					std::cout << triple.q1;
				}
				std::cout << ")\"";
				if (!pedge->value_is_id) {
					std::cout << ", shape=\"rect\"";
				}
				std::cout << "];" << std::endl;
				for (epmem_literal_set::iterator lit_iter = pedge->literals.begin(); lit_iter != pedge->literals.end(); lit_iter++) {
					epmem_literal* literal = *lit_iter;
					std::cout << "\"" << literal->value_sym << "\" -> \"" << pedge << "\";" << std::endl;
				}
				parent_pedge_map.insert(std::make_pair(triple.q0, pedge));
			}
		}
	}
	std::cout << "};" << std::endl;
	// PEDGE->PEDGE / PEDGE->NODE
	std::set<std::pair<epmem_pedge*, epmem_node_id> > drawn;
	for (int type = EPMEM_RIT_STATE_NODE; type <= EPMEM_RIT_STATE_EDGE; type++) {
		epmem_triple_uedge_map* uedge_cache = &uedge_caches[type];
		for (epmem_triple_uedge_map::iterator uedge_iter = uedge_cache->begin(); uedge_iter != uedge_cache->end(); uedge_iter++) {
			epmem_triple triple = (*uedge_iter).first;
			epmem_uedge* uedge = (*uedge_iter).second;
			if (triple.w != EPMEM_NODEID_BAD) {
				for (epmem_pedge_set::iterator pedge_iter = uedge->pedges.begin(); pedge_iter != uedge->pedges.end(); pedge_iter++) {
					epmem_pedge* pedge = *pedge_iter;
					std::pair<epmem_pedge*, epmem_node_id> pair = std::make_pair(pedge, triple.q0);
					if (!drawn.count(pair)) {
						drawn.insert(pair);
						std::cout << "\"" << pedge << "\" -> \"e" << triple.q0 << "\";" << std::endl;
					}
					std::cout << "\"" << pedge << "\" -> \"" << (pedge->value_is_id ? "e" : "n") << triple.q1 << "\" [style=\"dashed\"];" << std::endl;
					std::pair<std::multimap<epmem_node_id, epmem_pedge*>::iterator, std::multimap<epmem_node_id, epmem_pedge*>::iterator> pedge_iters = parent_pedge_map.equal_range(triple.q1);
					for (std::multimap<epmem_node_id, epmem_pedge*>::iterator pedge_iter = pedge_iters.first; pedge_iter != pedge_iters.second; pedge_iter++) {
						std::cout << "\"" << pedge << "\" -> \"" << (*pedge_iter).second << "\";" << std::endl;
					}
				}
			}
		}
	}
	std::cout << "}" << std::endl;
}

bool epmem_gm_mcv_comparator(const epmem_literal* a, const epmem_literal* b) {
	return (a->matches.size() < b->matches.size());
}

bool epmem_worker::epmem_register_pedges(epmem_node_id parent, epmem_literal* literal, epmem_pedge_pq& pedge_pq, epmem_time_id after, epmem_triple_pedge_map pedge_caches[], epmem_triple_uedge_map uedge_caches[]) {
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
		soar_module::pooled_sqlite_statement* pedge_sql = epmem_stmts_graph->pool_find_edge_queries[is_edge][has_value]->request();
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
			epmem_pedge* child_pedge = reinterpret_cast<epmem_pedge*>(malloc(sizeof(epmem_pedge)));
			child_pedge->triple = triple;
			child_pedge->value_is_id = literal->value_is_id;
			child_pedge->has_noncurrent = !literal->is_current;
			child_pedge->sql = pedge_sql;
            child_pedge->is_lti = literal->is_lti;
            child_pedge->promotion_time = literal->promotion_time;
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

bool epmem_worker::epmem_satisfy_literal(epmem_literal* literal, epmem_node_id parent, epmem_node_id child, double& current_score, long int& current_cardinality, epmem_symbol_node_pair_int_map& symbol_node_count, epmem_triple_uedge_map uedge_caches[], epmem_symbol_int_map& symbol_num_incoming) {
	epmem_symbol_node_pair_int_map::iterator match_iter;
	if (QUERY_DEBUG >= 1) {
		std::cout << "		RECURSING ON " << parent << " " << child << " " << literal << std::endl;
	}
	// check if the ancestors of this literal are satisfied
	bool parents_satisfied = (literal->id_sym == NULL);
	if (!parents_satisfied) {
		// ancestors are satisfied if:
		// 1. all incoming literals are satisfied
		// 2. all incoming literals have this particular node satisfying it
		int num_incoming = symbol_num_incoming[literal->id_sym];
		match_iter = symbol_node_count.find(std::make_pair(literal->id_sym, parent));
		// since, by definition, if a node satisfies all incoming literals, all incoming literals are satisfied
		parents_satisfied = (match_iter != symbol_node_count.end()) && ((*match_iter).second == num_incoming);
	}
	// if yes
	if (parents_satisfied) {
		// add the edge as a match
		literal->matches.insert(std::make_pair(parent, child));
		epmem_node_int_map::iterator values_iter = literal->values.find(child);
		if (values_iter == literal->values.end()) {
			literal->values[child] = 1;
			if (literal->is_leaf) {
				if (literal->matches.size() == 1) {
					current_score += literal->weight;
					current_cardinality += (literal->is_neg_q ? -1 : 1);
					if (QUERY_DEBUG >= 1) {
						std::cout << "			NEW SCORE: " << current_score << ", " << current_cardinality << std::endl;
					}
					return true;
				}
			} else {
				bool changed_score = false;
				// change bookkeeping information about ancestry
				epmem_symbol_node_pair match = std::make_pair(literal->value_sym, child);
				match_iter = symbol_node_count.find(match);
				if (match_iter == symbol_node_count.end()) {
					symbol_node_count[match] = 1;
				} else {
					symbol_node_count[match]++;
				}
				// recurse over child literals
				for (epmem_literal_set::iterator child_iter = literal->children.begin(); child_iter != literal->children.end(); child_iter++) {
					epmem_literal* child_lit = *child_iter;
					epmem_triple_uedge_map* uedge_cache = &uedge_caches[child_lit->value_is_id];
					epmem_triple child_triple = {child, child_lit->w, child_lit->q1};
					epmem_triple_uedge_map::iterator uedge_iter;
					epmem_uedge* child_uedge = NULL;
					if (child_lit->q1 == EPMEM_NODEID_BAD) {
						uedge_iter = uedge_cache->lower_bound(child_triple);
						while (uedge_iter != uedge_cache->end()) {
							child_triple = (*uedge_iter).first;
							child_uedge = (*uedge_iter).second;
							if (child_triple.q0 != child || child_triple.w != child_lit->w) {
								break;
							}
							if (child_uedge->activated && (!literal->is_current || child_uedge->activation_count == 1)) {
								changed_score |= epmem_satisfy_literal(child_lit, child_triple.q0, child_triple.q1, current_score, current_cardinality, symbol_node_count, uedge_caches, symbol_num_incoming);
							}
							uedge_iter++;
						}
					} else {
						uedge_iter = uedge_cache->find(child_triple);
						if(uedge_iter != uedge_cache->end()){
							child_uedge = (*uedge_iter).second;
							if (child_uedge->activated && (!literal->is_current || child_uedge->activation_count == 1)) {
								changed_score |= epmem_satisfy_literal(child_lit, child_triple.q0, child_triple.q1, current_score, current_cardinality, symbol_node_count, uedge_caches, symbol_num_incoming);
							}
						}
					}
				}
				return changed_score;
			}
		} else {
			(*values_iter).second++;
		}
	}
	return false;
}

bool epmem_worker::epmem_unsatisfy_literal(epmem_literal* literal, epmem_node_id parent, epmem_node_id child, double& current_score, long int& current_cardinality, epmem_symbol_node_pair_int_map& symbol_node_count) {
	epmem_symbol_int_map::iterator count_iter;
	if (literal->matches.size() == 0) {
		return false;
	}
	if (QUERY_DEBUG >= 1) {
		std::cout << "		RECURSING ON " << parent << " " << child << " " << literal << std::endl;
	}
	// we only need things if this parent-child pair is matching the literal
	epmem_node_pair_set::iterator lit_match_iter = literal->matches.find(std::make_pair(parent, child));
	if (lit_match_iter != literal->matches.end()) {
		// erase the edge from this literal's matches
		literal->matches.erase(lit_match_iter);
		epmem_node_int_map::iterator values_iter = literal->values.find(child);
		(*values_iter).second--;
		if ((*values_iter).second == 0) {
			literal->values.erase(values_iter);
			if (literal->is_leaf) {
				if (literal->matches.size() == 0) {
					current_score -= literal->weight;
					current_cardinality -= (literal->is_neg_q ? -1 : 1);
					if (QUERY_DEBUG >= 1) {
						std::cout << "			NEW SCORE: " << current_score << ", " << current_cardinality << std::endl;
					}
					return true;
				}
			} else {
				bool changed_score = false;
				epmem_symbol_node_pair match = std::make_pair(literal->value_sym, child);
				epmem_symbol_node_pair_int_map::iterator match_iter = symbol_node_count.find(match);
				(*match_iter).second--;
				if ((*match_iter).second == 0) {
					symbol_node_count.erase(match_iter);
				}
				// if this literal is no longer satisfied, recurse on all children
				// if this literal is still satisfied, recurse on children who is matching on descendants of this edge
				if (literal->matches.size() == 0) {
					for (epmem_literal_set::iterator child_iter = literal->children.begin(); child_iter != literal->children.end(); child_iter++) {
						epmem_literal* child_lit = *child_iter;
						for (epmem_node_pair_set::iterator node_iter = child_lit->matches.begin(); node_iter != child_lit->matches.end(); node_iter++) {
							changed_score |= epmem_unsatisfy_literal(child_lit, (*node_iter).first, (*node_iter).second, current_score, current_cardinality, symbol_node_count);
						}
					}
				} else {
					epmem_node_pair node_pair = std::make_pair(child, EPMEM_NODEID_BAD);
					for (epmem_literal_set::iterator child_iter = literal->children.begin(); child_iter != literal->children.end(); child_iter++) {
						epmem_literal* child_lit = *child_iter;
						epmem_node_pair_set::iterator node_iter = child_lit->matches.lower_bound(node_pair);
						if (node_iter != child_lit->matches.end() && (*node_iter).first == child) {
							changed_score |= epmem_unsatisfy_literal(child_lit, (*node_iter).first, (*node_iter).second, current_score, current_cardinality, symbol_node_count);
						}
					}
				}
				return changed_score;
			}
		}
	}
	return false;
}

bool epmem_worker::epmem_graph_match(epmem_literal_deque::iterator& dnf_iter, epmem_literal_deque::iterator& iter_end, epmem_literal_node_pair_map& bindings, epmem_node_symbol_map bound_nodes[], int depth = 0) {
	if (dnf_iter == iter_end) {
		return true;
	}
	epmem_literal* literal = *dnf_iter;
	if (bindings.count(literal)) {
		return false;
	}
	epmem_literal_deque::iterator next_iter = dnf_iter;
	next_iter++;
	epmem_node_set failed_parents;
	epmem_node_set failed_children;

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

epmem_literal* epmem_worker::epmem_build_dnf(epmem_query* query, epmem_cue_wme* cue_wme, epmem_wme_literal_map& literal_cache, epmem_literal_set& leaf_literals, epmem_symbol_int_map& symbol_num_incoming, epmem_literal_deque& gm_ordering, int query_type, epmem_cue_symbol_set& visiting, epmem_cue_symbol_set& currents, epmem_cue_wme_set& cue_wmes) {
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
	epmem_cue_symbol* value = cue_wme->value;
	epmem_literal* literal = reinterpret_cast<epmem_literal*>(malloc(sizeof(epmem_literal)));
	new(&(literal->parents)) epmem_literal_set();
	new(&(literal->children)) epmem_literal_set();

    if (!value->is_id) { // WME is a value
		literal->value_is_id = EPMEM_RIT_STATE_NODE;
		literal->is_leaf = true;
		literal->q1 = value->id;
        literal->is_lti = false;
		leaf_literals.insert(literal);
	} else if (value->is_lti) { // WME is an LTI
		// E587: AM:
		// if we can find the LTI node id, cache it; otherwise, return failure
		literal->value_is_id = EPMEM_RIT_STATE_EDGE;
		literal->is_leaf = true;
		literal->q1 = value->id;
        literal->is_lti = true;
        literal->promotion_time = value->promotion_time;
	} else { // WME is a normal identifier
		// we determine whether it is a leaf by checking for children
		epmem_cue_wme_list* children = value->children;
		literal->value_is_id = EPMEM_RIT_STATE_EDGE;
		literal->q1 = EPMEM_NODEID_BAD;
        literal->is_lti = false;

		// if the WME has no children, then it's a leaf
		// otherwise, we recurse for all children
		if (children->empty()) {
			literal->is_leaf = true;
			leaf_literals.insert(literal);
			delete children;
		} else {
			bool cycle = false;
			visiting.insert(cue_wme->value);
			for (epmem_cue_wme_list::iterator wme_iter = children->begin(); wme_iter != children->end(); wme_iter++) {
				// check to see if this child forms a cycle
				// if it does, we skip over it
				epmem_literal* child = epmem_build_dnf(query, *wme_iter, literal_cache, leaf_literals, symbol_num_incoming, gm_ordering, query_type, visiting, currents, cue_wmes);
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
                delete literal;
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
	literal->w = cue_wme->attr->id;
	literal->is_neg_q = query_type;
    // E587: AM: XXX: HACK, want real value here
    double balance = 1;
	literal->weight = (literal->is_neg_q ? -1 : 1) * (balance >= 1.0 - 1.0e-8 ? 1.0 : cue_wme->activation);
	new(&(literal->matches)) epmem_node_pair_set();
	new(&(literal->values)) epmem_node_int_map();

	literal_cache[cue_wme] = literal;
	return literal;
}

query_rsp_data* epmem_worker::epmem_perform_query(epmem_query* query){
    query_rsp_data* response = new query_rsp_data();
	// epmem options

    int level = query->level;

	// variables needed for cleanup
	epmem_wme_literal_map literal_cache;
	epmem_triple_pedge_map pedge_caches[2];
	epmem_triple_uedge_map uedge_caches[2] = {epmem_triple_uedge_map(), epmem_triple_uedge_map()};
	epmem_interval_set interval_cleanup = epmem_interval_set();

    // Initialize pos_query and neg_query
    epmem_cue_symbol* pos_query = &query->symbols[query->pos_query_index];
    epmem_cue_symbol* neg_query = NIL;
    if(query->neg_query_index != -1){
        neg_query = &query->symbols[query->neg_query_index];
    }

    // Initialize all the symbols
    for(epmem_cue_symbols_packed_list::iterator i = query->symbols.begin(); i != query->symbols.end(); i++){
        i->children = new epmem_cue_wme_list();
    }

    // Initialize the wmes
    epmem_cue_wme_set cue_wmes;
    for(epmem_packed_cue_wme_list::iterator i = query->wmes.begin(); i != query->wmes.end(); i++){
        epmem_cue_wme* new_wme = new epmem_cue_wme(&(*i), query);
        cue_wmes.insert(new_wme);
        query->symbols[i->id_index].children->push_back(new_wme);        
    }

	// Initialize the currents set
    epmem_cue_symbol_set currents;
    for(std::vector<int>::iterator i = query->currents.begin(); i != query->currents.end(); i++){
        currents.insert(&query->symbols[*i]);
    }

	// variables needed for building the DNF
	epmem_literal* root_literal = reinterpret_cast<epmem_literal*>(malloc(sizeof(epmem_literal)));
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
			root_literal->id_sym = NULL;
			root_literal->value_sym = pos_query;
			root_literal->is_neg_q = EPMEM_NODE_POS;
			root_literal->value_is_id = EPMEM_RIT_STATE_EDGE;
			root_literal->is_leaf = false;
			root_literal->is_current = false;
            root_literal->is_lti = false;
			root_literal->w = EPMEM_NODEID_BAD;
			root_literal->q1 = EPMEM_NODEID_ROOT;
			root_literal->weight = 0.0;
			new(&(root_literal->parents)) epmem_literal_set();
			new(&(root_literal->children)) epmem_literal_set();
			new(&(root_literal->matches)) epmem_node_pair_set();
			new(&(root_literal->values)) epmem_node_int_map();
			symbol_num_incoming[pos_query] = 1;
			literal_cache[NULL] = root_literal;

			std::set<epmem_cue_symbol*> visiting;
			visiting.insert(pos_query);
			visiting.insert(neg_query);
			for (int query_type = EPMEM_NODE_POS; query_type <= EPMEM_NODE_NEG; query_type++) {
				epmem_cue_symbol* query_root = NULL;
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
				// for each first level WME, build up a DNF
				for (epmem_cue_wme_list::iterator wme_iter = query_root->children->begin(); wme_iter != query_root->children->end(); wme_iter++) {
					epmem_literal* child = epmem_build_dnf(query, *wme_iter, literal_cache, leaf_literals, symbol_num_incoming, gm_ordering, query_type, visiting, currents, cue_wmes);
					if (child) {
						// force all first level literals to have the same id symbol
						child->id_sym = pos_query;
						child->parents.insert(root_literal);
						root_literal->children.insert(child);
					}
				}
			}
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
	    epmem_db->backup( "debug.db", new std::string("Backup error"));

		// set default values for before and after
		if (query->before == EPMEM_MEMID_NONE) {
	        soar_module::sqlite_statement get_max_time(epmem_db, "SELECT MAX(id) FROM times");
            get_max_time.prepare();
            if(get_max_time.execute() == soar_module::row){
			    query->before = get_max_time.column_int(0);
            }
		} else {
			query->before = query->before - 1; // since before's are strict
		}
		if (query->after == EPMEM_MEMID_NONE) {
			query->after = EPMEM_MEMID_NONE;
		}
		epmem_time_id current_episode = query->before;
		epmem_time_id next_episode;

		// create dummy edges and intervals
		{
			// insert dummy unique edge and interval end point queries for DNF root
			// we make an SQL statement just so we don't have to do anything special at cleanup
			epmem_triple triple = {EPMEM_NODEID_BAD, EPMEM_NODEID_BAD, EPMEM_NODEID_ROOT};
			epmem_pedge* root_pedge = reinterpret_cast<epmem_pedge*>(malloc(sizeof(epmem_pedge)));
			root_pedge->triple = triple;
			root_pedge->value_is_id = EPMEM_RIT_STATE_EDGE;
			root_pedge->has_noncurrent = false;
			new(&(root_pedge->literals)) epmem_literal_set();
			root_pedge->literals.insert(root_literal);
			root_pedge->sql = epmem_stmts_graph->pool_dummy->request();
			root_pedge->sql->prepare();
			root_pedge->sql->bind_int(1, LLONG_MAX);
			root_pedge->sql->execute( soar_module::op_reinit );
			root_pedge->time = LLONG_MAX;
            root_pedge->is_lti = root_literal->is_lti;
            root_pedge->promotion_time = root_literal->promotion_time;
			pedge_pq.push(root_pedge);
			pedge_caches[EPMEM_RIT_STATE_EDGE][triple] = root_pedge;

			epmem_uedge* root_uedge = reinterpret_cast<epmem_uedge*>(malloc(sizeof(epmem_uedge)));
			root_uedge->triple = triple;
			root_uedge->value_is_id = EPMEM_RIT_STATE_EDGE;
			root_uedge->has_noncurrent = false;
			root_uedge->activation_count = 0;
			new(&(root_uedge->pedges)) epmem_pedge_set();
			root_uedge->intervals = 1;
			root_uedge->activated = false;
			uedge_caches[EPMEM_RIT_STATE_EDGE][triple] = root_uedge;

			epmem_interval* root_interval = reinterpret_cast<epmem_interval*>(malloc(sizeof(epmem_interval)));
			root_interval->uedge = root_uedge;
			root_interval->is_end_point = true;
			root_interval->sql = epmem_stmts_graph->pool_dummy->request();
			root_interval->sql->prepare();
			root_interval->sql->bind_int(1, query->before);
			root_interval->sql->execute( soar_module::op_reinit );
			root_interval->time = query->before;
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
		while (pedge_pq.size() && current_episode > query->after) {
			epmem_time_id next_edge;
			epmem_time_id next_interval;

			bool changed_score = false;

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
							created |= epmem_register_pedges(triple.q1, *child_iter, pedge_pq, query->after, pedge_caches, uedge_caches);
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
					epmem_uedge* uedge = reinterpret_cast<epmem_uedge*>(malloc(sizeof(epmem_uedge)));
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
                    // Need to actually do this
                    bool is_lti = pedge->is_lti;
                    if(is_lti){
                        promo_time = pedge->promotion_time;
                    }
					for (int interval_type = EPMEM_RANGE_EP; interval_type <= EPMEM_RANGE_POINT; interval_type++) {
						for (int point_type = EPMEM_RANGE_START; point_type <= EPMEM_RANGE_END; point_type++) {
							// create the SQL query and bind it
							// try to find an existing query first; if none exist, allocate a new one from the memory pools
							soar_module::pooled_sqlite_statement* interval_sql = NULL;

							if (is_lti) {
								interval_sql = epmem_stmts_graph->pool_find_lti_queries[point_type][interval_type]->request();
							} else {
								interval_sql = epmem_stmts_graph->pool_find_interval_queries[pedge->value_is_id][point_type][interval_type]->request();
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
								epmem_interval* interval = reinterpret_cast<epmem_interval*>(malloc(sizeof(epmem_interval)));
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
                        // E587: AM: LTI: fix
						if (is_lti) {
							// insert a dummy promo time start for LTIs
							epmem_interval* start_interval = reinterpret_cast<epmem_interval*>(malloc(sizeof(epmem_interval)));
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
                        delete uedge;
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
			next_edge = (pedge_pq.empty() ? query->after : pedge_pq.top()->time);

			// process all intervals before the next edge arrives
			while (interval_pq.size() && interval_pq.top()->time > next_edge && current_episode > query->after) {
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
                            delete interval;
						} else {
							// TODO retract intervals
						}
					}
				}
				next_interval = (interval_pq.empty() ? query->after : interval_pq.top()->time);
				next_episode = (next_edge > next_interval ? next_edge : next_interval);

				// update the prohibits list to catch up
				while (query->prohibits.size() && query->prohibits.back() > current_episode) {
					query->prohibits.pop_back();
				}
				// ignore the episode if it is prohibited
				while (query->prohibits.size() && current_episode > next_episode && current_episode == query->prohibits.back()) {
					current_episode--;
					query->prohibits.pop_back();
				}

				if (QUERY_DEBUG >= 2) {
					epmem_print_retrieval_state(literal_cache, pedge_caches, uedge_caches);
				}

				// if
				// * the current time is still before any new intervals
				// * and the score was changed in this period
				// * and the new score is higher than the best score
				// then save the current time as the best one
				if (current_episode > next_episode && changed_score && (best_episode == EPMEM_MEMID_NONE || current_score > best_score || (query->do_graph_match && current_score == best_score && !best_graph_matched))) {
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
						if (query->do_graph_match) {
							if (query->gm_order == epmem_param_container::gm_order_undefined) {
								std::sort(gm_ordering.begin(), gm_ordering.end());
							} else if (query->gm_order == epmem_param_container::gm_order_mcv) {
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
							graph_matched = epmem_graph_match(begin, end, best_bindings, bound_nodes, 2);
						}
						if (!query->do_graph_match || graph_matched) {
							best_episode = current_episode;
							best_graph_matched = true;
							current_episode = EPMEM_MEMID_NONE;
							new_king = true;
						}
					}
				}

				if (current_episode == EPMEM_MEMID_NONE) {
					break;
				} else {
					current_episode = next_episode;
				}
			}
		}

        response->best_bindings = best_bindings;
        response->best_cardinality = best_cardinality;
        response->best_episode = best_episode;
        response->best_graph_matched = best_graph_matched;
        response->best_score = best_score;
        response->leaf_literals_size = leaf_literals.size();
        response->perfect_score = perfect_score;
        
	    // cleanup
	    for (epmem_interval_set::iterator iter = interval_cleanup.begin(); iter != interval_cleanup.end(); iter++) {
		    epmem_interval* interval = *iter;
		    if (interval->sql) {
			    interval->sql->get_pool()->release(interval->sql);
		    }
            delete interval;
	    }
	    for (int type = EPMEM_RIT_STATE_NODE; type <= EPMEM_RIT_STATE_EDGE; type++) {
		    for (epmem_triple_pedge_map::iterator iter = pedge_caches[type].begin(); iter != pedge_caches[type].end(); iter++) {
			    epmem_pedge* pedge = (*iter).second;
			    if (pedge->sql) {
				    pedge->sql->get_pool()->release(pedge->sql);
			    }
			    pedge->literals.clear();
                delete pedge;
		    }
		    for (epmem_triple_uedge_map::iterator iter = uedge_caches[type].begin(); iter != uedge_caches[type].end(); iter++) {
			    epmem_uedge* uedge = (*iter).second;
			    uedge->pedges.clear();
                delete uedge;
		    }
	    }
	    for (epmem_wme_literal_map::iterator iter = literal_cache.begin(); iter != literal_cache.end(); iter++) {
		    epmem_literal* literal = (*iter).second;
		    literal->parents.clear();
		    literal->children.clear();
		    literal->matches.clear();
		    literal->values.clear();
            delete literal;
	    }
    }
    return response;
}


// E587 JK initializes epmem pools
//void epmem_worker::initialize_epmem_pools()
//{
//    //todo this mem_pool calls expect agent* need to write own pool handlers
//    init_memory_pool(this, &epmem_literal_pool, 
//		     sizeof( epmem_literal), "epmem_literals");
//    init_memory_pool(this, &epmem_pedge_pool, 
//		     sizeof( epmem_pedge ), "epmem_pedges");
//    init_memory_pool(this, &epmem_uedge_pool, 
//		     sizeof( epmem_uedge ), "epmem_uedges" );
//    init_memory_pool(this, &epmem_interval_pool, 
//		     sizeof( epmem_interval ), "epmem_intervals" );
//}

