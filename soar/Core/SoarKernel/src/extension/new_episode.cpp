/*
 * new_episode.cpp
 *
 *  Created on: Nov 28, 2012
 *      Author: aaron
 */

#include "new_episode.h"

// all inserts
		{
			epmem_node_id *temp_node;

#ifdef EPMEM_EXPERIMENT
			epmem_dc_interval_inserts = epmem_node.size() + epmem_edge.size();
#endif

			// nodes
			while ( !epmem_node.empty() )
			{
				temp_node =& epmem_node.front();

				// add NOW entry
				// id = ?, start = ?
				my_agent->epmem_stmts_graph->add_node_now->bind_int( 1, (*temp_node) );
				my_agent->epmem_stmts_graph->add_node_now->bind_int( 2, time_counter );
				my_agent->epmem_stmts_graph->add_node_now->execute( soar_module::op_reinit );

				// update min
				(*my_agent->epmem_node_mins)[ (*temp_node) - 1 ] = time_counter;

				epmem_node.pop();
			}

			// edges
			while ( !epmem_edge.empty() )
			{
				temp_node =& epmem_edge.front();

				// add NOW entry
				// id = ?, start = ?
				my_agent->epmem_stmts_graph->add_edge_now->bind_int( 1, (*temp_node) );
				my_agent->epmem_stmts_graph->add_edge_now->bind_int( 2, time_counter );
				my_agent->epmem_stmts_graph->add_edge_now->execute( soar_module::op_reinit );

				// update min
				(*my_agent->epmem_edge_mins)[ (*temp_node) - 1 ] = time_counter;

				my_agent->epmem_stmts_graph->update_edge_unique_last->bind_int( 1, LLONG_MAX );
				my_agent->epmem_stmts_graph->update_edge_unique_last->bind_int( 2, *temp_node );
				my_agent->epmem_stmts_graph->update_edge_unique_last->execute( soar_module::op_reinit );

				epmem_edge.pop();
			}
		}

		// all removals
		{
			epmem_id_removal_map::iterator r;
			epmem_time_id range_start;
			epmem_time_id range_end;

#ifdef EPMEM_EXPERIMENT
			epmem_dc_interval_removes = 0;
#endif

			// nodes
			r = my_agent->epmem_node_removals->begin();
			while ( r != my_agent->epmem_node_removals->end() )
			{
				if ( r->second )
				{
#ifdef EPMEM_EXPERIMENT
					epmem_dc_interval_removes++;
#endif

					// remove NOW entry
					// id = ?
					my_agent->epmem_stmts_graph->delete_node_now->bind_int( 1, r->first );
					my_agent->epmem_stmts_graph->delete_node_now->execute( soar_module::op_reinit );

					range_start = (*my_agent->epmem_node_mins)[ r->first - 1 ];
					range_end = ( time_counter - 1 );

					// point (id, start)
					if ( range_start == range_end )
					{
						my_agent->epmem_stmts_graph->add_node_point->bind_int( 1, r->first );
						my_agent->epmem_stmts_graph->add_node_point->bind_int( 2, range_start );
						my_agent->epmem_stmts_graph->add_node_point->execute( soar_module::op_reinit );
					}
					// node
					else
					{
						epmem_rit_insert_interval( my_agent, range_start, range_end, r->first, &( my_agent->epmem_rit_state_graph[ EPMEM_RIT_STATE_NODE ] ) );
					}

					// update max
					(*my_agent->epmem_node_maxes)[ r->first - 1 ] = true;
				}

				r++;
			}
			my_agent->epmem_node_removals->clear();

			// edges
			r = my_agent->epmem_edge_removals->begin();
			while ( r != my_agent->epmem_edge_removals->end() )
			{
				if ( r->second )
				{
#ifdef EPMEM_EXPERIMENT
					epmem_dc_interval_removes++;
#endif

					// remove NOW entry
					// id = ?
					my_agent->epmem_stmts_graph->delete_edge_now->bind_int( 1, r->first );
					my_agent->epmem_stmts_graph->delete_edge_now->execute( soar_module::op_reinit );

					range_start = (*my_agent->epmem_edge_mins)[ r->first - 1 ];
					range_end = ( time_counter - 1 );

					my_agent->epmem_stmts_graph->update_edge_unique_last->bind_int( 1, range_end );
					my_agent->epmem_stmts_graph->update_edge_unique_last->bind_int( 2, r->first );
					my_agent->epmem_stmts_graph->update_edge_unique_last->execute( soar_module::op_reinit );
					// point (id, start)
					if ( range_start == range_end )
					{
						my_agent->epmem_stmts_graph->add_edge_point->bind_int( 1, r->first );
						my_agent->epmem_stmts_graph->add_edge_point->bind_int( 2, range_start );
						my_agent->epmem_stmts_graph->add_edge_point->execute( soar_module::op_reinit );
					}
					// node
					else
					{
						epmem_rit_insert_interval( my_agent, range_start, range_end, r->first, &( my_agent->epmem_rit_state_graph[ EPMEM_RIT_STATE_EDGE ] ) );
					}

					// update max
					(*my_agent->epmem_edge_maxes)[ r->first - 1 ] = true;
				}

				r++;
			}
			my_agent->epmem_edge_removals->clear();
		}

		// all in-place lti promotions
		{
			for ( epmem_symbol_set::iterator p_it=my_agent->epmem_promotions->begin(); p_it!=my_agent->epmem_promotions->end(); p_it++ )
			{
				if ( ( (*p_it)->id.smem_time_id == time_counter ) && ( (*p_it)->id.smem_valid == my_agent->epmem_validation ) )
				{
					_epmem_promote_id( my_agent, (*p_it), time_counter );
				}

				symbol_remove_ref( my_agent, (*p_it) );
			}
			my_agent->epmem_promotions->clear();
		}

		// add the time id to the times table
		my_agent->epmem_stmts_graph->add_time->bind_int( 1, time_counter );
		my_agent->epmem_stmts_graph->add_time->execute( soar_module::op_reinit );

		my_agent->epmem_stats->time->set_value( time_counter + 1 );

		// update time wme on all states
		{
			Symbol* state = my_agent->bottom_goal;
			Symbol* my_time_sym = make_int_constant( my_agent, time_counter + 1 );

			while ( state != NULL )
			{
				if ( state->id.epmem_time_wme != NIL )
				{
					soar_module::remove_module_wme( my_agent, state->id.epmem_time_wme );
				}

				state->id.epmem_time_wme = soar_module::add_module_wme( my_agent, state->id.epmem_header, my_agent->epmem_sym_present_id, my_time_sym );

				state = state->id.higher_goal;
			}

			symbol_remove_ref( my_agent, my_time_sym );
		}
