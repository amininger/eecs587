/*************************************************************************
 *
 *  file:  epmem_data.cpp
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

#include epmem_data.h

void initialize(epmem_data_parallel* epData)
{
    
    
    //memory pools
    init_memory_pool(epData, &(epData->epmem_literal_pool), 
		     sizeof( epmem_literal), "epmem_literals");
    init_memory_pool(epData, &(epData->epmem_pedge_pool), 
		     sizeof( epmem_pedge ), "epmem_pedges");
    init_memory_pool(epData, &(epData->epmem_uedge_pool), 
		     sizeof( epmem_uedge ), "epmem_uedges" );
    init_memory_pool(epData, &(epData->epmem_interval_pool), 
		     sizeof( epmem_interval ), "epmem_intervals" );
}

void init_epmem_db(epmem_data_parallel* epData) {
    //removed
}

void epmem_respond_to_cmd(epmem_data_parallel* epData)
{
    // if this is before the first episode, initialize db components
    if (epData->epmem_db->get_status() == soar_module::disconnected )
    {
	init_epmem_db(epData);
    }
    
    // respond to query only if db is properly initialized
    if (epData->epmem_db->get_status() != soar_module::connected )
    {
	return;
    }
    // start at the bottom and work our way up
    // (could go in the opposite direction as well)
    Symbol *state = my_agent->bottom_goal;

    epmem_wme_list *wmes;
    epmem_wme_list *cmds;
    epmem_wme_list::iterator w_p;

    soar_module::wme_set cue_wmes;
    soar_module::symbol_triple_list meta_wmes;
    soar_module::symbol_triple_list retrieval_wmes;

    epmem_time_id retrieve;
    Symbol *next;
    Symbol *previous;
    Symbol *query;
    Symbol *neg_query;
    epmem_time_list prohibit;
    epmem_time_id before, after;
#ifdef USE_MEM_POOL_ALLOCATORS
    epmem_symbol_set currents = epmem_symbol_set( std::less< Symbol* >(), soar_module::soar_memory_pool_allocator< Symbol* >( my_agent ) );
#else
    epmem_symbol_set currents = epmem_symbol_set();
#endif
    bool good_cue;
    int path;

    uint64_t wme_count;
    bool new_cue;

    bool do_wm_phase = false;

    while ( state != NULL )
    {
	my_agent->epmem_timers->api->start();
	
	// make sure this state has had some sort of change to the cmd
	new_cue = false;
	wme_count = 0;
	cmds = NULL;
	{
	    tc_number tc = get_new_tc_number( my_agent );
	    std::queue<Symbol *> syms;
	    Symbol *parent_sym;

	    // initialize BFS at command
	    syms.push( state->id.epmem_cmd_header );

	    while ( !syms.empty() )
	    {
		// get state
		parent_sym = syms.front();
		syms.pop();

		// get children of the current identifier
		wmes = epmem_get_augs_of_id( parent_sym, tc );
		{
		    for (w_p=wmes->begin(); w_p!=wmes->end(); w_p++)
		    {
			wme_count++;

			if ((*w_p)->timetag > state->id.epmem_info->last_cmd_time)
			{
			    new_cue = true;
			    state->id.epmem_info->last_cmd_time = (*w_p)->timetag;
			}

			if ( (*w_p)->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
			{
			    syms.push( (*w_p)->value );
			}
		    }

		    // free space from aug list
		    if ( cmds == NIL )
		    {
			cmds = wmes;
		    }
		    else
		    {
			delete wmes;
		    }
		}
	    }

	    // see if any WMEs were removed
	    if ( state->id.epmem_info->last_cmd_count != wme_count )
	    {
		new_cue = true;
		state->id.epmem_info->last_cmd_count = wme_count;
	    }

	    if ( new_cue )
	    {
		// clear old results
		epmem_clear_result( my_agent, state );

		do_wm_phase = true;
	    }
	}

	// a command is issued if the cue is new
	// and there is something on the cue
	if ( new_cue && wme_count )
	{
	    _epmem_respond_to_cmd_parse( my_agent, cmds, good_cue, path, retrieve, next, previous, query, neg_query, prohibit, before, after, currents, cue_wmes );

	    ////////////////////////////////////////////////////////////////////////////
	    my_agent->epmem_timers->api->stop();
	    ////////////////////////////////////////////////////////////////////////////

	    retrieval_wmes.clear();
	    meta_wmes.clear();

	    // process command
	    if ( good_cue )
	    {
		// retrieve
		if ( path == 1 )
		{
		    epmem_install_memory( my_agent, state, retrieve, meta_wmes, retrieval_wmes );
		}
		// previous or next
		else if ( path == 2 )
		{
		    if ( next )
		    {
			epmem_install_memory( my_agent, state, epmem_next_episode( my_agent, state->id.epmem_info->last_memory ), meta_wmes, retrieval_wmes );

			// add one to the next stat
			my_agent->epmem_stats->nexts->set_value( my_agent->epmem_stats->nexts->get_value() + 1 );
		    }
		    else
		    {
			epmem_install_memory( my_agent, state, epmem_previous_episode( my_agent, state->id.epmem_info->last_memory ), meta_wmes, retrieval_wmes );

			// add one to the prev stat
			my_agent->epmem_stats->prevs->set_value( my_agent->epmem_stats->prevs->get_value() + 1 );
		    }

		    if ( state->id.epmem_info->last_memory == EPMEM_MEMID_NONE )
		    {
			epmem_buffer_add_wme( meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_failure, ( ( next )?( next ):( previous ) ) );
		    }
		    else
		    {
			epmem_buffer_add_wme( meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_success, ( ( next )?( next ):( previous ) ) );
		    }
		}
		// query
		else if ( path == 3 )
		{
		    epmem_process_query( my_agent, state, query, neg_query, prohibit, before, after, currents, cue_wmes, meta_wmes, retrieval_wmes );

		    // add one to the cbr stat
		    my_agent->epmem_stats->cbr->set_value( my_agent->epmem_stats->cbr->get_value() + 1 );
		}
	    }
	    else
	    {
		epmem_buffer_add_wme( meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_status, my_agent->epmem_sym_bad_cmd );
	    }

	    // clear prohibit list
	    prohibit.clear();

	    if ( !retrieval_wmes.empty() || !meta_wmes.empty() )
	    {
		// process preference assertion en masse
		epmem_process_buffered_wmes( my_agent, state, cue_wmes, meta_wmes, retrieval_wmes );

		// clear cache
		{
		    soar_module::symbol_triple_list::iterator mw_it;

		    for ( mw_it=retrieval_wmes.begin(); mw_it!=retrieval_wmes.end(); mw_it++ )
		    {
			symbol_remove_ref( my_agent, (*mw_it)->id );
			symbol_remove_ref( my_agent, (*mw_it)->attr );
			symbol_remove_ref( my_agent, (*mw_it)->value );

			delete (*mw_it);
		    }
		    retrieval_wmes.clear();

		    for ( mw_it=meta_wmes.begin(); mw_it!=meta_wmes.end(); mw_it++ )
		    {
			symbol_remove_ref( my_agent, (*mw_it)->id );
			symbol_remove_ref( my_agent, (*mw_it)->attr );
			symbol_remove_ref( my_agent, (*mw_it)->value );

			delete (*mw_it);
		    }
		    meta_wmes.clear();
		}

		// process wm changes on this state
		do_wm_phase = true;
	    }

	    // clear cue wmes
	    cue_wmes.clear();
	}
	else
	{
	    ////////////////////////////////////////////////////////////////////
	    my_agent->epmem_timers->api->stop();
	    ////////////////////////////////////////////////////////////////////
	}

	// free space from command aug list
	if ( cmds )
	{
	    delete cmds;
	}

	state = state->id.higher_goal;
    }

    if ( do_wm_phase )
    {
	///////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->wm_phase->start();
	////////////////////////////////////////////////////////////////////////

	do_working_memory_phase( my_agent );

	///////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->wm_phase->stop();
	///////////////////////////////////////////////////////////////////////
    }

}





