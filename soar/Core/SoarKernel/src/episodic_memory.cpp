#include <portability.h>
/*************************************************************************
 * PLEASE SEE THE FILE "COPYING" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION.
 *************************************************************************/

/*************************************************************************
 *
 *  file:  episodic_memory.cpp
 *
 * =======================================================================
 * Description  :  Various functions for Soar-EpMem
 * =======================================================================
 */

#include <cmath>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <fstream>
#include <set>
#include <climits>

#include "episodic_memory.h"
#include "semantic_memory.h"
#include "extension/epmem_query.h"

#include "agent.h"
#include "prefmem.h"
#include "symtab.h"
#include "wmem.h"
#include "print.h"
#include "xml.h"
#include "instantiations.h"
#include "decide.h"

// E587: 
#ifdef USE_MPI
#include "extension/epmem_manager.h"
#include "mpi.h"
#endif

#ifdef USE_MPI
void close_workers();
#endif


#ifdef EPMEM_EXPERIMENT

uint64_t epmem_episodes_searched = 0;
uint64_t epmem_dc_interval_inserts = 0;
uint64_t epmem_dc_interval_removes = 0;
uint64_t epmem_dc_wme_adds = 0;
std::ofstream* epmem_exp_output = NULL;

enum epmem_exp_states
{
	exp_state_wm_adds,
	exp_state_wm_removes,
	exp_state_sqlite_mem,
};

int64_t epmem_exp_state[] = { 0, 0, 0 };

soar_module::timer* epmem_exp_timer = NULL;

#endif

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Bookmark strings to help navigate the code
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

// parameters	 				epmem::param
// stats 						epmem::stats
// timers 						epmem::timers
// statements					epmem::statements

// wme-related					epmem::wmes

// variable abstraction			epmem::var

// relational interval tree		epmem::rit

// cleaning up					epmem::clean
// initialization				epmem::init

// temporal hash				epmem::hash

// storing new episodes			epmem::storage
// non-cue-based queries		epmem::ncb
// cue-based queries			epmem::cbr

// vizualization				epmem::viz

// high-level api				epmem::api



//E587 JK functions
#ifdef USE_MPI
void epmem_handle_query(
    agent *my_agent, Symbol *state, Symbol *pos_query, Symbol *neg_query, 
    epmem_time_list& prohibits, epmem_time_id before, epmem_time_id after, 
    epmem_symbol_set& currents, soar_module::wme_set& cue_wmes);
void epmem_handle_search_result(Symbol *state, agent* my_agent, query_rsp_data* rsp);
void send_epmem_worker_init_data(epmem_param_container* epmem_params);
query_rsp_data* send_epmem_query_message(epmem_query* query);
#endif

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Parameter Functions (epmem::params)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

epmem_param_container::epmem_param_container( agent *new_agent ): soar_module::param_container( new_agent )
{
	// learning
	learning = new soar_module::boolean_param( "learning", soar_module::off, new soar_module::f_predicate<soar_module::boolean>() );
	add( learning );

	////////////////////
	// Encoding
	////////////////////

	// phase
	phase = new soar_module::constant_param<phase_choices>( "phase", phase_output, new soar_module::f_predicate<phase_choices>() );
	phase->add_mapping( phase_output, "output" );
	phase->add_mapping( phase_selection, "selection" );
	add( phase );

	// trigger
	trigger = new soar_module::constant_param<trigger_choices>( "trigger", output, new soar_module::f_predicate<trigger_choices>() );
	trigger->add_mapping( none, "none" );
	trigger->add_mapping( output, "output" );
	trigger->add_mapping( dc, "dc" );
	add( trigger );

	// force
	force = new soar_module::constant_param<force_choices>( "force", force_off, new soar_module::f_predicate<force_choices>() );
	force->add_mapping( remember, "remember" );
	force->add_mapping( ignore, "ignore" );
	force->add_mapping( force_off, "off" );
	add( force );

	// exclusions - this is initialized with "epmem" directly after hash tables
	exclusions = new soar_module::sym_set_param( "exclusions", new soar_module::f_predicate<const char *>, my_agent );
	add( exclusions );


	////////////////////
	// Storage
	////////////////////

	// database
	database = new soar_module::constant_param<db_choices>( "database", memory, new epmem_db_predicate<db_choices>( my_agent ) );
	database->add_mapping( memory, "memory" );
	database->add_mapping( file, "file" );
	add( database );

	// path
	path = new epmem_path_param( "path", "", new soar_module::predicate<const char *>(), new epmem_db_predicate<const char *>( my_agent ), my_agent );
	add( path );

	// auto-commit
	lazy_commit = new soar_module::boolean_param( "lazy-commit", soar_module::on, new epmem_db_predicate<soar_module::boolean>( my_agent ) );
	add( lazy_commit );


	////////////////////
	// Retrieval
	////////////////////

	// graph-match
	graph_match = new soar_module::boolean_param( "graph-match", soar_module::on, new soar_module::f_predicate<soar_module::boolean>() );
	add( graph_match );

	// balance
	balance = new soar_module::decimal_param( "balance", 1, new soar_module::btw_predicate<double>( 0, 1, true ), new soar_module::f_predicate<double>() );
	add( balance );


	////////////////////
	// Performance
	////////////////////

	// timers
	timers = new soar_module::constant_param<soar_module::timer::timer_level>( "timers", soar_module::timer::zero, new soar_module::f_predicate<soar_module::timer::timer_level>() );
	timers->add_mapping( soar_module::timer::zero, "off" );
	timers->add_mapping( soar_module::timer::one, "one" );
	timers->add_mapping( soar_module::timer::two, "two" );
	timers->add_mapping( soar_module::timer::three, "three" );
	add( timers );

	// page_size
	page_size = new soar_module::constant_param<page_choices>( "page-size", page_8k, new epmem_db_predicate<page_choices>( my_agent ) );
	page_size->add_mapping( page_1k, "1k" );
	page_size->add_mapping( page_2k, "2k" );
	page_size->add_mapping( page_4k, "4k" );
	page_size->add_mapping( page_8k, "8k" );
	page_size->add_mapping( page_16k, "16k" );
	page_size->add_mapping( page_32k, "32k" );
	page_size->add_mapping( page_64k, "64k" );
	add( page_size );

	// cache_size
	cache_size = new soar_module::integer_param( "cache-size", 10000, new soar_module::gt_predicate<int64_t>( 1, true ), new epmem_db_predicate<int64_t>( my_agent ) );
	add( cache_size );

	// opt
	opt = new soar_module::constant_param<opt_choices>( "optimization", opt_speed, new epmem_db_predicate<opt_choices>( my_agent ) );
	opt->add_mapping( opt_safety, "safety" );
	opt->add_mapping( opt_speed, "performance" );
	add( opt );


	////////////////////
	// Experimental
	////////////////////

	gm_ordering = new soar_module::constant_param<gm_ordering_choices>( "graph-match-ordering", gm_order_undefined, new soar_module::f_predicate<gm_ordering_choices>() );
	gm_ordering->add_mapping( gm_order_undefined, "undefined" );
	gm_ordering->add_mapping( gm_order_dfs, "dfs" );
	gm_ordering->add_mapping( gm_order_mcv, "mcv" );
	add( gm_ordering );

	// merge
	merge = new soar_module::constant_param<merge_choices>( "merge", merge_none, new soar_module::f_predicate<merge_choices>() );
	merge->add_mapping( merge_none, "none" );
	merge->add_mapping( merge_add, "add" );
	add( merge );
}

//

epmem_path_param::epmem_path_param( const char *new_name, const char *new_value, soar_module::predicate<const char *> *new_val_pred, soar_module::predicate<const char *> *new_prot_pred, agent *new_agent ): soar_module::string_param( new_name, new_value, new_val_pred, new_prot_pred ), my_agent( new_agent ) {}

void epmem_path_param::set_value( const char *new_value )
{
	if ( my_agent->epmem_first_switch )
	{
		my_agent->epmem_first_switch = false;
		my_agent->epmem_params->database->set_value( epmem_param_container::file );

		const char *msg = "Database set to file";
		print( my_agent, const_cast<char *>( msg ) );
		xml_generate_message( my_agent, const_cast<char *>( msg ) );
	}

	value->assign( new_value );
}

//

template <typename T>
epmem_db_predicate<T>::epmem_db_predicate( agent *new_agent ): soar_module::agent_predicate<T>( new_agent ) {}

template <typename T>
bool epmem_db_predicate<T>::operator() ( T /*val*/ ) { return ( this->my_agent->epmem_db->get_status() == soar_module::connected ); }


/***************************************************************************
 * Function     : epmem_enabled
 * Author		: Nate Derbinsky
 * Notes		: Shortcut function to system parameter
 **************************************************************************/
bool epmem_enabled( agent *my_agent )
{
	return ( my_agent->epmem_params->learning->get_value() == soar_module::on );
}

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Statistic Functions (epmem::stats)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

epmem_stat_container::epmem_stat_container( agent *new_agent ): soar_module::stat_container( new_agent )
{
	// time
	time = new epmem_time_id_stat( "time", 0, new epmem_db_predicate<epmem_time_id>( my_agent ) );
	add( time );

	// db-lib-version
	db_lib_version = new epmem_db_lib_version_stat( my_agent, "db-lib-version", NULL, new soar_module::predicate< const char* >() );
	add( db_lib_version );

	// mem-usage
	mem_usage = new epmem_mem_usage_stat( my_agent, "mem-usage", 0, new soar_module::predicate<int64_t>() );
	add( mem_usage );

	// mem-high
	mem_high = new epmem_mem_high_stat( my_agent, "mem-high", 0, new soar_module::predicate<int64_t>() );
	add( mem_high );

	// cue-based-retrievals
	cbr = new soar_module::integer_stat( "queries", 0, new soar_module::f_predicate<int64_t>() );
	add( cbr );

	// nexts
	nexts = new soar_module::integer_stat( "nexts", 0, new soar_module::f_predicate<int64_t>() );
	add( nexts );

	// prev's
	prevs = new soar_module::integer_stat( "prevs", 0, new soar_module::f_predicate<int64_t>() );
	add( prevs );

	// ncb-wmes
	ncb_wmes = new soar_module::integer_stat( "ncb-wmes", 0, new soar_module::f_predicate<int64_t>() );
	add( ncb_wmes );

	// qry-pos
	qry_pos = new soar_module::integer_stat( "qry-pos", 0, new soar_module::f_predicate<int64_t>() );
	add( qry_pos );

	// qry-neg
	qry_neg = new soar_module::integer_stat( "qry-neg", 0, new soar_module::f_predicate<int64_t>() );
	add( qry_neg );

	// qry-ret
	qry_ret = new epmem_time_id_stat( "qry-ret", 0, new soar_module::f_predicate<epmem_time_id>() );
	add( qry_ret );

	// qry-card
	qry_card = new soar_module::integer_stat( "qry-card", 0, new soar_module::f_predicate<int64_t>() );
	add( qry_card );

	// qry-lits
	qry_lits = new soar_module::integer_stat( "qry-lits", 0, new soar_module::f_predicate<int64_t>() );
	add( qry_lits );

	// next-id
	next_id = new epmem_node_id_stat( "next-id", 0, new epmem_db_predicate<epmem_node_id>( my_agent ) );
	add( next_id );

	// rit-offset-1
	rit_offset_1 = new soar_module::integer_stat( "rit-offset-1", 0, new epmem_db_predicate<int64_t>( my_agent ) );
	add( rit_offset_1 );

	// rit-left-root-1
	rit_left_root_1 = new soar_module::integer_stat( "rit-left-root-1", 0, new epmem_db_predicate<int64_t>( my_agent ) );
	add( rit_left_root_1 );

	// rit-right-root-1
	rit_right_root_1 = new soar_module::integer_stat( "rit-right-root-1", 0, new epmem_db_predicate<int64_t>( my_agent ) );
	add( rit_right_root_1 );

	// rit-min-step-1
	rit_min_step_1 = new soar_module::integer_stat( "rit-min-step-1", 0, new epmem_db_predicate<int64_t>( my_agent ) );
	add( rit_min_step_1 );

	// rit-offset-2
	rit_offset_2 = new soar_module::integer_stat( "rit-offset-2", 0, new epmem_db_predicate<int64_t>( my_agent ) );
	add( rit_offset_2 );

	// rit-left-root-2
	rit_left_root_2 = new soar_module::integer_stat( "rit-left-root-2", 0, new epmem_db_predicate<int64_t>( my_agent ) );
	add( rit_left_root_2 );

	// rit-right-root-2
	rit_right_root_2 = new soar_module::integer_stat( "rit-right-root-2", 0, new epmem_db_predicate<int64_t>( my_agent ) );
	add( rit_right_root_2 );

	// rit-min-step-2
	rit_min_step_2 = new soar_module::integer_stat( "rit-min-step-2", 0, new epmem_db_predicate<int64_t>( my_agent ) );
	add( rit_min_step_2 );
}

//

epmem_db_lib_version_stat::epmem_db_lib_version_stat( agent* new_agent, const char* new_name, const char* new_value, soar_module::predicate< const char* >* new_prot_pred ): soar_module::primitive_stat< const char* >( new_name, new_value, new_prot_pred ), my_agent( new_agent ) {}

const char* epmem_db_lib_version_stat::get_value()
{
	return my_agent->epmem_db->lib_version();
}

//

epmem_mem_usage_stat::epmem_mem_usage_stat( agent *new_agent, const char *new_name, int64_t new_value, soar_module::predicate<int64_t> *new_prot_pred ): soar_module::integer_stat( new_name, new_value, new_prot_pred ), my_agent( new_agent ) {}

int64_t epmem_mem_usage_stat::get_value()
{
	return my_agent->epmem_db->memory_usage();
}

//

epmem_mem_high_stat::epmem_mem_high_stat( agent *new_agent, const char *new_name, int64_t new_value, soar_module::predicate<int64_t> *new_prot_pred ): soar_module::integer_stat( new_name, new_value, new_prot_pred ), my_agent( new_agent ) {}

int64_t epmem_mem_high_stat::get_value()
{
	return my_agent->epmem_db->memory_highwater();

}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Timer Functions (epmem::timers)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

epmem_timer_container::epmem_timer_container( agent *new_agent ): soar_module::timer_container( new_agent )
{
	// one

	total = new epmem_timer( "_total", my_agent, soar_module::timer::one );
	add( total );

	// two

	storage = new epmem_timer( "epmem_storage", my_agent, soar_module::timer::two );
	add( storage );

	ncb_retrieval = new epmem_timer( "epmem_ncb_retrieval", my_agent, soar_module::timer::two );
	add( ncb_retrieval );

	query = new epmem_timer( "epmem_query", my_agent, soar_module::timer::two );
	add( query );

	api = new epmem_timer( "epmem_api", my_agent, soar_module::timer::two );
	add( api );

	trigger = new epmem_timer( "epmem_trigger", my_agent, soar_module::timer::two );
	add( trigger );

	init = new epmem_timer( "epmem_init", my_agent, soar_module::timer::two );
	add( init );

	next = new epmem_timer( "epmem_next", my_agent, soar_module::timer::two );
	add( next );

	prev = new epmem_timer( "epmem_prev", my_agent, soar_module::timer::two );
	add( prev );

	hash = new epmem_timer( "epmem_hash", my_agent, soar_module::timer::two );
	add( hash );

	wm_phase = new epmem_timer( "epmem_wm_phase", my_agent, soar_module::timer::two );
	add( wm_phase );

	// three

	ncb_edge = new epmem_timer( "ncb_edge", my_agent, soar_module::timer::three );
	add( ncb_edge );

	ncb_edge_rit = new epmem_timer( "ncb_edge_rit", my_agent, soar_module::timer::three );
	add( ncb_edge_rit );

	ncb_node = new epmem_timer( "ncb_node", my_agent, soar_module::timer::three );
	add( ncb_node );

	ncb_node_rit = new epmem_timer( "ncb_node_rit", my_agent, soar_module::timer::three );
	add( ncb_node_rit );

	query_dnf = new epmem_timer( "query_dnf", my_agent, soar_module::timer::three );
	add( query_dnf );

	query_walk = new epmem_timer( "query_walk", my_agent, soar_module::timer::three );
	add( query_walk );

	query_walk_edge = new epmem_timer( "query_walk_edge", my_agent, soar_module::timer::three );
	add( query_walk_edge );

	query_walk_interval = new epmem_timer( "query_walk_interval", my_agent, soar_module::timer::three );
	add( query_walk_interval );

	query_graph_match = new epmem_timer( "query_graph_match", my_agent, soar_module::timer::three );
	add( query_graph_match );

	query_result = new epmem_timer( "query_result", my_agent, soar_module::timer::three );
	add( query_result );

	query_cleanup = new epmem_timer( "query_cleanup", my_agent, soar_module::timer::three );
	add( query_cleanup );

	query_sql_edge = new epmem_timer( "query_sql_edge", my_agent, soar_module::timer::three );
	add( query_sql_edge );

	query_sql_start_ep = new epmem_timer( "query_sql_start_ep", my_agent, soar_module::timer::three );
	add( query_sql_start_ep );

	query_sql_start_now = new epmem_timer( "query_sql_start_now", my_agent, soar_module::timer::three );
	add( query_sql_start_now );

	query_sql_start_point = new epmem_timer( "query_sql_start_point", my_agent, soar_module::timer::three );
	add( query_sql_start_point );

	query_sql_end_ep = new epmem_timer( "query_sql_end_ep", my_agent, soar_module::timer::three );
	add( query_sql_end_ep );

	query_sql_end_now = new epmem_timer( "query_sql_end_now", my_agent, soar_module::timer::three );
	add( query_sql_end_now );

	query_sql_end_point = new epmem_timer( "query_sql_end_point", my_agent, soar_module::timer::three );
	add( query_sql_end_point );

}

//

epmem_timer_level_predicate::epmem_timer_level_predicate( agent *new_agent ): soar_module::agent_predicate<soar_module::timer::timer_level>( new_agent ) {}

bool epmem_timer_level_predicate::operator() ( soar_module::timer::timer_level val ) { return ( my_agent->epmem_params->timers->get_value() >= val ); }

//

epmem_timer::epmem_timer(const char *new_name, agent *new_agent, soar_module::timer::timer_level new_level): soar_module::timer( new_name, new_agent, new_level, new epmem_timer_level_predicate( new_agent ) ) {}




//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// WME Functions (epmem::wmes)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_get_augs_of_id
 * Author		: Nate Derbinsky
 * Notes		: This routine gets all wmes rooted at an id.
 **************************************************************************/
epmem_wme_list *epmem_get_augs_of_id( Symbol * id, tc_number tc )
{
	slot *s;
	wme *w;
	epmem_wme_list *return_val = new epmem_wme_list;

	// augs only exist for identifiers
	if ( ( id->common.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&
			( id->id.tc_num != tc ) )
	{
		id->id.tc_num = tc;

		// impasse wmes
		for ( w=id->id.impasse_wmes; w!=NIL; w=w->next )
		{
			return_val->push_back( w );
		}

		// input wmes
		for ( w=id->id.input_wmes; w!=NIL; w=w->next )
		{
			return_val->push_back( w );
		}

		// regular wmes
		for ( s=id->id.slots; s!=NIL; s=s->next )
		{
			for ( w=s->wmes; w!=NIL; w=w->next )
			{
				return_val->push_back( w );
			}

			for ( w=s->acceptable_preference_wmes; w!=NIL; w=w->next )
			{
				return_val->push_back( w );
			}
		}
	}

	return return_val;
}

inline void _epmem_process_buffered_wme_list( agent* my_agent, Symbol* state, soar_module::wme_set& cue_wmes, soar_module::symbol_triple_list& my_list, epmem_wme_stack* epmem_wmes )
{
	if ( my_list.empty() )
	{
		return;
	}

	instantiation* inst = soar_module::make_fake_instantiation( my_agent, state, &cue_wmes, &my_list );

	for ( preference* pref=inst->preferences_generated; pref; pref=pref->inst_next )
	{
		// add the preference to temporary memory
		if ( add_preference_to_tm( my_agent, pref ) )
		{
			// add to the list of preferences to be removed
			// when the goal is removed
			insert_at_head_of_dll( state->id.preferences_from_goal, pref, all_of_goal_next, all_of_goal_prev );
			pref->on_goal_list = true;

			if ( epmem_wmes )
			{
				// if this is a meta wme, then it is completely local
				// to the state and thus we will manually remove it
				// (via preference removal) when the time comes
				epmem_wmes->push_back( pref );
			}
		}
		else
		{
			preference_add_ref( pref );
			preference_remove_ref( my_agent, pref );
		}
	}

	if ( !epmem_wmes )
	{
		// otherwise, we submit the fake instantiation to backtracing
		// such as to potentially produce justifications that can follow
		// it to future adventures (potentially on new states)
		instantiation *my_justification_list = NIL;
		chunk_instantiation( my_agent, inst, false, &my_justification_list );

		// if any justifications are created, assert their preferences manually
		// (copied mainly from assert_new_preferences with respect to our circumstances)
		if ( my_justification_list != NIL )
		{
			preference *just_pref = NIL;
			instantiation *next_justification = NIL;

			for ( instantiation *my_justification=my_justification_list;
					my_justification!=NIL;
					my_justification=next_justification )
			{
				next_justification = my_justification->next;

				if ( my_justification->in_ms )
				{
					insert_at_head_of_dll( my_justification->prod->instantiations, my_justification, next, prev );
				}

				for ( just_pref=my_justification->preferences_generated; just_pref!=NIL; just_pref=just_pref->inst_next )
				{
					if ( add_preference_to_tm( my_agent, just_pref ) )
					{
						if ( wma_enabled( my_agent ) )
						{
							wma_activate_wmes_in_pref( my_agent, just_pref );
						}
					}
					else
					{
						preference_add_ref( just_pref );
						preference_remove_ref( my_agent, just_pref );
					}
				}
			}
		}
	}
}

inline void epmem_process_buffered_wmes( agent* my_agent, Symbol* state, soar_module::wme_set& cue_wmes, soar_module::symbol_triple_list& meta_wmes, soar_module::symbol_triple_list& retrieval_wmes )
{
	_epmem_process_buffered_wme_list( my_agent, state, cue_wmes, meta_wmes, state->id.epmem_info->epmem_wmes );
	_epmem_process_buffered_wme_list( my_agent, state, cue_wmes, retrieval_wmes, NULL );
}

inline void epmem_buffer_add_wme( soar_module::symbol_triple_list& my_list, Symbol* id, Symbol* attr, Symbol* value )
{
	my_list.push_back( new soar_module::symbol_triple( id, attr, value ) );

	symbol_add_ref( id );
	symbol_add_ref( attr );
	symbol_add_ref( value );
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
bool epmem_get_variable( agent *my_agent, epmem_variable_key variable_id, int64_t *variable_value )
{
	soar_module::exec_result status;
	// E587: AM:
	soar_module::sqlite_statement *var_get = my_agent->epmem_stmts_master->var_get;

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
void epmem_set_variable( agent *my_agent, epmem_variable_key variable_id, int64_t variable_value )
{
	// E587: AM:
	soar_module::sqlite_statement *var_set = my_agent->epmem_stmts_master->var_set;

	var_set->bind_int( 1, variable_id );
	var_set->bind_int( 2, variable_value );

	var_set->execute( soar_module::op_reinit );
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Clean-Up Functions (epmem::clean)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_close
 * Author		: Nate Derbinsky
 * Notes		: Performs cleanup operations when the database needs
 * 				  to be closed (end soar, manual close, etc)
 **************************************************************************/
void epmem_close( agent *my_agent )
{

	// E587: AM:
	if ( my_agent->epmem_db->get_status() == soar_module::connected )
	{
#ifndef USE_MPI
		my_agent->epmem_worker_p->close();
		delete my_agent->epmem_worker_p;
#else
		close_workers();
#endif

		// if lazy, commit
		if ( my_agent->epmem_params->lazy_commit->get_value() == soar_module::on )
		{
			my_agent->epmem_stmts_master->commit->execute( soar_module::op_reinit );
		}

		// de-allocate local data structures
		{
			epmem_parent_id_pool::iterator p;
			epmem_hashed_id_pool::iterator p_p;

			for ( p=my_agent->epmem_id_repository->begin(); p!=my_agent->epmem_id_repository->end(); p++ )
			{
				for ( p_p=p->second->begin(); p_p!=p->second->end(); p_p++ )
				{
					delete p_p->second;
				}

				delete p->second;
			}

			my_agent->epmem_id_repository->clear();
			my_agent->epmem_id_replacement->clear();
			for ( epmem_id_ref_counter::iterator rf_it=my_agent->epmem_id_ref_counts->begin(); rf_it!=my_agent->epmem_id_ref_counts->end(); rf_it++ )
			{
				delete rf_it->second;
			}
			my_agent->epmem_id_ref_counts->clear();

			my_agent->epmem_wme_adds->clear();

			for ( epmem_symbol_set::iterator p_it=my_agent->epmem_promotions->begin(); p_it!=my_agent->epmem_promotions->end(); p_it++ )
			{
				symbol_remove_ref( my_agent, (*p_it) );
			}
			my_agent->epmem_promotions->clear();
		}

		delete my_agent->epmem_stmts_master;
		my_agent->epmem_db->disconnect();
	}

#ifdef EPMEM_EXPERIMENT
	if ( epmem_exp_output )
	{
		epmem_exp_output->close();
		delete epmem_exp_output;
		epmem_exp_output = NULL;

		if ( epmem_exp_timer )
		{
			delete epmem_exp_timer;
		}
	}
#endif
}

/***************************************************************************
 * Function     : epmem_clear_result
 * Author		: Nate Derbinsky
 * Notes		: Removes any WMEs produced by EpMem resulting from
 * 				  a command
 **************************************************************************/
void epmem_clear_result( agent *my_agent, Symbol *state )
{
	preference *pref;

	while ( !state->id.epmem_info->epmem_wmes->empty() )
	{
		pref = state->id.epmem_info->epmem_wmes->back();
		state->id.epmem_info->epmem_wmes->pop_back();

		if ( pref->in_tm )
		{
			remove_preference_from_tm( my_agent, pref );
		}
	}
}

/***************************************************************************
 * Function     : epmem_reset
 * Author		: Nate Derbinsky
 * Notes		: Performs cleanup when a state is removed
 **************************************************************************/
void epmem_reset( agent *my_agent, Symbol *state )
{
	if ( state == NULL )
	{
		state = my_agent->top_goal;
	}

	while( state )
	{
		epmem_data *data = state->id.epmem_info;

		data->last_ol_time = 0;

		data->last_cmd_time = 0;
		data->last_cmd_count = 0;

		data->last_memory = EPMEM_MEMID_NONE;

		// this will be called after prefs from goal are already removed,
		// so just clear out result stack
		data->epmem_wmes->clear();

		state = state->id.lower_goal;
	}
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Initialization Functions (epmem::init)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_init_db
 * Author		: Nate Derbinsky
 * Notes		: Opens the SQLite database and performs all
 * 				  initialization required for the current mode
 *
 *                The readonly param should only be used in
 *                experimentation where you don't want to alter
 *                previous database state.
 **************************************************************************/
void epmem_init_db( agent *my_agent, bool readonly = false )
{
	// E587: AM:
	if ( my_agent->epmem_db->get_status() != soar_module::disconnected )
	{
		return;
	}

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->init->start();
	////////////////////////////////////////////////////////////////////////////

	const char *db_path;
	if ( my_agent->epmem_params->database->get_value() == epmem_param_container::memory )
	{
		db_path = ":memory:";
	}
	else
	{
		db_path = my_agent->epmem_params->path->get_value();
	}

	// attempt connection
	my_agent->epmem_db->connect( db_path );

	if ( my_agent->epmem_db->get_status() == soar_module::problem )
	{
		char buf[256];
		SNPRINTF( buf, 254, "DB ERROR: %s", my_agent->epmem_db->get_errmsg() );

		print( my_agent, buf );
		xml_generate_warning( my_agent, buf );
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
				switch ( my_agent->epmem_params->page_size->get_value() )
				{
					case ( epmem_param_container::page_1k ):
						temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA page_size = 1024" );
						break;

					case ( epmem_param_container::page_2k ):
						temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA page_size = 2048" );
						break;

					case ( epmem_param_container::page_4k ):
						temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA page_size = 4096" );
						break;

					case ( epmem_param_container::page_8k ):
						temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA page_size = 8192" );
						break;

					case ( epmem_param_container::page_16k ):
						temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA page_size = 16384" );
						break;

					case ( epmem_param_container::page_32k ):
						temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA page_size = 32768" );
						break;

					case ( epmem_param_container::page_64k ):
						temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA page_size = 65536" );
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
				char* str = my_agent->epmem_params->cache_size->get_string();
				cache_sql.append( str );
				free(str);
				str = NULL;

				temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, cache_sql.c_str() );

				temp_q->prepare();
				temp_q->execute();
				delete temp_q;
				temp_q = NULL;
			}

			// optimization
			if ( my_agent->epmem_params->opt->get_value() == epmem_param_container::opt_speed )
			{
				// synchronous - don't wait for writes to complete (can corrupt the db in case unexpected crash during transaction)
				temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA synchronous = OFF" );
				temp_q->prepare();
				temp_q->execute();
				delete temp_q;
				temp_q = NULL;

				// journal_mode - no atomic transactions (can result in database corruption if crash during transaction)
				temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA journal_mode = OFF" );
				temp_q->prepare();
				temp_q->execute();
				delete temp_q;
				temp_q = NULL;

				// locking_mode - no one else can view the database after our first write
				temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "PRAGMA locking_mode = EXCLUSIVE" );
				temp_q->prepare();
				temp_q->execute();
				delete temp_q;
				temp_q = NULL;
			}
		}

		// point stuff
		epmem_time_id range_start;
		epmem_time_id time_last;

		// update validation count
		my_agent->epmem_validation++;

#ifdef USE_MPI
		send_epmem_worker_init_data(my_agent->epmem_params);
#else
		my_agent->epmem_worker_p = new epmem_worker();
		my_agent->epmem_worker_p->initialize(my_agent->epmem_params);
        my_agent->epmem_worker_p2 = new epmem_worker();
        my_agent->epmem_worker_p2->initialize(my_agent->epmem_params);
#endif

		my_agent->epmem_stmts_master = new epmem_master_statement_container(my_agent);
		my_agent->epmem_stmts_master->structure();
		my_agent->epmem_stmts_master->prepare();

//		// setup common structures/queries
//		my_agent->epmem_stmts_common = new epmem_common_statement_container( my_agent );
//		my_agent->epmem_stmts_common->structure();
//		my_agent->epmem_stmts_common->prepare();
//
//		// setup graph structures/queries
//		my_agent->epmem_stmts_graph = new epmem_graph_statement_container( my_agent );
//		my_agent->epmem_stmts_graph->structure();
//		my_agent->epmem_stmts_graph->prepare();
			{

			// initialize range tracking
			my_agent->epmem_node_mins->clear();
			my_agent->epmem_node_maxes->clear();
			my_agent->epmem_node_removals->clear();

			my_agent->epmem_edge_mins->clear();
			my_agent->epmem_edge_maxes->clear();
			my_agent->epmem_edge_removals->clear();

			(*my_agent->epmem_id_repository)[ EPMEM_NODEID_ROOT ] = new epmem_hashed_id_pool;
			{
#ifdef USE_MEM_POOL_ALLOCATORS
				epmem_wme_set* wms_temp = new epmem_wme_set( std::less< wme* >(), soar_module::soar_memory_pool_allocator< wme* >( my_agent ) );
#else
				epmem_wme_set* wms_temp = new epmem_wme_set();
#endif

				// voigtjr: Cast to wme* is necessary for compilation in VS10
				// Without it, it picks insert(int) instead of insert(wme*) and does not compile.
				wms_temp->insert( static_cast<wme*>(NULL) );

				(*my_agent->epmem_id_ref_counts)[ EPMEM_NODEID_ROOT ] = wms_temp;
			}

			// initialize time
			my_agent->epmem_stats->time->set_value( 1 );

			// initialize next_id
			my_agent->epmem_stats->next_id->set_value( 1 );
			{
				int64_t stored_id = NIL;
				if ( epmem_get_variable( my_agent, var_next_id, &stored_id ) )
				{
					my_agent->epmem_stats->next_id->set_value( stored_id );
				}
				else
				{
					epmem_set_variable( my_agent, var_next_id, my_agent->epmem_stats->next_id->get_value() );
				}
			}

			////

			// get max time
			{
				temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "SELECT MAX(id) FROM times" );
				temp_q->prepare();
				if ( temp_q->execute() == soar_module::row )
					my_agent->epmem_stats->time->set_value( temp_q->column_int( 0 ) + 1 );

				delete temp_q;
				temp_q = NULL;
			}
			time_max = my_agent->epmem_stats->time->get_value();

			// insert non-NOW intervals for all current NOW's
			// remove NOW's
			if ( !readonly )
			{
				// E583: AM: XXX: For this project we'll ignore loading existing databases
				//time_last = ( time_max - 1 );

				//const char *now_select[] = { "SELECT id,start FROM node_now", "SELECT id,start FROM edge_now" };
				//// E587: AM:
				//soar_module::sqlite_statement *now_add[] = { my_agent->epmem_worker_p->epmem_stmts_graph->add_node_point, my_agent->epmem_worker_p->epmem_stmts_graph->add_edge_point };
				//const char *now_delete[] = { "DELETE FROM node_now", "DELETE FROM edge_now" };

				//for ( int i=EPMEM_RIT_STATE_NODE; i<=EPMEM_RIT_STATE_EDGE; i++ )
				//{
				//	temp_q = now_add[i];
				//	temp_q->bind_int( 2, time_last );

				//	temp_q2 = new soar_module::sqlite_statement( my_agent->epmem_db, now_select[i] );
				//	temp_q2->prepare();
				//	while ( temp_q2->execute() == soar_module::row )
				//	{
				//		range_start = temp_q2->column_int( 1 );

				//		// point
				//		if ( range_start == time_last )
				//		{
				//			temp_q->bind_int( 1, temp_q2->column_int( 0 ) );
				//			temp_q->execute( soar_module::op_reinit );
				//		}
				//		else
				//		{
				//			epmem_rit_insert_interval( my_agent, range_start, time_last, temp_q2->column_int( 0 ), &( my_agent->epmem_rit_state_graph[i] ) );
				//		}

				//		if ( i == EPMEM_RIT_STATE_EDGE)
				//		{
				//			// E587: AM:
				//			my_agent->epmem_worker_p->epmem_stmts_graph->update_edge_unique_last->bind_int( 1, time_last );
				//			my_agent->epmem_worker_p->epmem_stmts_graph->update_edge_unique_last->bind_int( 2, temp_q2->column_int( 0 ) );
				//			my_agent->epmem_worker_p->epmem_stmts_graph->update_edge_unique_last->execute( soar_module::op_reinit );
				//		}
				//	}
				//	delete temp_q2;
				//	temp_q2 = NULL;
				//	temp_q = NULL;


				//	// remove all NOW intervals
				//	temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, now_delete[i] );
				//	temp_q->prepare();
				//	temp_q->execute();
				//	delete temp_q;
				//	temp_q = NULL;
				//}
			}

			// get max id + max list
			{
				const char *minmax_select[] = { "SELECT MAX(child_id) FROM node_unique", "SELECT MAX(parent_id) FROM edge_unique" };
				std::vector<bool> *minmax_max[] = { my_agent->epmem_node_maxes, my_agent->epmem_edge_maxes };
				std::vector<epmem_time_id> *minmax_min[] = { my_agent->epmem_node_mins, my_agent->epmem_edge_mins };

				for ( int i=EPMEM_RIT_STATE_NODE; i<=EPMEM_RIT_STATE_EDGE; i++ )
				{
					temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, minmax_select[i] );
					temp_q->prepare();
					temp_q->execute();
					if ( temp_q->column_type( 0 ) != soar_module::null_t )
					{
						std::vector<bool>::size_type num_ids = temp_q->column_int( 0 );

						minmax_max[i]->resize( num_ids, true );
						minmax_min[i]->resize( num_ids, time_max );
					}

					delete temp_q;
					temp_q = NULL;
				}
			}

			// get id pools
			{
				epmem_node_id q0;
				int64_t w;
				epmem_node_id q1;
				epmem_node_id parent_id;

				epmem_hashed_id_pool **hp;
				epmem_id_pool **ip;

				temp_q = new soar_module::sqlite_statement( my_agent->epmem_db, "SELECT q0, w, q1, parent_id FROM edge_unique" );
				temp_q->prepare();

				while ( temp_q->execute() == soar_module::row )
				{
					q0 = temp_q->column_int( 0 );
					w = temp_q->column_int( 1 );
					q1 = temp_q->column_int( 2 );
					parent_id = temp_q->column_int( 3 );

					hp =& (*my_agent->epmem_id_repository)[ q0 ];
					if ( !(*hp) )
						(*hp) = new epmem_hashed_id_pool;

					ip =& (*(*hp))[ w ];
					if ( !(*ip) )
						(*ip) = new epmem_id_pool;

					(*ip)->push_front( std::make_pair( q1, parent_id ) );

					hp =& (*my_agent->epmem_id_repository)[ q1 ];
					if ( !(*hp) )
						(*hp) = new epmem_hashed_id_pool;
				}

				delete temp_q;
				temp_q = NULL;
			}

			// at init, top-state is considered the only known identifier
			my_agent->top_goal->id.epmem_id = EPMEM_NODEID_ROOT;
			my_agent->top_goal->id.epmem_valid = my_agent->epmem_validation;

			// capture augmentations of top-state as the sole set of adds,
			// which catches up to what would have been incremental encoding
			// to this point
			{
				my_agent->epmem_wme_adds->insert( my_agent->top_state );
			}
		}

		// if lazy commit, then we encapsulate the entire lifetime of the agent in a single transaction
		if ( my_agent->epmem_params->lazy_commit->get_value() == soar_module::on )
		{
			my_agent->epmem_stmts_master->begin->execute( soar_module::op_reinit );
		}
	}

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->init->stop();
	////////////////////////////////////////////////////////////////////////////
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Temporal Hash Functions (epmem::hash)
//
// The rete has symbol hashing, but the values are
// reliable only for the lifetime of a symbol.  This
// isn't good for EpMem.  Hence, we implement a simple
// lookup table, relying upon SQLite to deal with
// efficiency issues.
//
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_temporal_hash
 * Author		: Nate Derbinsky
 * Notes		: Returns a temporally unique integer representing
 *                a symbol constant.
 **************************************************************************/
epmem_hash_id epmem_temporal_hash( agent *my_agent, Symbol *sym, bool add_on_fail = true )
{
	epmem_hash_id return_val = NIL;

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->hash->start();
	////////////////////////////////////////////////////////////////////////////

	if ( ( sym->common.symbol_type == SYM_CONSTANT_SYMBOL_TYPE ) ||
			( sym->common.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) ||
			( sym->common.symbol_type == FLOAT_CONSTANT_SYMBOL_TYPE ) )
	{
		if ( ( !sym->common.epmem_hash ) || ( sym->common.epmem_valid != my_agent->epmem_validation ) )
		{
			sym->common.epmem_hash = NIL;
			sym->common.epmem_valid = my_agent->epmem_validation;

			// basic process:
			// - search
			// - if found, return
			// - else, add

			// E587: AM:
			my_agent->epmem_stmts_master->hash_get->bind_int( 1, sym->common.symbol_type );
			switch ( sym->common.symbol_type )
			{
				case SYM_CONSTANT_SYMBOL_TYPE:
					my_agent->epmem_stmts_master->hash_get->bind_text( 2, static_cast<const char *>( sym->sc.name ) );
					break;

				case INT_CONSTANT_SYMBOL_TYPE:
					my_agent->epmem_stmts_master->hash_get->bind_int( 2, sym->ic.value );
					break;

				case FLOAT_CONSTANT_SYMBOL_TYPE:
					my_agent->epmem_stmts_master->hash_get->bind_double( 2, sym->fc.value );
					break;
			}

			if ( my_agent->epmem_stmts_master->hash_get->execute() == soar_module::row )
			{
				return_val = static_cast<epmem_hash_id>( my_agent->epmem_stmts_master->hash_get->column_int( 0 ) );
			}

			my_agent->epmem_stmts_master->hash_get->reinitialize();

			//

			if ( !return_val && add_on_fail )
			{
				// E587: AM:
				my_agent->epmem_stmts_master->hash_add->bind_int( 1, sym->common.symbol_type );
				switch ( sym->common.symbol_type )
				{
					case SYM_CONSTANT_SYMBOL_TYPE:
						my_agent->epmem_stmts_master->hash_add->bind_text( 2, static_cast<const char *>( sym->sc.name ) );
						break;

					case INT_CONSTANT_SYMBOL_TYPE:
						my_agent->epmem_stmts_master->hash_add->bind_int( 2, sym->ic.value );
						break;

					case FLOAT_CONSTANT_SYMBOL_TYPE:
						my_agent->epmem_stmts_master->hash_add->bind_double( 2, sym->fc.value );
						break;
				}

				my_agent->epmem_stmts_master->hash_add->execute( soar_module::op_reinit );
				return_val = static_cast<epmem_hash_id>( my_agent->epmem_db->last_insert_rowid() );
			}

			// cache results for later re-use
			sym->common.epmem_hash = return_val;
			sym->common.epmem_valid = my_agent->epmem_validation;
		}

		return_val = sym->common.epmem_hash;
	}

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->hash->stop();
	////////////////////////////////////////////////////////////////////////////

	return return_val;
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Storage Functions (epmem::storage)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

void epmem_schedule_promotion( agent* my_agent, Symbol* id )
{
	if ( my_agent->epmem_db->get_status() == soar_module::connected )
	{
		if ( ( id->id.epmem_id != EPMEM_NODEID_BAD ) && ( id->id.epmem_valid == my_agent->epmem_validation ) )
		{
			symbol_add_ref( id );
			my_agent->epmem_promotions->insert( id );
		}
	}
}

inline void _epmem_promote_id( agent* my_agent, Symbol* id, epmem_time_id t )
{
	// E587: AM: XXX: Need to notify the worker too
	// parent_id,letter,num,time_id
	my_agent->epmem_stmts_master->promote_id->bind_int( 1, id->id.epmem_id );
	my_agent->epmem_stmts_master->promote_id->bind_int( 2, static_cast<uint64_t>( id->id.name_letter ) );
	my_agent->epmem_stmts_master->promote_id->bind_int( 3, static_cast<uint64_t>( id->id.name_number ) );
	my_agent->epmem_stmts_master->promote_id->bind_int( 4, t );
	my_agent->epmem_stmts_master->promote_id->execute( soar_module::op_reinit );
}

// three cases for sharing ids amongst identifiers in two passes:
// 1. value known in phase one (try reservation)
// 2. value unknown in phase one, but known at phase two (try assignment adhering to constraint)
// 3. value unknown in phase one/two (if anything is left, unconstrained assignment)
inline void _epmem_store_level( agent* my_agent, std::queue< Symbol* >& parent_syms, std::queue< epmem_node_id >& parent_ids, tc_number tc, epmem_wme_list::iterator w_b, epmem_wme_list::iterator w_e, epmem_node_id parent_id, epmem_time_id time_counter,
		std::map< wme*, epmem_id_reservation* >& id_reservations, std::set< Symbol* >& new_identifiers, std::queue< epmem_node_id >& epmem_node, std::queue< epmem_node_id >& epmem_edge )
{
	epmem_wme_list::iterator w_p;
	bool value_known_apriori = false;

	// temporal hash
	epmem_hash_id my_hash;	// attribute
	epmem_hash_id my_hash2;	// value

	// id repository
	epmem_id_pool **my_id_repo;
	epmem_id_pool *my_id_repo2;
	epmem_id_pool::iterator pool_p;
	std::map<wme *, epmem_id_reservation *>::iterator r_p;
	epmem_id_reservation *new_id_reservation;

	// identifier recursion
	epmem_wme_list* wmes = NULL;
	epmem_wme_list::iterator w_p2;
	bool good_recurse = false;

	// find WME ID for WMEs whose value is an identifier and has a known epmem id (prevents ordering issues with unknown children)
	for ( w_p=w_b; w_p!=w_e; w_p++ )
	{
		// skip over WMEs already in the system
		if ( ( (*w_p)->epmem_id != EPMEM_NODEID_BAD ) && ( (*w_p)->epmem_valid == my_agent->epmem_validation ) )
		{
			continue;
		}

		if ( ( (*w_p)->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&
				( ( (*w_p)->value->id.epmem_id != EPMEM_NODEID_BAD ) && ( (*w_p)->value->id.epmem_valid == my_agent->epmem_validation ) ) &&
				( !(*w_p)->value->id.smem_lti ) )
		{
			// prevent exclusions from being recorded
			if ( my_agent->epmem_params->exclusions->in_set( (*w_p)->attr ) )
			{
				continue;
			}

			// if still here, create reservation (case 1)
			new_id_reservation = new epmem_id_reservation;
			new_id_reservation->my_id = EPMEM_NODEID_BAD;
			new_id_reservation->my_pool = NULL;

			if ( (*w_p)->acceptable )
			{
				new_id_reservation->my_hash = EPMEM_HASH_ACCEPTABLE;
			}
			else
			{
				new_id_reservation->my_hash = epmem_temporal_hash( my_agent, (*w_p)->attr );
			}

			// try to find appropriate reservation
			my_id_repo =& (*(*my_agent->epmem_id_repository)[ parent_id ])[ new_id_reservation->my_hash ];
			if ( (*my_id_repo) )
			{
				for ( pool_p = (*my_id_repo)->begin(); pool_p != (*my_id_repo)->end(); pool_p++ )
				{
					if ( pool_p->first == (*w_p)->value->id.epmem_id )
					{
						new_id_reservation->my_id = pool_p->second;
						(*my_id_repo)->erase( pool_p );
						break;
					}
				}
			}
			else
			{
				// add repository
				(*my_id_repo) = new epmem_id_pool;
			}

			new_id_reservation->my_pool = (*my_id_repo);
			id_reservations[ (*w_p) ] = new_id_reservation;
			new_id_reservation = NULL;
		}
	}

	for ( w_p=w_b; w_p!=w_e; w_p++ )
	{
		// skip over WMEs already in the system
		if ( ( (*w_p)->epmem_id != EPMEM_NODEID_BAD ) && ( (*w_p)->epmem_valid == my_agent->epmem_validation ) )
		{
			continue;
		}

		// prevent exclusions from being recorded
		if ( my_agent->epmem_params->exclusions->in_set( (*w_p)->attr ) )
		{
			continue;
		}

		if ( (*w_p)->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
		{
			(*w_p)->epmem_valid = my_agent->epmem_validation;
			(*w_p)->epmem_id = EPMEM_NODEID_BAD;

			my_hash = NIL;
			my_id_repo2 = NIL;

			// if the value already has an epmem_id, the WME ID would have already been assigned above (ie. the epmem_id of the VALUE is KNOWN APRIORI [sic])
			// however, it's also possible that the value is known but no WME ID is given (eg. (<s> ^foo <a> ^bar <a>)); this is case 2
			value_known_apriori = ( ( (*w_p)->value->id.epmem_id != EPMEM_NODEID_BAD ) && ( (*w_p)->value->id.epmem_valid == my_agent->epmem_validation ) );

			// if long-term identifier as value, special processing (we may need to promote, we don't add to/take from any pools)
			if ( (*w_p)->value->id.smem_lti )
			{
				// find the lti or add new one
				if ( !value_known_apriori )
				{
					(*w_p)->value->id.epmem_id = EPMEM_NODEID_BAD;
					(*w_p)->value->id.epmem_valid = my_agent->epmem_validation;

					// try to find
					{
						// E587: AM:
						my_agent->epmem_stmts_master->find_lti->bind_int( 1, static_cast<uint64_t>( (*w_p)->value->id.name_letter ) );
						my_agent->epmem_stmts_master->find_lti->bind_int( 2, static_cast<uint64_t>( (*w_p)->value->id.name_number ) );

						if ( my_agent->epmem_stmts_master->find_lti->execute() == soar_module::row )
						{
							(*w_p)->value->id.epmem_id = static_cast<epmem_node_id>( my_agent->epmem_stmts_master->find_lti->column_int( 0 ) );
						}

						my_agent->epmem_stmts_master->find_lti->reinitialize();
					}

					// add if necessary
					if ( (*w_p)->value->id.epmem_id == EPMEM_NODEID_BAD )
					{
						(*w_p)->value->id.epmem_id = my_agent->epmem_stats->next_id->get_value();
						my_agent->epmem_stats->next_id->set_value( (*w_p)->value->id.epmem_id + 1 );
						epmem_set_variable( my_agent, var_next_id, (*w_p)->value->id.epmem_id + 1 );

						// add repository
						(*my_agent->epmem_id_repository)[ (*w_p)->value->id.epmem_id ] = new epmem_hashed_id_pool;

						_epmem_promote_id( my_agent, (*w_p)->value, time_counter );
					}
				}

				// now perform deliberate edge search
				// ltis don't use the pools, so we make a direct search in the edge_unique table
				// if failure, drop below and use standard channels
				{
					// get temporal hash
					if ( (*w_p)->acceptable )
					{
						my_hash = EPMEM_HASH_ACCEPTABLE;
					}
					else
					{
						my_hash = epmem_temporal_hash( my_agent, (*w_p)->attr );
					}

					// E587: AM:
					// q0, w, q1
					my_agent->epmem_stmts_master->find_edge_unique_shared->bind_int( 1, parent_id );
					my_agent->epmem_stmts_master->find_edge_unique_shared->bind_int( 2, my_hash );
					my_agent->epmem_stmts_master->find_edge_unique_shared->bind_int( 3, (*w_p)->value->id.epmem_id );

					if ( my_agent->epmem_stmts_master->find_edge_unique_shared->execute() == soar_module::row )
					{
						(*w_p)->epmem_id = my_agent->epmem_stmts_master->find_edge_unique_shared->column_int( 0 );
					}

					my_agent->epmem_stmts_master->find_edge_unique_shared->reinitialize();
				}
			}
			else
			{
				// in the case of a known value, we already have a reservation (case 1)
				if ( value_known_apriori )
				{
					r_p = id_reservations.find( (*w_p) );

					if ( r_p != id_reservations.end() )
					{
						// restore reservation info
						my_hash = r_p->second->my_hash;
						my_id_repo2 = r_p->second->my_pool;

						if ( r_p->second->my_id != EPMEM_NODEID_BAD )
						{
							(*w_p)->epmem_id = r_p->second->my_id;
							(*my_agent->epmem_id_replacement)[ (*w_p)->epmem_id ] = my_id_repo2;
						}

						// delete reservation and map entry
						delete r_p->second;
						id_reservations.erase( r_p );
					}
					// OR a shared identifier at the same level, in which case we need an exact match (case 2)
					else
					{
						// get temporal hash
						if ( (*w_p)->acceptable )
						{
							my_hash = EPMEM_HASH_ACCEPTABLE;
						}
						else
						{
							my_hash = epmem_temporal_hash( my_agent, (*w_p)->attr );
						}

						// try to get an id that matches new information
						my_id_repo =& (*(*my_agent->epmem_id_repository)[ parent_id ])[ my_hash ];
						if ( (*my_id_repo) )
						{
							if ( !(*my_id_repo)->empty() )
							{
								for ( pool_p = (*my_id_repo)->begin(); pool_p != (*my_id_repo)->end(); pool_p++ )
								{
									if ( pool_p->first == (*w_p)->value->id.epmem_id )
									{
										(*w_p)->epmem_id = pool_p->second;
										(*my_id_repo)->erase( pool_p );
										(*my_agent->epmem_id_replacement)[ (*w_p)->epmem_id ] = (*my_id_repo);
										break;
									}
								}
							}
						}
						else
						{
							// add repository
							(*my_id_repo) = new epmem_id_pool;
						}

						// keep the address for later (used if w->epmem_id was not assigned)
						my_id_repo2 = (*my_id_repo);
					}
				}
				// case 3
				else
				{
					// UNKNOWN identifier
					new_identifiers.insert( (*w_p)->value );

					// get temporal hash
					if ( (*w_p)->acceptable )
					{
						my_hash = EPMEM_HASH_ACCEPTABLE;
					}
					else
					{
						my_hash = epmem_temporal_hash( my_agent, (*w_p)->attr );
					}

					// try to find node
					my_id_repo =& (*(*my_agent->epmem_id_repository)[ parent_id ])[ my_hash ];
					if ( (*my_id_repo) )
					{
						// if something leftover, try to use it
						if ( !(*my_id_repo)->empty() )
						{
							for (pool_p = (*my_id_repo)->begin(); pool_p != (*my_id_repo)->end(); pool_p++)
							{
								// the ref set for this epmem_id may not be there if the pools were regenerated from a previous DB
								// a non-existant ref set is the equivalent of a ref count of 0 (ie. an empty ref set)
								// so we allow the identifier from the pool to be used
								if ( ( my_agent->epmem_id_ref_counts->count(pool_p->first) == 0 ) ||
										( (*my_agent->epmem_id_ref_counts)[ pool_p->first ]->empty() ) )

								{
									(*w_p)->epmem_id = pool_p->second;
									(*w_p)->value->id.epmem_id = pool_p->first;
									(*w_p)->value->id.epmem_valid = my_agent->epmem_validation;
									(*my_id_repo)->erase( pool_p );
									(*my_agent->epmem_id_replacement)[ (*w_p)->epmem_id ] = (*my_id_repo);
									break;
								}
							}
						}
					}
					else
					{
						// add repository
						(*my_id_repo) = new epmem_id_pool;
					}

					// keep the address for later (used if w->epmem_id was not assgined)
					my_id_repo2 = (*my_id_repo);
				}
			}

			// add wme if no success above
			if ( (*w_p)->epmem_id == EPMEM_NODEID_BAD )
			{
				// can't use value_known_apriori, since value may have been assigned (lti, id repository via case 3)
				if ( ( (*w_p)->value->id.epmem_id == EPMEM_NODEID_BAD ) || ( (*w_p)->value->id.epmem_valid != my_agent->epmem_validation ) )
				{
					// update next id
					(*w_p)->value->id.epmem_id = my_agent->epmem_stats->next_id->get_value();
					(*w_p)->value->id.epmem_valid = my_agent->epmem_validation;
					my_agent->epmem_stats->next_id->set_value( (*w_p)->value->id.epmem_id + 1 );
					epmem_set_variable( my_agent, var_next_id, (*w_p)->value->id.epmem_id + 1 );

					// add repository for possible future children
					(*my_agent->epmem_id_repository)[ (*w_p)->value->id.epmem_id ] = new epmem_hashed_id_pool;

					// add ref set
#ifdef USE_MEM_POOL_ALLOCATORS
					(*my_agent->epmem_id_ref_counts)[ (*w_p)->value->id.epmem_id ] = new epmem_wme_set( std::less< wme* >(), soar_module::soar_memory_pool_allocator< wme* >( my_agent ) );
#else
					(*my_agent->epmem_id_ref_counts)[ (*w_p)->value->id.epmem_id ] = new epmem_wme_set();
#endif
				}

				// insert (q0,w,q1)
				my_agent->epmem_stmts_master->add_edge_unique->bind_int( 1, parent_id );
				my_agent->epmem_stmts_master->add_edge_unique->bind_int( 2, my_hash );
				my_agent->epmem_stmts_master->add_edge_unique->bind_int( 3, (*w_p)->value->id.epmem_id );
				my_agent->epmem_stmts_master->add_edge_unique->bind_int( 4, LLONG_MAX );
				my_agent->epmem_stmts_master->add_edge_unique->execute( soar_module::op_reinit );

				(*w_p)->epmem_id = static_cast<epmem_node_id>( my_agent->epmem_db->last_insert_rowid() );

				if ( !(*w_p)->value->id.smem_lti )
				{
					// replace the epmem_id and wme id in the right place
					(*my_agent->epmem_id_replacement)[ (*w_p)->epmem_id ] = my_id_repo2;
				}

				// new nodes definitely start
				epmem_edge.push( (*w_p)->epmem_id );
				my_agent->epmem_edge_mins->push_back( time_counter );
				my_agent->epmem_edge_maxes->push_back( false );
			}
			else
			{
				// definitely don't remove
				(*my_agent->epmem_edge_removals)[ (*w_p)->epmem_id ] = false;

				// we add ONLY if the last thing we did was remove
				if ( (*my_agent->epmem_edge_maxes)[ (*w_p)->epmem_id - 1 ] )
				{
					epmem_edge.push( (*w_p)->epmem_id );
					(*my_agent->epmem_edge_maxes)[ (*w_p)->epmem_id - 1 ] = false;
				}
			}

			// at this point we have successfully added a new wme
			// whose value is an identifier.  If the value was
			// unknown at the beginning of this episode, then we need
			// to update its ref count for each WME added (thereby catching
			// up with ref counts that would have been accumulated via wme adds)
			if ( new_identifiers.find( (*w_p)->value ) != new_identifiers.end() )
			{
				// because we could have bypassed the ref set before, we need to create it here
				if ( my_agent->epmem_id_ref_counts->count( (*w_p)->value->id.epmem_id ) == 0 )
				{
#ifdef USE_MEM_POOL_ALLOCATORS
					(*my_agent->epmem_id_ref_counts)[ (*w_p)->value->id.epmem_id ] = new epmem_wme_set( std::less< wme* >(), soar_module::soar_memory_pool_allocator< wme* >( my_agent ) );
#else
					(*my_agent->epmem_id_ref_counts)[ (*w_p)->value->id.epmem_id ] = new epmem_wme_set;
#endif
				}
				(*my_agent->epmem_id_ref_counts)[ (*w_p)->value->id.epmem_id ]->insert( (*w_p) );
			}

			// if the value has not been iterated over, continue to augmentations
			if ( (*w_p)->value->id.tc_num != tc )
			{
				parent_syms.push( (*w_p)->value );
				parent_ids.push( (*w_p)->value->id.epmem_id );
			}
		}
		else
		{
			// have we seen this node in this database?
			if ( ( (*w_p)->epmem_id == EPMEM_NODEID_BAD ) || ( (*w_p)->epmem_valid != my_agent->epmem_validation ) )
			{
				(*w_p)->epmem_id = EPMEM_NODEID_BAD;
				(*w_p)->epmem_valid = my_agent->epmem_validation;

				my_hash = epmem_temporal_hash( my_agent, (*w_p)->attr );
				my_hash2 = epmem_temporal_hash( my_agent, (*w_p)->value );

				// try to get node id
				{
					// E587: AM:
					// parent_id=? AND attr=? AND value=?
					my_agent->epmem_stmts_master->find_node_unique->bind_int( 1, parent_id );
					my_agent->epmem_stmts_master->find_node_unique->bind_int( 2, my_hash );
					my_agent->epmem_stmts_master->find_node_unique->bind_int( 3, my_hash2 );

					if ( my_agent->epmem_stmts_master->find_node_unique->execute() == soar_module::row )
					{
						(*w_p)->epmem_id = my_agent->epmem_stmts_master->find_node_unique->column_int( 0 );
					}

					my_agent->epmem_stmts_master->find_node_unique->reinitialize();
				}

				// act depending on new/existing feature
				if ( (*w_p)->epmem_id == EPMEM_NODEID_BAD )
				{
					// E587: AM:
					// insert (parent_id,attr,value)
					my_agent->epmem_stmts_master->add_node_unique->bind_int( 1, parent_id );
					my_agent->epmem_stmts_master->add_node_unique->bind_int( 2, my_hash );
					my_agent->epmem_stmts_master->add_node_unique->bind_int( 3, my_hash2 );
					my_agent->epmem_stmts_master->add_node_unique->execute( soar_module::op_reinit );

					(*w_p)->epmem_id = (epmem_node_id) my_agent->epmem_db->last_insert_rowid();

					// new nodes definitely start
					epmem_node.push( (*w_p)->epmem_id );
					my_agent->epmem_node_mins->push_back( time_counter );
					my_agent->epmem_node_maxes->push_back( false );
				}
				else
				{
					// definitely don't remove
					(*my_agent->epmem_node_removals)[ (*w_p)->epmem_id ] = false;

					// add ONLY if the last thing we did was add
					if ( (*my_agent->epmem_node_maxes)[ (*w_p)->epmem_id - 1 ] )
					{
						epmem_node.push( (*w_p)->epmem_id );
						(*my_agent->epmem_node_maxes)[ (*w_p)->epmem_id - 1 ] = false;
					}
				}
			}
		}
	}
}

#ifdef USE_MPI
void close_workers(){
	epmem_msg msg;
	msg.source = AGENT_ID;
	msg.type = EXIT;
	msg.size = sizeof(epmem_msg);
	MPI::COMM_WORLD.Send(&msg, msg.size, MPI::CHAR, MANAGER_ID, 1);
}
query_rsp_data* send_epmem_query_message(epmem_query* query)
{
	epmem_msg *msg = query->pack();
	MPI::Status status;
	msg->source = AGENT_ID;
	MPI::COMM_WORLD.Send(msg, msg->size, MPI::CHAR, MANAGER_ID, 1);
	delete msg;
	

	// get response
    //blocking probe call (unknown message size)
    MPI::COMM_WORLD.Probe(MANAGER_ID, 1, status);
    
    //get source and size of incoming message
    int buffSize = status.Get_count(MPI::CHAR);
    
    epmem_msg * recMsg = (epmem_msg*) malloc(buffSize);
    MPI::COMM_WORLD.Recv(recMsg, buffSize, MPI::CHAR, MANAGER_ID, 1, status);
    // handle
	query_rsp_data* rsp = new query_rsp_data();
	
	rsp->unpack(recMsg);
	delete recMsg;

	return rsp;
}

void send_epmem_worker_init_data(epmem_param_container* epmem_params)
{
	int page_size = epmem_params->page_size->get_value();
	bool optimization = (epmem_params->opt->get_value() == epmem_param_container::opt_speed );
	char* str = epmem_params->cache_size->get_string();
	int size = strlen(str) + 1;

	int dataSize = sizeof(epmem_worker_init_data) + size;
	int msgsize = dataSize + sizeof(epmem_msg);
	
	char *msg = (char*)malloc(msgsize);
	epmem_msg *ep = (epmem_msg*)msg;
	
	ep->source = AGENT_ID;
    ep->size = msgsize;
    ep->type = INIT_WORKER;
	
	epmem_worker_init_data* data = (epmem_worker_init_data*) &(ep->data);
	data->page_size = page_size;
	data->optimization = optimization;
	data->buffsize = dataSize;
    memcpy(&(data->str), str, size);//episode->buffer_size-10);
	MPI::COMM_WORLD.Send(msg, msgsize, MPI::CHAR, MANAGER_ID, 1);
    
    delete msg;
	free(str);
    return;
}

void send_new_episode(epmem_episode_diff* episode)
{
	int ep_size = episode->buffer_size;//*sizeof(int64_t);
    int size =  ep_size + sizeof(epmem_msg);//sizeof(int)*2 + sizeof(EPMEM_MSG_TYPE);
	int id = MPI::COMM_WORLD.Get_rank(); 
	
	char *msg = (char*)malloc(size);//size);
	epmem_msg *ep = (epmem_msg*)msg;
	ep->source = AGENT_ID;
    ep->size = size;
    ep->type = NEW_EP;
	
	int64_t *buffer = (int64_t*)malloc(ep_size);
	
    memcpy(&(ep->data), (char*)episode->buffer, ep_size);//episode->buffer_size-10);
    MPI::COMM_WORLD.Send(msg, size, MPI::CHAR, MANAGER_ID, 1);
    
    delete msg;
    return;
}

#endif

void epmem_new_episode( agent *my_agent )
{
	// if this is the first episode, initialize db components
	if ( my_agent->epmem_db->get_status() == soar_module::disconnected )
	{
		epmem_init_db( my_agent );
	}

	// add the episode only if db is properly initialized
	if ( my_agent->epmem_db->get_status() != soar_module::connected )
	{
		return;
	}

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->storage->start();
	////////////////////////////////////////////////////////////////////////////

	epmem_time_id time_counter = my_agent->epmem_stats->time->get_value();
#ifdef USE_MPI
	double start_time = MPI_Wtime();
#endif

	// provide trace output
	if ( my_agent->sysparams[ TRACE_EPMEM_SYSPARAM ] )
	{
		char buf[256];

		SNPRINTF( buf, 254, "NEW EPISODE: %ld", static_cast<long int>(time_counter) );

		print( my_agent, buf );
		xml_generate_warning( my_agent, buf );
	}

	// perform storage
	{
		// seen nodes (non-identifiers) and edges (identifiers)
		std::queue<epmem_node_id> epmem_node;
		std::queue<epmem_node_id> epmem_edge;

		// walk appropriate levels
		{
			// prevents infinite loops
			tc_number tc = get_new_tc_number( my_agent );

			// children of the current identifier
			epmem_wme_list* wmes = NULL;

			// breadth first search state
			std::queue< Symbol* > parent_syms;
			Symbol* parent_sym = NULL;
			std::queue< epmem_node_id > parent_ids;
			epmem_node_id parent_id;

			// cross-level information
			std::map< wme*, epmem_id_reservation* > id_reservations;
			std::set< Symbol* > new_identifiers;

			// start with new WMEs attached to known identifiers
			for ( epmem_symbol_set::iterator id_p = my_agent->epmem_wme_adds->begin(); id_p != my_agent->epmem_wme_adds->end(); id_p++ )
			{
				// make sure the WME is valid
				// it can be invalid if a child WME was added, but then the parent was removed, setting the epmem_id to EPMEM_NODEID_BAD
				if ((*id_p)->id.epmem_id != EPMEM_NODEID_BAD) {
					parent_syms.push( (*id_p) );
					parent_ids.push( (*id_p)->id.epmem_id );
					while ( !parent_syms.empty() )
					{
						parent_sym = parent_syms.front();
						parent_syms.pop();
						parent_id = parent_ids.front();
						parent_ids.pop();
						wmes = epmem_get_augs_of_id( parent_sym, tc );
						if ( ! wmes->empty() )
						{
							_epmem_store_level( my_agent, parent_syms, parent_ids, tc, wmes->begin(), wmes->end(), parent_id, time_counter, id_reservations, new_identifiers, epmem_node, epmem_edge );
						}
						delete wmes;
					}
				}
			}
		}

		int num_node_removals = 0;
		int num_edge_removals = 0;
		
		// First calculate how many nodes/edges will be removed
		{
			epmem_id_removal_map::iterator r;
			epmem_time_id range_start;
			epmem_time_id range_end;

#ifdef EPMEM_EXPERIMENT
			epmem_dc_interval_removes = 0;
#endif
			
			// Removing edges, first count the number, then store them in the array
			for(r = my_agent->epmem_edge_removals->begin(); r != my_agent->epmem_edge_removals->end(); r++){
				if ( !r->second ){
					continue;
				}
				num_edge_removals++;

#ifdef EPMEM_EXPERIMENT
					epmem_dc_interval_removes++;
#endif

				// update max
				(*my_agent->epmem_edge_maxes)[ r->first - 1 ] = true;
			}

			// Removing nodes, first count the number, then store them in the array
			for(r = my_agent->epmem_node_removals->begin(); r != my_agent->epmem_node_removals->end(); r++){
				if ( !r->second ){
					continue;
				}
				num_node_removals++;

#ifdef EPMEM_EXPERIMENT
					epmem_dc_interval_removes++;
#endif

				// update max
				(*my_agent->epmem_node_maxes)[ r->first - 1 ] = true;
			}
		}
		
		// Allocate space for the new episode
		epmem_episode_diff* episode = new epmem_episode_diff(time_counter, epmem_node.size(), num_node_removals, epmem_edge.size(), num_edge_removals);

		// all inserts
		{
			epmem_node_id *temp_node;

#ifdef EPMEM_EXPERIMENT
			epmem_dc_interval_inserts = epmem_node.size() + epmem_edge.size();
#endif


			int cur_node = 0;

			// Add the added nodes to the episode
			while ( !epmem_node.empty() )
			{
				temp_node =& epmem_node.front();

				// If the id is not in the node_unique table then add it
				my_agent->epmem_stmts_master->get_node_unique->bind_int( 1, (*temp_node) );
				if ( my_agent->epmem_stmts_master->get_node_unique->execute() == soar_module::row ) {
					episode->added_nodes[cur_node].id = (*temp_node);
					episode->added_nodes[cur_node].parent_id = my_agent->epmem_stmts_master->get_node_unique->column_int(0);
					episode->added_nodes[cur_node].attribute = my_agent->epmem_stmts_master->get_node_unique->column_int(1);
					episode->added_nodes[cur_node].value = my_agent->epmem_stmts_master->get_node_unique->column_int(2);
				}
				my_agent->epmem_stmts_master->get_node_unique->reinitialize();
				cur_node++;

				// update min
				(*my_agent->epmem_node_mins)[ (*temp_node) - 1 ] = time_counter;

				epmem_node.pop();
			}

			int cur_edge = 0;

			// edges
			while ( !epmem_edge.empty() )
			{
				temp_node =& epmem_edge.front();

				// Lookup the edge from edge_unique and store its information
				my_agent->epmem_stmts_master->get_edge_unique->bind_int( 1, (*temp_node) );
				if ( my_agent->epmem_stmts_master->get_edge_unique->execute() == soar_module::row ) {
					episode->added_edges[cur_edge].id = (*temp_node);
					episode->added_edges[cur_edge].parent_id = my_agent->epmem_stmts_master->get_edge_unique->column_int(0);
					episode->added_edges[cur_edge].attribute = my_agent->epmem_stmts_master->get_edge_unique->column_int(1);
					episode->added_edges[cur_edge].child_id = my_agent->epmem_stmts_master->get_edge_unique->column_int(2);
				}
				my_agent->epmem_stmts_master->get_edge_unique->reinitialize();
				cur_edge++;

				// update min
				(*my_agent->epmem_edge_mins)[ (*temp_node) - 1 ] = time_counter;

				epmem_edge.pop();
			}
		}

		// Add the removed nodes to the episode
		{
			epmem_id_removal_map::iterator r;
			// Go through and add the removal information to the epmem_episode_diff
			int cur_node = 0;
			for(r = my_agent->epmem_node_removals->begin(); r != my_agent->epmem_node_removals->end(); r++){
				if ( !r->second ){
					continue;
				}				
				
				my_agent->epmem_stmts_master->get_node_unique->bind_int( 1, r->first );
				if ( my_agent->epmem_stmts_master->get_node_unique->execute() == soar_module::row ) {
					episode->removed_nodes[cur_node].id = r->first;
					episode->removed_nodes[cur_node].parent_id = my_agent->epmem_stmts_master->get_node_unique->column_int(0);
					episode->removed_nodes[cur_node].attribute = my_agent->epmem_stmts_master->get_node_unique->column_int(1);
					episode->removed_nodes[cur_node].value = my_agent->epmem_stmts_master->get_node_unique->column_int(2);
				}
				my_agent->epmem_stmts_master->get_node_unique->reinitialize();
				cur_node++;
			}

			my_agent->epmem_node_removals->clear();

			// Go through and add the removal information to the epmem_episode_diff
			int cur_edge = 0;
			for(r = my_agent->epmem_edge_removals->begin(); r != my_agent->epmem_edge_removals->end(); r++){
				if ( !r->second ){
					continue;
				}				
				
				my_agent->epmem_stmts_master->get_edge_unique->bind_int( 1, r->first );
				if ( my_agent->epmem_stmts_master->get_edge_unique->execute() == soar_module::row ) {
					episode->removed_edges[cur_edge].id = r->first;
					episode->removed_edges[cur_edge].parent_id = my_agent->epmem_stmts_master->get_edge_unique->column_int(0);
					episode->removed_edges[cur_edge].attribute = my_agent->epmem_stmts_master->get_edge_unique->column_int(1);
					episode->removed_edges[cur_edge].child_id = my_agent->epmem_stmts_master->get_edge_unique->column_int(2);
				}
				my_agent->epmem_stmts_master->get_edge_unique->reinitialize();
				cur_edge++;
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
		my_agent->epmem_stmts_master->add_time->bind_int( 1, time_counter );
		my_agent->epmem_stmts_master->add_time->execute( soar_module::op_reinit );

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

		// clear add/remove maps
		{
			my_agent->epmem_wme_adds->clear();
		}

        // Send the episode diff to the processor

#ifdef USE_MPI
        send_new_episode(episode);
		MPI::COMM_WORLD.Barrier();
#else
        //STORE
		my_agent->epmem_worker_p->add_epmem_episode_diff(episode);
        if(episode->time > 4 && episode->time % 2 == 0){
            epmem_episode_diff* diff = my_agent->epmem_worker_p->remove_oldest_episode();
            my_agent->epmem_worker_p2->add_epmem_episode_diff(diff);
            delete diff;
        }
/*
            my_agent->epmem_worker_p->epmem_db->backup("p1_after.db", new std::string("Backup error"));
            my_agent->epmem_worker_p2->epmem_db->backup("p2_after.db", new std::string("Backup error"));*/
#endif
		delete episode;
	}
#ifdef USE_MPI
	if(time_counter % 1000 == 0){
		std::cout << "Storing " << time_counter << " time(" << (MPI_Wtime() - start_time) << ")" << std::endl;
	}
#endif
	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->storage->stop();
	////////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Non-Cue-Based Retrieval Functions (epmem::ncb)
//
// NCB retrievals occur when you know the episode you
// want to retrieve.  It is the process of converting
// the database representation to WMEs in working
// memory.
//
// This occurs at the end of a cue-based query, or
// in response to a retrieve/next/previous command.
//
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_valid_episode
 * Author		: Nate Derbinsky
 * Notes		: Returns true if the temporal id is valid
 **************************************************************************/
bool epmem_valid_episode( agent *my_agent, epmem_time_id memory_id )
{
	bool return_val = false;

	{
		// E587: AM:
		soar_module::sqlite_statement *my_q = my_agent->epmem_stmts_master->valid_episode;

		my_q->bind_int( 1, memory_id );
		my_q->execute();
		return_val = ( my_q->column_int( 0 ) > 0 );
		my_q->reinitialize();
	}

	return return_val;
}

inline void _epmem_install_id_wme( agent* my_agent, Symbol* parent, Symbol* attr, std::map< epmem_node_id, std::pair< Symbol*, bool > >* ids, epmem_node_id q1, bool val_is_short_term, char val_letter, uint64_t val_num, epmem_id_mapping* id_record, soar_module::symbol_triple_list& retrieval_wmes )
{
	std::map< epmem_node_id, std::pair< Symbol*, bool > >::iterator id_p = ids->find( q1 );
	bool existing_identifier = ( id_p != ids->end() );

	if ( val_is_short_term )
	{
		if ( !existing_identifier )
		{
			id_p = ids->insert( std::make_pair< epmem_node_id, std::pair< Symbol*, bool > >( q1, std::make_pair< Symbol*, bool >( make_new_identifier( my_agent, ( ( attr->common.symbol_type == SYM_CONSTANT_SYMBOL_TYPE )?( attr->sc.name[0] ):('E') ), parent->id.level ), true ) ) ).first;
			if ( id_record )
			{
				epmem_id_mapping::iterator rec_p = id_record->find( q1 );
				if ( rec_p != id_record->end() )
				{
					rec_p->second = id_p->second.first;
				}
			}
		}

		epmem_buffer_add_wme( retrieval_wmes, parent, attr, id_p->second.first );

		if ( !existing_identifier )
		{
			symbol_remove_ref( my_agent, id_p->second.first );
		}
	}
	else
	{
		if ( existing_identifier )
		{
			epmem_buffer_add_wme( retrieval_wmes, parent, attr, id_p->second.first );
		}
		else
		{
			Symbol* value = smem_lti_soar_make( my_agent, smem_lti_get_id( my_agent, val_letter, val_num ), val_letter, val_num, parent->id.level );

			if ( id_record )
			{
				epmem_id_mapping::iterator rec_p = id_record->find( q1 );
				if ( rec_p != id_record->end() )
				{
					rec_p->second = value;
				}
			}

			epmem_buffer_add_wme( retrieval_wmes, parent, attr, value );
			symbol_remove_ref( my_agent, value );

			ids->insert( std::make_pair< epmem_node_id, std::pair< Symbol*, bool > >( q1, std::make_pair< Symbol*, bool >( value, !( ( value->id.impasse_wmes ) || ( value->id.input_wmes ) || ( value->id.slots ) ) ) ) );
		}
	}
}

/***************************************************************************
 * Function     : epmem_install_memory
 * Author		: Nate Derbinsky
 * Notes		: Reconstructs an episode in working memory.
 *
 * 				  Use RIT to collect appropriate ranges.  Then
 * 				  combine with NOW and POINT.  Merge with unique
 * 				  to get all the data necessary to create WMEs.
 *
 * 				  The id_record parameter is only used in the case
 * 				  that the graph-match has a match and creates
 * 				  a mapping of identifiers that should be recorded
 * 				  during reconstruction.
 **************************************************************************/
void epmem_install_memory( agent *my_agent, Symbol *state, epmem_time_id memory_id, soar_module::symbol_triple_list& meta_wmes, soar_module::symbol_triple_list& retrieval_wmes, epmem_id_mapping *id_record = NULL )
{
	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->ncb_retrieval->start();
	////////////////////////////////////////////////////////////////////////////

	// get the ^result header for this state
	Symbol *result_header = state->id.epmem_result_header;

	// initialize stat
	int64_t num_wmes = 0;
	my_agent->epmem_stats->ncb_wmes->set_value( num_wmes );

	// if no memory, say so
	if ( ( memory_id == EPMEM_MEMID_NONE ) ||
			!epmem_valid_episode( my_agent, memory_id ) )
	{
		epmem_buffer_add_wme( meta_wmes, result_header, my_agent->epmem_sym_retrieved, my_agent->epmem_sym_no_memory );
		state->id.epmem_info->last_memory = EPMEM_MEMID_NONE;

		////////////////////////////////////////////////////////////////////////////
		my_agent->epmem_timers->ncb_retrieval->stop();
		////////////////////////////////////////////////////////////////////////////

		return;
	}

	// remember this as the last memory installed
	state->id.epmem_info->last_memory = memory_id;

	// create a new ^retrieved header for this result
	Symbol *retrieved_header;
	retrieved_header = make_new_identifier( my_agent, 'R', result_header->id.level );
	if ( id_record )
	{
		(*id_record)[ EPMEM_NODEID_ROOT ] = retrieved_header;
	}

	epmem_buffer_add_wme( meta_wmes, result_header, my_agent->epmem_sym_retrieved, retrieved_header );
	symbol_remove_ref( my_agent, retrieved_header );

	// add *-id wme's
	{
		Symbol *my_meta;
		
		my_meta = make_int_constant( my_agent, static_cast<int64_t>( memory_id ) );
		epmem_buffer_add_wme( meta_wmes, result_header, my_agent->epmem_sym_memory_id, my_meta );
		symbol_remove_ref( my_agent, my_meta );

		my_meta = make_int_constant( my_agent, static_cast<int64_t>( my_agent->epmem_stats->time->get_value() ) );
		epmem_buffer_add_wme( meta_wmes, result_header, my_agent->epmem_sym_present_id, my_meta );
		symbol_remove_ref( my_agent, my_meta );
	}

	// install memory
#ifndef USE_MPI //TODO MAY CHANGE
	{
		// Big picture: create identifier skeleton, then hang non-identifers
		//
		// Because of shared WMEs at different levels of the storage breadth-first search,
		// there is the possibility that the unique database id of an identifier can be
		// greater than that of its parent.  Because the retrieval query sorts by
		// unique id ascending, it is thus possible to have an "orphan" - a child with
		// no current parent.  We keep track of orphans and add them later, hoping their
		// parents have shown up.  I *suppose* there could be a really evil case in which
		// the ordering of the unique ids is exactly opposite of their insertion order.
		// I just hope this isn't a common case...

		// shared identifier lookup table
		std::map< epmem_node_id, std::pair< Symbol*, bool > > ids;
		bool dont_abide_by_ids_second = ( my_agent->epmem_params->merge->get_value() == epmem_param_container::merge_add );

		// symbols used to create WMEs
		Symbol *attr = NULL;

		// lookup query
		soar_module::sqlite_statement *my_q;

		// initialize the lookup table
		ids[ EPMEM_NODEID_ROOT ] = std::make_pair< Symbol*, bool >( retrieved_header, true );



		// E587: AM:
		// first identifiers (i.e. reconstruct)
		{
			// relates to finite automata: q1 = d(q0, w)
			epmem_node_id q0; // id
			epmem_node_id q1; // attribute
			int64_t w_type; // we support any constant attribute symbol

			bool val_is_short_term = false;
			char val_letter = NIL;
			int64_t val_num = NIL;

			// used to lookup shared identifiers
			// the bool in the pair refers to if children are allowed on this id (re: lti)
			std::map< epmem_node_id, std::pair< Symbol*, bool> >::iterator id_p;

			// orphaned children
			std::queue< epmem_edge* > orphans;
			epmem_edge *orphan;

			soar_module::sqlite_statement* get_edge_desc = my_agent->epmem_stmts_master->get_edge_desc;

			std::vector<epmem_node_id> edge_ids;
			my_agent->epmem_worker_p->get_edges_at_episode(memory_id, &edge_ids);

			for(int i = 0; i < edge_ids.size(); i++){
				get_edge_desc->bind_int(1, memory_id);
				get_edge_desc->bind_int(2, edge_ids[i]);
				if(get_edge_desc->execute() != soar_module::row){
					get_edge_desc->reinitialize();
					continue;
				}
				// q0, w, q1, w_type
				q0 = get_edge_desc->column_int( 0 );
				q1 = get_edge_desc->column_int( 2 );
				w_type = get_edge_desc->column_int( 3 );

				switch ( w_type )
				{
					case INT_CONSTANT_SYMBOL_TYPE:
						attr = make_int_constant( my_agent,get_edge_desc->column_int( 1 ) );
						break;

					case FLOAT_CONSTANT_SYMBOL_TYPE:
						attr = make_float_constant( my_agent, get_edge_desc->column_double( 1 ) );
						break;

					case SYM_CONSTANT_SYMBOL_TYPE:
						attr = make_sym_constant( my_agent, const_cast<char *>( reinterpret_cast<const char *>( get_edge_desc->column_text( 1 ) ) ) );
						break;
				}

				// short vs. long-term
				val_is_short_term = ( get_edge_desc->column_type( 4 ) == soar_module::null_t );
				if ( !val_is_short_term )
				{
					val_letter = static_cast<char>( get_edge_desc->column_int( 4 ) );
					val_num = static_cast<uint64_t>( get_edge_desc->column_int( 5 ) );
				}

				// get a reference to the parent
				id_p = ids.find( q0 );
				if ( id_p != ids.end() )
				{
					// if existing lti with kids don't touch
					if ( dont_abide_by_ids_second || id_p->second.second )
					{
						_epmem_install_id_wme( my_agent, id_p->second.first, attr, &( ids ), q1, val_is_short_term, val_letter, val_num, id_record, retrieval_wmes );
						num_wmes++;
					}

					symbol_remove_ref( my_agent, attr );
				}
				else
				{
					// out of order
					orphan = new epmem_edge;
					orphan->q0 = q0;
					orphan->w = attr;
					orphan->q1 = q1;

					orphan->val_letter = NIL;
					orphan->val_num = NIL;

					orphan->val_is_short_term = val_is_short_term;
					if ( !val_is_short_term )
					{
						orphan->val_letter = val_letter;
						orphan->val_num = val_num;
					}

					orphans.push( orphan );
				}
				get_edge_desc->reinitialize();
			}

			// take care of any orphans
			if ( !orphans.empty() )
			{
				std::queue<epmem_edge *>::size_type orphans_left;
				std::queue<epmem_edge *> still_orphans;

				do
				{
					orphans_left = orphans.size();

					while ( !orphans.empty() )
					{
						orphan = orphans.front();
						orphans.pop();

						// get a reference to the parent
						id_p = ids.find( orphan->q0 );
						if ( id_p != ids.end() )
						{
							if ( dont_abide_by_ids_second || id_p->second.second )
							{
								_epmem_install_id_wme( my_agent, id_p->second.first, orphan->w, &( ids ), orphan->q1, orphan->val_is_short_term, orphan->val_letter, orphan->val_num, id_record, retrieval_wmes );
								num_wmes++;
							}

							symbol_remove_ref( my_agent, orphan->w );

							delete orphan;
						}
						else
						{
							still_orphans.push( orphan );
						}
					}

					orphans = still_orphans;
					while ( !still_orphans.empty() )
					{
						still_orphans.pop();
					}

				} while ( ( !orphans.empty() ) && ( orphans_left != orphans.size() ) );

				while ( !orphans.empty() )
				{
					orphan = orphans.front();
					orphans.pop();

					symbol_remove_ref( my_agent, orphan->w );

					delete orphan;
				}
			}
		}

		// E587: AM:
		// then node_unique
		{
			epmem_node_id child_id;
			epmem_node_id parent_id;
			int64_t attr_type;
			int64_t value_type;

			std::pair< Symbol*, bool > parent;
			Symbol *value = NULL;
			
			soar_module::sqlite_statement* get_node_desc = my_agent->epmem_stmts_master->get_node_desc;

			std::vector<epmem_node_id> node_ids;
			my_agent->epmem_worker_p->get_nodes_at_episode(memory_id, &node_ids);

			for(int i = 0; i < node_ids.size(); i++){
				child_id = node_ids[i];
				get_node_desc->bind_int(1, child_id);
				if(get_node_desc->execute() != soar_module::row){
					get_node_desc->reinitialize();
					continue;
				}


				// f.child_id, f.parent_id, f.name, f.value, f.attr_type, f.value_type
				parent_id = get_node_desc->column_int( 1 );
				attr_type = get_node_desc->column_int( 4 );
				value_type = get_node_desc->column_int( 5 );

				// get a reference to the parent
				parent = ids[ parent_id ];

				if ( dont_abide_by_ids_second || parent.second )
				{
					// make a symbol to represent the attribute
					switch ( attr_type )
					{
						case INT_CONSTANT_SYMBOL_TYPE:
							attr = make_int_constant( my_agent, get_node_desc->column_int( 2 ) );
							break;

						case FLOAT_CONSTANT_SYMBOL_TYPE:
							attr = make_float_constant( my_agent, get_node_desc->column_double( 2 ) );
							break;

						case SYM_CONSTANT_SYMBOL_TYPE:
							attr = make_sym_constant( my_agent, const_cast<char *>( reinterpret_cast<const char *>( get_node_desc->column_text( 2 ) ) ) );
							break;
					}

					// make a symbol to represent the value
					switch ( value_type )
					{
						case INT_CONSTANT_SYMBOL_TYPE:
							value = make_int_constant( my_agent, get_node_desc->column_int( 3 ) );
							break;

						case FLOAT_CONSTANT_SYMBOL_TYPE:
							value = make_float_constant( my_agent, get_node_desc->column_double( 3 ) );
							break;

						case SYM_CONSTANT_SYMBOL_TYPE:
							value = make_sym_constant( my_agent, const_cast<char *>( (const char *) get_node_desc->column_text( 3 ) ) );
							break;
					}

					epmem_buffer_add_wme( retrieval_wmes, parent.first, attr, value );
					num_wmes++;

					symbol_remove_ref( my_agent, attr );
					symbol_remove_ref( my_agent, value );
				}
				get_node_desc->reinitialize();
			}
		}
	}
	#endif

	// adjust stat
	my_agent->epmem_stats->ncb_wmes->set_value( num_wmes );

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->ncb_retrieval->stop();
	////////////////////////////////////////////////////////////////////////////
}

/***************************************************************************
 * Function     : epmem_next_episode
 * Author		: Nate Derbinsky
 * Notes		: Returns the next valid temporal id.  This is really
 * 				  only an issue if you implement episode dynamics like
 * 				  forgetting.
 **************************************************************************/
epmem_time_id epmem_next_episode( agent *my_agent, epmem_time_id memory_id )
{
	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->next->start();
	////////////////////////////////////////////////////////////////////////////

	epmem_time_id return_val = EPMEM_MEMID_NONE;

	if ( memory_id != EPMEM_MEMID_NONE )
	{
		// E587: AM:
		soar_module::sqlite_statement *my_q = my_agent->epmem_stmts_master->next_episode;
		my_q->bind_int( 1, memory_id );
		if ( my_q->execute() == soar_module::row )
		{
			return_val = (epmem_time_id) my_q->column_int( 0 );
		}

		my_q->reinitialize();
	}

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->next->stop();
	////////////////////////////////////////////////////////////////////////////

	return return_val;
}

/***************************************************************************
 * Function     : epmem_previous_episode
 * Author		: Nate Derbinsky
 * Notes		: Returns the last valid temporal id.  This is really
 * 				  only an issue if you implement episode dynamics like
 * 				  forgetting.
 **************************************************************************/
epmem_time_id epmem_previous_episode( agent *my_agent, epmem_time_id memory_id )
{
	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->prev->start();
	////////////////////////////////////////////////////////////////////////////

	epmem_time_id return_val = EPMEM_MEMID_NONE;

	if ( memory_id != EPMEM_MEMID_NONE )
	{
		// E587: AM:
		soar_module::sqlite_statement *my_q = my_agent->epmem_stmts_master->prev_episode;
		my_q->bind_int( 1, memory_id );
		if ( my_q->execute() == soar_module::row )
		{
			return_val = (epmem_time_id) my_q->column_int( 0 );
		}

		my_q->reinitialize();
	}

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->prev->stop();
	////////////////////////////////////////////////////////////////////////////

	return return_val;
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Cue-Based Retrieval (epmem::cbr)
//
// Cue-based retrievals are searches in response to
// queries and/or neg-queries.
//
// All functions below implement John/Andy's range search
// algorithm (see Andy's thesis).  The primary insight
// is to only deal with changes.  In this case, change
// occurs at the end points of ranges of node occurrence.
//
// The implementations below share a common theme:
// 1) identify wmes in the cue
// 2) get pointers to ALL b-tree leaves
//    associated with sorted occurrence-endpoints
//    of these wmes (ranges, points, now)
// 3) step through the leaves according to the range
//    search algorithm
//
// In the Working Memory Tree, the occurrence of a leaf
// node is tantamount to the occurrence of the path to
// the leaf node (since there are no shared identifiers).
// However, when we add graph functionality, path is
// important.  Moreover, identifiers that "blink" have
// ambiguous identities over time.  Thus I introduced
// the Disjunctive Normal Form (DNF) graph.
//
// The primary insight of the DNF graph is that paths to
// leaf nodes can be written as the disjunction of the
// conjunction of paths.
//
// Metaphor: consider that a human child's lineage is
// in question (perhaps for purposes of estate).  We
// are unsure as to the child's grandfather.  The grand-
// father can be either gA or gB.  If it is gA, then the
// father is absolutely fA (otherwise fB).  So the child
// could exist as (where cX is child with lineage X):
//
// (gA ^ fA ^ cA) \/ (gB ^ fB ^ cB)
//
// Note that due to family... irregularities
// (i.e. men sleeping around), a parent might contribute
// to multiple family lines.  Thus gX could exist in
// multiple clauses.  However, due to well-enforced
// incest laws (i.e. we only support acyclic graph cues),
// an individual can only occur once within a lineage/clause.
//
// We have a "match" (i.e. identify the child's lineage)
// only if all literals are "on" in a path of
// lineage.  Thus, our task is to efficiently track DNF
// satisfaction while flipping on/off a single literal
// (which may exist in multiple clauses).
//
// The DNF graph implements this intuition efficiently by
// (say it with me) only processing changes!  First we
// construct the graph by creating "literals" (gA, fA, etc)
// and maintaining parent-child relationships (gA connects
// to fA which connects to cA).  Leaf literals have no
// children, but are associated with a "match."  Each match
// has a cardinality count (positive/negative 1 depending on
// query vs. neg-query) and a WMA value (weighting).
//
// We then connect the b-tree pointers from above with
// each of the literals.  It is possible that a query
// can serve multiple literals, so we gain from sharing.
//
// Nodes within the DNF Graph need only save a "count":
// zero means neither it nor its lineage to date is
// satisfied, one means either its lineage or it is
// satisfied, two means it and its lineage is satisfied.
//
// The range search algorithm is simply walking (in-order)
// the parallel b-tree pointers.  When we get to an endpoint,
// we appropriately turn on/off all directly associated
// literals.  Based upon the current state of the literals,
// we may need to propagate changes to children.
//
// If propogation reaches and changes a match, we alter the
// current episode's cardinality/score.  Thus we achieve
// the Soar mantra of only processing changes!
//
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////
// Justin's Stuff
//////////////////////////////////////////////////////////

#define QUERY_DEBUG 0


epmem_query* construct_epmem_query(agent* my_agent, Symbol* state, Symbol* pos_query, Symbol* neg_query, epmem_time_list& prohibits, epmem_time_id before, epmem_time_id after, epmem_symbol_set& currents, soar_module::wme_set& cue_wmes, int level){
    epmem_query* query = new epmem_query();
    query->level = level;

    epmem_cue_symbol_table sym_table(&query->symbols);

    // pos_query
    query->pos_query_index = sym_table.get_cue_symbol(pos_query, my_agent);

    // neg_query
    if(neg_query != NIL){
        query->neg_query_index = sym_table.get_cue_symbol(pos_query, my_agent);
    } else {
        query->neg_query_index = -1;
    }

    // prohibits
    query->prohibits = prohibits;	
	if (!query->prohibits.empty()) {
		std::sort(query->prohibits.begin(), query->prohibits.end());
	}

    // currents
    for(epmem_symbol_set::iterator i = currents.begin(); i != currents.end(); i++){
        query->currents.push_back(sym_table.get_cue_symbol(*i, my_agent));
    }

    // before/after
    query->before = before;
    query->after = after;

    // graph match parameters
    query->do_graph_match = (my_agent->epmem_params->graph_match->get_value() == soar_module::on);
    query->gm_order = my_agent->epmem_params->gm_ordering->get_value();

    // load the wme's that make up the cue
    std::queue<Symbol*> cue_symbols;
    cue_symbols.push(pos_query);
    if(neg_query != NIL){
        cue_symbols.push(neg_query);
    }

    tc_number tc_num = get_new_tc_number(my_agent);

    // Breadth first search for wme's in the cue
    while(cue_symbols.size() > 0){
        Symbol* cue_symbol = cue_symbols.front();
        cue_symbols.pop();
		epmem_wme_list* children = epmem_get_augs_of_id(cue_symbol, tc_num);
		for (epmem_wme_list::iterator wme_iter = children->begin(); wme_iter != children->end(); wme_iter++) {
            query->wmes.push_back(epmem_packed_cue_wme(*wme_iter, my_agent, sym_table));
            if((*wme_iter)->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE){
                cue_symbols.push((*wme_iter)->value);
            }
		}
        delete children;
    }

    return query;
}

void epmem_process_query(agent *my_agent, Symbol *state, Symbol *pos_query, Symbol *neg_query, epmem_time_list& prohibits, epmem_time_id before, epmem_time_id after, epmem_symbol_set& currents, soar_module::wme_set& cue_wmes, soar_module::symbol_triple_list& meta_wmes, soar_module::symbol_triple_list& retrieval_wmes, int level=3) {
	// a query must contain a positive cue
	if (pos_query == NULL) {
		epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_status, my_agent->epmem_sym_bad_cmd);
		return;
	}

	// before and after, if specified, must be valid relative to each other
	if (before != EPMEM_MEMID_NONE && after != EPMEM_MEMID_NONE && before <= after) {
		epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_status, my_agent->epmem_sym_bad_cmd);
		return;
	}

	if (QUERY_DEBUG >= 1) {
		std::cout << std::endl << "==========================" << std::endl << std::endl;
	}

    epmem_query* query = construct_epmem_query(my_agent, state, pos_query, neg_query, prohibits, before, after, currents, cue_wmes, level);
	

#ifdef USE_MPI
	//MPI::COMM_WORLD.Barrier();
	double wtime = MPI::Wtime ();	
	query_rsp_data* response = send_epmem_query_message(query);
	std::cout << "Query time: " << MPI::Wtime() - wtime << std::endl;
#else
    // SEARCH QUERY
    query_rsp_data* r1 = my_agent->epmem_worker_p->epmem_perform_query(query);
    query_rsp_data* response = r1;
    
    delete query;
    query = construct_epmem_query(my_agent, state, pos_query, neg_query, prohibits, before, after, currents, cue_wmes, level);
    query_rsp_data* r2 = my_agent->epmem_worker_p2->epmem_perform_query(query);
    
    if((r1->best_score > r2->best_score) ||
        (query->do_graph_match && r1->best_graph_matched && !r2->best_graph_matched)){
            response = r1;
            delete r2;
    } else {
        response = r2;
        delete r1;
    }


#endif

	my_agent->epmem_timers->query->start();

    {
		// if the best episode is the default, fail
		// otherwise, put the episode in working memory
		if (response->best_episode == EPMEM_MEMID_NONE) {
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
			temp_sym = make_int_constant(my_agent, response->leaf_literals_size);
			epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_cue_size, temp_sym);
			symbol_remove_ref(my_agent, temp_sym);
			// match cardinality
			temp_sym = make_int_constant(my_agent, response->best_cardinality);
			epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_match_cardinality, temp_sym);
			symbol_remove_ref(my_agent, temp_sym);
			// match score
			temp_sym = make_float_constant(my_agent, response->best_score);
			epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_match_score, temp_sym);
			symbol_remove_ref(my_agent, temp_sym);
			// normalized match score
			temp_sym = make_float_constant(my_agent, response->best_score / response->perfect_score);
			epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_normalized_match_score, temp_sym);
			symbol_remove_ref(my_agent, temp_sym);
			// status
			epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_success, pos_query);
			if (neg_query) {
				epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_success, neg_query);
			}
			// give more metadata if graph match is turned on
			if (query->do_graph_match) {
				// graph match
				temp_sym = make_int_constant(my_agent, (response->best_graph_matched ? 1 : 0));
				epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_graph_match, temp_sym);
				symbol_remove_ref(my_agent, temp_sym);

                // E587: AM: XXX: Need to add back in
				// mapping
				//if (response->best_graph_matched) {
				//	goal_stack_level level = state->id.epmem_result_header->id.level;
				//	// mapping identifier
				//	Symbol* mapping = make_new_identifier(my_agent, 'M', level);
				//	epmem_buffer_add_wme(meta_wmes, state->id.epmem_result_header, my_agent->epmem_sym_graph_match_mapping, mapping);
				//	symbol_remove_ref(my_agent, mapping);

				//	for (epmem_literal_node_pair_map::iterator iter = response->best_bindings.begin(); iter != response->best_bindings.end(); iter++) {
				//		if ((*iter).first->value_is_id) {
				//			// create the node
				//			temp_sym = make_new_identifier(my_agent, 'N', level);
				//			epmem_buffer_add_wme(meta_wmes, mapping, my_agent->epmem_sym_graph_match_mapping_node, temp_sym);
				//			symbol_remove_ref(my_agent, temp_sym);
				//			// point to the cue identifier
				//			epmem_buffer_add_wme(meta_wmes, temp_sym, my_agent->epmem_sym_graph_match_mapping_cue, (*iter).first->value_sym);
				//			// save the mapping point for the episode
				//			node_map_map[(*iter).second.second] = temp_sym;
				//			node_mem_map[(*iter).second.second] = NULL;
				//		}
				//	}
				//}
			}
			// reconstruct the actual episode
		
			if (level > 2) {
				epmem_install_memory(my_agent, state, response->best_episode, meta_wmes, retrieval_wmes, &node_mem_map);
			}
			if (response->best_graph_matched) {
				for (epmem_id_mapping::iterator iter = node_mem_map.begin(); iter != node_mem_map.end(); iter++) {
					epmem_id_mapping::iterator map_iter = node_map_map.find((*iter).first);
					if (map_iter != node_map_map.end() && (*iter).second) {
						epmem_buffer_add_wme(meta_wmes, (*map_iter).second, my_agent->epmem_sym_retrieved, (*iter).second);
					}
				}
			}
			my_agent->epmem_timers->query_result->stop();
		}
	}

	my_agent->epmem_timers->query_cleanup->stop();

	my_agent->epmem_timers->query->stop();
}


//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Visualization (epmem::viz)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

inline std::string _epmem_print_sti( epmem_node_id id )
{
	std::string t1, t2;

	t1.assign( "<id" );

	to_string( id, t2 );
	t1.append( t2 );
	t1.append( ">" );

	return t1;
}

void epmem_print_episode( agent* my_agent, epmem_time_id memory_id, std::string* buf )
{
	// if this is before the first episode, initialize db components
	if ( my_agent->epmem_db->get_status() == soar_module::disconnected )
	{
		epmem_init_db( my_agent );
	}

    if(my_agent->epmem_worker_p == NIL){
        return;
    }

	// if bad memory, bail
	buf->clear();
	if ( ( memory_id == EPMEM_MEMID_NONE ) ||
			!epmem_valid_episode( my_agent, memory_id ) )
	{
		return;
	}

	// fill episode map
	std::map< epmem_node_id, std::string > ltis;
	std::map< epmem_node_id, std::map< std::string, std::list< std::string > > > ep;
	{
		soar_module::sqlite_statement *my_q;
		std::string temp_s, temp_s2, temp_s3;
		double temp_d;
		int64_t temp_i;

		// E587: AM:
		{
			epmem_node_id q0;
			epmem_node_id q1;
			int64_t w_type;
			bool val_is_short_term;

			soar_module::sqlite_statement* get_edge_desc = my_agent->epmem_stmts_master->get_edge_desc;

			std::vector<epmem_node_id> edge_ids;
			my_agent->epmem_worker_p->get_edges_at_episode(memory_id, &edge_ids);

			for(int i = 0; i < edge_ids.size(); i++){
				get_edge_desc->bind_int(1, memory_id);
				get_edge_desc->bind_int(2, edge_ids[i]);
				if(get_edge_desc->execute() != soar_module::row){
					get_edge_desc->reinitialize();
					continue;
				}
				q0 = get_edge_desc->column_int( 0 );
				q1 = get_edge_desc->column_int( 2 );
				w_type = get_edge_desc->column_int( 3 );
				val_is_short_term = ( get_edge_desc->column_type( 4 ) == soar_module::null_t );

				switch ( w_type )
				{
					case INT_CONSTANT_SYMBOL_TYPE:
						temp_i = static_cast<int64_t>( get_edge_desc->column_int( 1 ) );
						to_string( temp_i, temp_s );
						break;

					case FLOAT_CONSTANT_SYMBOL_TYPE:
						temp_d = get_edge_desc->column_double( 1 );
						to_string( temp_d, temp_s );
						break;

					case SYM_CONSTANT_SYMBOL_TYPE:
						temp_s.assign( const_cast<char *>( reinterpret_cast<const char *>( get_edge_desc->column_text( 1 ) ) ) );
						break;
				}

				if ( val_is_short_term )
				{
					temp_s2 = _epmem_print_sti( q1 );
				}
				else
				{
					temp_s2.assign( "@" );
					temp_s2.push_back( static_cast< char >( get_edge_desc->column_int( 4 ) ) );

					temp_i = static_cast< uint64_t >( get_edge_desc->column_int( 5 ) );
					to_string( temp_i, temp_s3 );
					temp_s2.append( temp_s3 );

					ltis[ q1 ] = temp_s2;
				}

				ep[ q0 ][ temp_s ].push_back( temp_s2 );
				get_edge_desc->reinitialize();
			}
		}

		// E587: AM:
		{
			epmem_node_id parent_id;
			int64_t attr_type, value_type;
			
			soar_module::sqlite_statement* get_node_desc = my_agent->epmem_stmts_master->get_node_desc;

			std::vector<epmem_node_id> node_ids;
			my_agent->epmem_worker_p->get_nodes_at_episode(memory_id, &node_ids);

			for(int i = 0; i < node_ids.size(); i++){
				get_node_desc->bind_int(1, node_ids[i]);
				if(get_node_desc->execute() != soar_module::row){
					get_node_desc->reinitialize();
					continue;
				}
				parent_id = get_node_desc->column_int( 1 );
				attr_type = get_node_desc->column_int( 4 );
				value_type = get_node_desc->column_int( 5 );

				switch ( attr_type )
				{
					case INT_CONSTANT_SYMBOL_TYPE:
						temp_i = static_cast<int64_t>( get_node_desc->column_int( 2 ) );
						to_string( temp_i, temp_s );
						break;

					case FLOAT_CONSTANT_SYMBOL_TYPE:
						temp_d = get_node_desc->column_double( 2 );
						to_string( temp_d, temp_s );
						break;

					case SYM_CONSTANT_SYMBOL_TYPE:
						temp_s.assign( const_cast<char *>( reinterpret_cast<const char *>( get_node_desc->column_text( 2 ) ) ) );
						break;
				}

				switch ( value_type )
				{
					case INT_CONSTANT_SYMBOL_TYPE:
						temp_i = static_cast<int64_t>( get_node_desc->column_int( 3 ) );
						to_string( temp_i, temp_s2 );
						break;

					case FLOAT_CONSTANT_SYMBOL_TYPE:
						temp_d = get_node_desc->column_double( 3 );
						to_string( temp_d, temp_s2 );
						break;

					case SYM_CONSTANT_SYMBOL_TYPE:
						temp_s2.assign( const_cast<char *>( reinterpret_cast<const char *>( get_node_desc->column_text( 3 ) ) ) );
						break;
				}

				ep[ parent_id ][ temp_s ].push_back( temp_s2 );
				get_node_desc->reinitialize();
			}
		}
	}

	// output
	{
		std::map< epmem_node_id, std::string >::iterator lti_it;
		std::map< epmem_node_id, std::map< std::string, std::list< std::string > > >::iterator ep_it;
		std::map< std::string, std::list< std::string > >::iterator slot_it;
		std::list< std::string >::iterator val_it;

		for ( ep_it=ep.begin(); ep_it!=ep.end(); ep_it++ )
		{
			buf->append( "(" );

			// id
			lti_it = ltis.find( ep_it->first );
			if ( lti_it == ltis.end() )
			{
				buf->append( _epmem_print_sti( ep_it->first ) );
			}
			else
			{
				buf->append( lti_it->second );
			}

			// attr
			for ( slot_it=ep_it->second.begin(); slot_it!=ep_it->second.end(); slot_it++ )
			{
				buf->append( " ^" );
				buf->append( slot_it->first );

				for ( val_it=slot_it->second.begin(); val_it!=slot_it->second.end(); val_it++ )
				{
					buf->append( " " );
					buf->append( *val_it );
				}
			}

			buf->append( ")\n" );
		}
	}
}

void epmem_visualize_episode( agent* my_agent, epmem_time_id memory_id, std::string* buf )
{
	// if this is before the first episode, initialize db components
	if ( my_agent->epmem_db->get_status() == soar_module::disconnected )
	{
		epmem_init_db( my_agent );
	}

    if(my_agent->epmem_worker_p == NIL){
        return;
    }

	// if bad memory, bail
	buf->clear();
	if ( ( memory_id == EPMEM_MEMID_NONE ) ||
			!epmem_valid_episode( my_agent, memory_id ) )
	{
		return;
	}

	// init
	{
		buf->append( "digraph epmem {\n" );
	}

	// taken heavily from install
	{
		soar_module::sqlite_statement *my_q;

		// E587: AM:
		// first identifiers (i.e. reconstruct)
		{
			// for printing
			std::map< epmem_node_id, std::string > stis;
			std::map< epmem_node_id, std::pair< std::string, std::string > > ltis;
			std::list< std::string > edges;
			std::map< epmem_node_id, std::string >::iterator sti_p;
			std::map< epmem_node_id, std::pair< std::string, std::string > >::iterator lti_p;

			// relates to finite automata: q1 = d(q0, w)
			epmem_node_id q0; // id
			epmem_node_id q1; // attribute
			int64_t w_type; // we support any constant attribute symbol
			std::string temp, temp2, temp3, temp4;
			double temp_d;
			int64_t temp_i;

			bool val_is_short_term;
			char val_letter;
			uint64_t val_num;

			// 0 is magic
			temp.assign( "ID_0" );
			stis.insert( std::make_pair< epmem_node_id, std::string >( 0, temp ) );

			soar_module::sqlite_statement* get_edge_desc = my_agent->epmem_stmts_master->get_edge_desc;

			std::vector<epmem_node_id> edge_ids;
			my_agent->epmem_worker_p->get_edges_at_episode(memory_id, &edge_ids);

			for(int i = 0; i < edge_ids.size(); i++){
				get_edge_desc->bind_int(1, memory_id);
				get_edge_desc->bind_int(2, edge_ids[i]);
				if(get_edge_desc->execute() != soar_module::row){
					get_edge_desc->reinitialize();
					continue;
				}
				// q0, w, q1, w_type, letter, num
				q0 = get_edge_desc->column_int( 0 );
				q1 = get_edge_desc->column_int( 2 );
				w_type = get_edge_desc->column_int( 3 );

				// "ID_Q0"
				temp.assign( "ID_" );
				to_string( q0, temp2 );
				temp.append( temp2 );

				// "ID_Q1"
				temp3.assign( "ID_" );
				to_string( q1, temp2 );
				temp3.append( temp2 );

				val_is_short_term = ( get_edge_desc->column_type( 4 ) == soar_module::null_t );
				if ( val_is_short_term )
				{
					sti_p = stis.find( q1 );
					if ( sti_p == stis.end() )
					{
						stis.insert( std::make_pair< epmem_node_id, std::string >( q1, temp3 ) );
					}
				}
				else
				{
					lti_p = ltis.find( q1 );

					if ( lti_p == ltis.end() )
					{
						// "L#"
						val_letter = static_cast<char>( get_edge_desc->column_int( 4 ) );
						to_string( val_letter, temp4 );
						val_num = static_cast<uint64_t>( get_edge_desc->column_int( 5 ) );
						to_string( val_num, temp2 );
						temp4.append( temp2 );

						ltis.insert( std::make_pair< epmem_node_id, std::pair< std::string, std::string > >( q1, std::make_pair< std::string, std::string >( temp3, temp4 ) ) );
					}
				}

				// " -> ID_Q1"
				temp.append( " -> " );
				temp.append( temp3 );

				// " [ label="w" ];\n"
				temp.append( " [ label=\"" );
				switch ( w_type )
				{
					case INT_CONSTANT_SYMBOL_TYPE:
						temp_i = static_cast<int64_t>( get_edge_desc->column_int( 1 ) );
						to_string( temp_i, temp2 );
						break;

					case FLOAT_CONSTANT_SYMBOL_TYPE:
						temp_d = get_edge_desc->column_double( 1 );
						to_string( temp_d, temp2 );
						break;

					case SYM_CONSTANT_SYMBOL_TYPE:
						temp2.assign( const_cast<char *>( reinterpret_cast<const char *>( get_edge_desc->column_text( 1 ) ) ) );
						break;
				}
				temp.append( temp2 );
				temp.append( "\" ];\n" );

				edges.push_back( temp );
				get_edge_desc->reinitialize();
			}

			// identifiers
			{
				// short-term
				{
					buf->append( "node [ shape = circle ];\n" );

					for ( sti_p=stis.begin(); sti_p!=stis.end(); sti_p++ )
					{
						buf->append( sti_p->second );
						buf->append( " " );
					}

					buf->append( ";\n" );
				}

				// long-term
				{
					buf->append( "node [ shape = doublecircle ];\n" );

					for ( lti_p=ltis.begin(); lti_p!=ltis.end(); lti_p++ )
					{
						buf->append( lti_p->second.first );
						buf->append( " [ label=\"" );
						buf->append( lti_p->second.second );
						buf->append( "\" ];\n" );
					}

					buf->append( "\n" );
				}
			}

			// edges
			{
				std::list< std::string >::iterator e_p;

				for ( e_p=edges.begin(); e_p!=edges.end(); e_p++ )
				{
					buf->append( (*e_p) );
				}
			}
		}

		// E587: AM:
		// then node_unique
		{
			epmem_node_id child_id;
			epmem_node_id parent_id;
			int64_t attr_type;
			int64_t value_type;

			std::list< std::string > edges;
			std::list< std::string > consts;

			std::string temp, temp2;
			double temp_d;
			int64_t temp_i;

			soar_module::sqlite_statement* get_node_desc = my_agent->epmem_stmts_master->get_node_desc;

			std::vector<epmem_node_id> node_ids;
			my_agent->epmem_worker_p->get_nodes_at_episode(memory_id, &node_ids);

			for(int i = 0; i < node_ids.size(); i++){
				get_node_desc->bind_int(1, node_ids[i]);
				if(get_node_desc->execute() != soar_module::row){
					get_node_desc->reinitialize();
					continue;
				}
				// f.child_id, f.parent_id, f.name, f.value, f.attr_type, f.value_type
				child_id = get_node_desc->column_int( 0 );
				parent_id = get_node_desc->column_int( 1 );
				attr_type = get_node_desc->column_int( 4 );
				value_type = get_node_desc->column_int( 5 );

				temp.assign( "ID_" );
				to_string( parent_id, temp2 );
				temp.append( temp2 );
				temp.append( " -> C_" );
				to_string( child_id, temp2 );
				temp.append( temp2 );
				temp.append( " [ label=\"" );

				// make a symbol to represent the attribute
				switch ( attr_type )
				{
					case INT_CONSTANT_SYMBOL_TYPE:
						temp_i = static_cast<int64_t>( get_node_desc->column_int( 2 ) );
						to_string( temp_i, temp2 );
						break;

					case FLOAT_CONSTANT_SYMBOL_TYPE:
						temp_d = get_node_desc->column_double( 2 );
						to_string( temp_d, temp2 );
						break;

					case SYM_CONSTANT_SYMBOL_TYPE:
						temp2.assign( const_cast<char *>( reinterpret_cast<const char *>( get_node_desc->column_text( 2 ) ) ) );
						break;
				}

				temp.append( temp2 );
				temp.append( "\" ];\n" );
				edges.push_back( temp );

				temp.assign( "C_" );
				to_string( child_id, temp2 );
				temp.append( temp2 );
				temp.append( " [ label=\"" );

				// make a symbol to represent the value
				switch ( value_type )
				{
					case INT_CONSTANT_SYMBOL_TYPE:
						temp_i = static_cast<int64_t>( get_node_desc->column_int( 3 ) );
						to_string( temp_i, temp2 );
						break;

					case FLOAT_CONSTANT_SYMBOL_TYPE:
						temp_d = get_node_desc->column_double( 3 );
						to_string( temp_d, temp2 );
						break;

					case SYM_CONSTANT_SYMBOL_TYPE:
						temp2.assign( const_cast<char *>( reinterpret_cast<const char *>( get_node_desc->column_text( 3 ) ) ) );
						break;
				}

				temp.append( temp2 );
				temp.append( "\" ];\n" );

				consts.push_back( temp );
				get_node_desc->reinitialize();
			}

			// constant nodes
			{
				std::list< std::string >::iterator e_p;

				buf->append( "node [ shape = plaintext ];\n" );

				for ( e_p=consts.begin(); e_p!=consts.end(); e_p++ )
				{
					buf->append( (*e_p) );
				}
			}

			// edges
			{
				std::list< std::string >::iterator e_p;

				for ( e_p=edges.begin(); e_p!=edges.end(); e_p++ )
				{
					buf->append( (*e_p) );
				}
			}
		}
	}

	// close
	{
		buf->append( "\n}\n" );
	}
}

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// API Implementation (epmem::api)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_consider_new_episode
 * Author		: Nate Derbinsky
 * Notes		: Based upon trigger/force parameter settings, potentially
 * 				  records a new episode
 **************************************************************************/
bool epmem_consider_new_episode( agent *my_agent )
{
	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->trigger->start();
	////////////////////////////////////////////////////////////////////////////

	const int64_t force = my_agent->epmem_params->force->get_value();
	bool new_memory = false;

	if ( force == epmem_param_container::force_off )
	{
		const int64_t trigger = my_agent->epmem_params->trigger->get_value();

		if ( trigger == epmem_param_container::output )
		{
			slot *s;
			wme *w;
			Symbol *ol = my_agent->io_header_output;

			// examine all commands on the output-link for any
			// that appeared since last memory was recorded
			for ( s = ol->id.slots; s != NIL; s = s->next )
			{
				for ( w = s->wmes; w != NIL; w = w->next )
				{
					if ( w->timetag > my_agent->top_goal->id.epmem_info->last_ol_time )
					{
						new_memory = true;
						my_agent->top_goal->id.epmem_info->last_ol_time = w->timetag;
					}
				}
			}
		}
		else if ( trigger == epmem_param_container::dc )
		{
			new_memory = true;
		}
		else if ( trigger == epmem_param_container::none )
		{
			new_memory = false;
		}
	}
	else
	{
		new_memory = ( force == epmem_param_container::remember );

		my_agent->epmem_params->force->set_value( epmem_param_container::force_off );
	}

	////////////////////////////////////////////////////////////////////////////
	my_agent->epmem_timers->trigger->stop();
	////////////////////////////////////////////////////////////////////////////

	if ( new_memory )
	{
		epmem_new_episode( my_agent );
	}

	return new_memory;
}

void inline _epmem_respond_to_cmd_parse( agent* my_agent, epmem_wme_list* cmds, bool& good_cue, int& path, epmem_time_id& retrieve, Symbol*& next, Symbol*& previous, Symbol*& query, Symbol*& neg_query, epmem_time_list& prohibit, epmem_time_id& before, epmem_time_id& after, epmem_symbol_set& currents, soar_module::wme_set& cue_wmes )
{
	cue_wmes.clear();

	retrieve = EPMEM_MEMID_NONE;
	next = NULL;
	previous = NULL;
	query = NULL;
	neg_query = NULL;
	prohibit.clear();
	before = EPMEM_MEMID_NONE;
	after = EPMEM_MEMID_NONE;
	good_cue = true;
	path = 0;

	for ( epmem_wme_list::iterator w_p=cmds->begin(); w_p!=cmds->end(); w_p++ )
	{
		cue_wmes.insert( (*w_p) );

		if ( good_cue )
		{
			// collect information about known commands
			if ( (*w_p)->attr == my_agent->epmem_sym_retrieve )
			{
				if ( ( (*w_p)->value->ic.common_symbol_info.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&
						( path == 0 ) &&
						( (*w_p)->value->ic.value > 0 ) )
				{
					retrieve = (*w_p)->value->ic.value;
					path = 1;
				}
				else
				{
					good_cue = false;
				}
			}
			else if ( (*w_p)->attr == my_agent->epmem_sym_next )
			{
				if ( ( (*w_p)->value->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&
						( path == 0 ) )
				{
					next = (*w_p)->value;
					path = 2;
				}
				else
				{
					good_cue = false;
				}
			}
			else if ( (*w_p)->attr == my_agent->epmem_sym_prev )
			{
				if ( ( (*w_p)->value->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&
						( path == 0 ) )
				{
					previous = (*w_p)->value;
					path = 2;
				}
				else
				{
					good_cue = false;
				}
			}
			else if ( (*w_p)->attr == my_agent->epmem_sym_query )
			{
				if ( ( (*w_p)->value->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&
						( ( path == 0 ) || ( path == 3 ) ) &&
						( query == NULL ) )

				{
					query = (*w_p)->value;
					path = 3;
				}
				else
				{
					good_cue = false;
				}
			}
			else if ( (*w_p)->attr == my_agent->epmem_sym_negquery )
			{
				if ( ( (*w_p)->value->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&
						( ( path == 0 ) || ( path == 3 ) ) &&
						( neg_query == NULL ) )

				{
					neg_query = (*w_p)->value;
					path = 3;
				}
				else
				{
					good_cue = false;
				}
			}
			else if ( (*w_p)->attr == my_agent->epmem_sym_before )
			{
				if ( ( (*w_p)->value->ic.common_symbol_info.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&
						( ( path == 0 ) || ( path == 3 ) ) )
				{
					if ( ( before == EPMEM_MEMID_NONE ) || ( (*w_p)->value->ic.value < before ) )
					{
						before = (*w_p)->value->ic.value;
					}
					path = 3;
				}
				else
				{
					good_cue = false;
				}
			}
			else if ( (*w_p)->attr == my_agent->epmem_sym_after )
			{
				if ( ( (*w_p)->value->ic.common_symbol_info.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&
						( ( path == 0 ) || ( path == 3 ) ) )
				{
					if ( after < (*w_p)->value->ic.value )
					{
						after = (*w_p)->value->ic.value;
					}
					path = 3;
				}
				else
				{
					good_cue = false;
				}
			}
			else if ( (*w_p)->attr == my_agent->epmem_sym_prohibit )
			{
				if ( ( (*w_p)->value->ic.common_symbol_info.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&
						( ( path == 0 ) || ( path == 3 ) ) )
				{
					prohibit.push_back( (*w_p)->value->ic.value );
					path = 3;
				}
				else
				{
					good_cue = false;
				}
			}
			else if ( (*w_p)->attr == my_agent->epmem_sym_current )
			{
				if ( ( (*w_p)->value->ic.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&
						( ( path == 0 ) || ( path == 3 ) ) )
				{
					currents.insert( (*w_p)->value );
					path = 3;
				}
				else
				{
					good_cue = false;
				}
			}
			else
			{
				good_cue = false;
			}
		}
	}

	// if on path 3 must have query
	if ( ( path == 3 ) && ( query == NULL ) )
	{
		good_cue = false;
	}

	// must be on a path
	if ( path == 0 )
	{
		good_cue = false;
	}
}

/***************************************************************************
 * Function     : epmem_respond_to_cmd
 * Author		: Nate Derbinsky
 * Notes		: Implements the Soar-EpMem API
 **************************************************************************/
void epmem_respond_to_cmd( agent *my_agent )
{
	// if this is before the first episode, initialize db components
	if ( my_agent->epmem_db->get_status() == soar_module::disconnected )
	{
		epmem_init_db( my_agent );
	}

	// respond to query only if db is properly initialized
	if ( my_agent->epmem_db->get_status() != soar_module::connected )
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
		////////////////////////////////////////////////////////////////////////////
		my_agent->epmem_timers->api->start();
		////////////////////////////////////////////////////////////////////////////

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
					for ( w_p=wmes->begin(); w_p!=wmes->end(); w_p++ )
					{
						wme_count++;

						if ( (*w_p)->timetag > state->id.epmem_info->last_cmd_time )
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
//E587: JK: now we know cue is good can have commands sent to epmem_manager
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
				    //E587 create and send message to epmem_manager to handle query
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
			////////////////////////////////////////////////////////////////////////////
			my_agent->epmem_timers->api->stop();
			////////////////////////////////////////////////////////////////////////////
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
		////////////////////////////////////////////////////////////////////////////
		my_agent->epmem_timers->wm_phase->start();
		////////////////////////////////////////////////////////////////////////////

		do_working_memory_phase( my_agent );

		////////////////////////////////////////////////////////////////////////////
		my_agent->epmem_timers->wm_phase->stop();
		////////////////////////////////////////////////////////////////////////////
	}
}

#ifdef EPMEM_EXPERIMENT
void inline _epmem_exp( agent* my_agent )
{
	// hopefully generally useful code for evaluating
	// query speed as number of episodes increases
	// usage: top-state.epmem.queries <q>
	//        <q> ^reps #
	//            ^mod # (optional, defaults to 1)
	//            ^output |filename|
	//            ^format << csv speedy >>
	//            ^features <fs>
	//               <fs> ^|key| |value| # note: value must be a string, not int or float; wrap it in pipes (eg. |0|)
	//            ^commands <cmds>
	//               <cmds> ^|label| <cmd>

	if ( !epmem_exp_timer )
	{
		epmem_exp_timer = new soar_module::timer( "exp", my_agent, soar_module::timer::zero, new soar_module::predicate<soar_module::timer::timer_level>(), false );
	}

	double c1;

	epmem_exp_timer->reset();
	epmem_exp_timer->start();
	epmem_dc_wme_adds = -1;
	bool epmem_episode_diff = epmem_consider_new_episode( my_agent );
	epmem_exp_timer->stop();
	c1 = epmem_exp_timer->value();

	if ( epmem_episode_diff )
	{
		Symbol* queries = make_sym_constant( my_agent, "queries" );
		Symbol* reps = make_sym_constant( my_agent, "reps" );
		Symbol* mod = make_sym_constant( my_agent, "mod" );
		Symbol* output = make_sym_constant( my_agent, "output" );
		Symbol* format = make_sym_constant( my_agent, "format" );
		Symbol* features = make_sym_constant( my_agent, "features" );
		Symbol* cmds = make_sym_constant( my_agent, "commands" );
		Symbol* csv = make_sym_constant( my_agent, "csv" );

		slot* queries_slot = find_slot( my_agent->top_goal->id.epmem_header, queries );
		if ( queries_slot )
		{
			wme* queries_wme = queries_slot->wmes;

			if ( queries_wme->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
			{
				Symbol* queries_id = queries_wme->value;
				slot* reps_slot = find_slot( queries_id, reps );
				slot* mod_slot = find_slot( queries_id, mod );
				slot* output_slot = find_slot( queries_id, output );
				slot* format_slot = find_slot( queries_id, format );
				slot* features_slot = find_slot( queries_id, features );
				slot* commands_slot = find_slot( queries_id, cmds );

				if ( reps_slot && output_slot && format_slot && commands_slot )
				{
					wme* reps_wme = reps_slot->wmes;
					wme* mod_wme = ( ( mod_slot )?( mod_slot->wmes ):( NULL ) );
					wme* output_wme = output_slot->wmes;
					wme* format_wme = format_slot->wmes;
					wme* features_wme = ( ( features_slot )?( features_slot->wmes ):( NULL ) );
					wme* commands_wme = commands_slot->wmes;

					if ( ( reps_wme->value->common.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&
							( output_wme->value->common.symbol_type == SYM_CONSTANT_SYMBOL_TYPE ) &&
							( format_wme->value->common.symbol_type == SYM_CONSTANT_SYMBOL_TYPE ) &&
							( commands_wme->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE ) )
					{
						int64_t reps = reps_wme->value->ic.value;
						int64_t mod = ( ( mod_wme && ( mod_wme->value->common.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) )?( mod_wme->value->ic.value ):(1) );
						const char* output_fname = output_wme->value->sc.name;
						bool format_csv = ( format_wme->value == csv );
						std::list< std::pair< std::string, std::string > > output_contents;
						std::string temp_str, temp_str2;
						std::set< std::string > cmd_names;

						std::map< std::string, std::string > features;
						if ( features_wme && features_wme->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
						{
							for ( slot* s=features_wme->value->id.slots; s; s=s->next )
							{
								for ( wme* w=s->wmes; w; w=w->next )
								{
									if ( ( w->attr->common.symbol_type == SYM_CONSTANT_SYMBOL_TYPE ) &&
											( w->value->common.symbol_type == SYM_CONSTANT_SYMBOL_TYPE ) )
									{
										features[ w->attr->sc.name ] = w->value->sc.name;
									}
								}
							}
						}

						if ( ( mod == 1 ) || ( ( ( my_agent->epmem_stats->time->get_value()-1 ) % mod ) == 1 ) )
						{

							// all fields (used to produce csv header), possibly stub values at this point
							{
								// features
								for ( std::map< std::string, std::string >::iterator f_it=features.begin(); f_it!=features.end(); f_it++ )
								{
									output_contents.push_back( std::make_pair< std::string, std::string >( f_it->first, f_it->second ) );
								}

								// decision
								{
									to_string( my_agent->d_cycle_count, temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "dc", temp_str ) );
								}

								// episode number
								{
									to_string( my_agent->epmem_stats->time->get_value()-1, temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "episodes", temp_str ) );
								}

								// reps
								{
									to_string( reps, temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "reps", temp_str ) );
								}

								// current wm size
								{
									to_string( my_agent->num_wmes_in_rete, temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "wmcurrent", temp_str ) );
								}

								// wm adds/removes
								{
									to_string( ( my_agent->wme_addition_count - epmem_exp_state[ exp_state_wm_adds ] ), temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "wmadds", temp_str ) );
									epmem_exp_state[ exp_state_wm_adds ] = static_cast< int64_t >( my_agent->wme_addition_count );

									to_string( ( my_agent->wme_removal_count - epmem_exp_state[ exp_state_wm_removes ] ), temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "wmremoves", temp_str ) );
									epmem_exp_state[ exp_state_wm_removes ] = static_cast< int64_t >( my_agent->wme_removal_count );
								}

								// dc interval add/removes
								{
									to_string( epmem_dc_interval_inserts, temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "dcintervalinserts", temp_str ) );

									to_string( epmem_dc_interval_removes, temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "dcintervalremoves", temp_str ) );
								}

								// dc wme adds
								{
									to_string( epmem_dc_wme_adds, temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "dcwmeadds", temp_str ) );
								}

								// sqlite memory
								{
									int64_t sqlite_mem = my_agent->epmem_stats->mem_usage->get_value();

									to_string( ( sqlite_mem - epmem_exp_state[ exp_state_sqlite_mem ] ), temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "sqlitememadd", temp_str ) );
									epmem_exp_state[ exp_state_sqlite_mem ] = sqlite_mem;

									to_string( sqlite_mem, temp_str );
									output_contents.push_back( std::make_pair< std::string, std::string >( "sqlitememcurrent", temp_str ) );
								}

								// storage time in seconds
								{
									to_string( c1, temp_str );

									output_contents.push_back( std::make_pair< std::string, std::string >( "storage", temp_str ) );
									cmd_names.insert( "storage" );
								}

								// commands
								for ( slot* s=commands_wme->value->id.slots; s; s=s->next )
								{
									output_contents.push_back( std::make_pair< std::string, std::string >( s->attr->sc.name, "" ) );
									std::string searched = "numsearched";
									searched.append(s->attr->sc.name);
									output_contents.push_back( std::make_pair< std::string, std::string >( searched , "" ) );
								}
							}

							// open file, write header
							if ( !epmem_exp_output )
							{
								epmem_exp_output = new std::ofstream( output_fname );

								if ( format_csv )
								{
									for ( std::list< std::pair< std::string, std::string > >::iterator it=output_contents.begin(); it!=output_contents.end(); it++ )
									{
										if ( it != output_contents.begin() )
										{
											(*epmem_exp_output) << ",";
										}

										(*epmem_exp_output) << "\"" << it->first << "\"";
									}

									(*epmem_exp_output) << std::endl;
								}
							}

							// collect timing data
							{
								epmem_wme_list* cmds = NULL;
								soar_module::wme_set cue_wmes;
								soar_module::symbol_triple_list meta_wmes;
								soar_module::symbol_triple_list retrieval_wmes;

								epmem_time_id retrieve;
								Symbol* next;
								Symbol* previous;
								Symbol* query;
								Symbol* neg_query;
								epmem_time_list prohibit;
								epmem_time_id before, after;
#ifdef USE_MEM_POOL_ALLOCATORS
								epmem_symbol_set currents = epmem_symbol_set( std::less< Symbol* >(), soar_module::soar_memory_pool_allocator< Symbol* >( my_agent ) );
#else
								epmem_symbol_set currents = epmem_symbol_set();
#endif
								bool good_cue;
								int path;

								//

								for ( slot* s=commands_wme->value->id.slots; s; s=s->next )
								{
									if ( s->wmes->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
									{
										// parse command once
										{
											cmds = epmem_get_augs_of_id( s->wmes->value, get_new_tc_number( my_agent ) );
											_epmem_respond_to_cmd_parse( my_agent, cmds, good_cue, path, retrieve, next, previous, query, neg_query, prohibit, before, after, currents, cue_wmes );
										}

										if ( good_cue && ( path == 3 ) )
										{
											// execute lots of times
											double c_total = 0;
											{
												epmem_exp_timer->reset();
												epmem_exp_timer->start();
												for ( int64_t i=1; i<=reps; i++ )
												{
													epmem_process_query( my_agent, my_agent->top_goal, query, neg_query, prohibit, before, after, currents, cue_wmes, meta_wmes, retrieval_wmes, 2 );

													if ( !retrieval_wmes.empty() || !meta_wmes.empty() )
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
												}
												epmem_exp_timer->stop();
												c_total = epmem_exp_timer->value();
											}

											// update results
											{
												to_string( c_total, temp_str );

												for ( std::list< std::pair< std::string, std::string > >::iterator oc_it=output_contents.begin(); oc_it!=output_contents.end(); oc_it++ )
												{
													if ( oc_it->first.compare( s->attr->sc.name ) == 0 )
													{
														oc_it->second.assign( temp_str );
														oc_it++;
														to_string( epmem_episodes_searched , temp_str );
														oc_it->second.assign( temp_str );
													}
												}

												cmd_names.insert( s->attr->sc.name );
											}
										}

										// clean
										{
											delete cmds;
										}
									}
								}
							}

							// output data
							if ( format_csv )
							{
								for ( std::list< std::pair< std::string, std::string > >::iterator it=output_contents.begin(); it!=output_contents.end(); it++ )
								{
									if ( it != output_contents.begin() )
									{
										(*epmem_exp_output) << ",";
									}

									(*epmem_exp_output) << "\"" << it->second << "\"";
								}

								(*epmem_exp_output) << std::endl;
							}
							else
							{
								for ( std::set< std::string >::iterator c_it=cmd_names.begin(); c_it!=cmd_names.end(); c_it++ )
								{
									for ( std::list< std::pair< std::string, std::string > >::iterator it=output_contents.begin(); it!=output_contents.end(); it++ )
									{
										if ( cmd_names.find( it->first ) == cmd_names.end() )
										{
											if ( it->first.substr( 0, 11 ).compare( "numsearched" ) == 0 )
											{
												continue;
											}

											if ( it != output_contents.begin() )
											{
												(*epmem_exp_output) << " ";
											}
											if ( ( it->first.compare( "reps" ) == 0 ) && ( c_it->compare( "storage" ) == 0 ) )
											{
												(*epmem_exp_output) << it->first << "=" << "1";
											}
											else
											{
												(*epmem_exp_output) << it->first << "=" << it->second;
											}
										}
										else if ( c_it->compare( it->first ) == 0 )
										{
											(*epmem_exp_output) << " command=" << it->first << " totalsec=" << it->second;
											if ( it->first.compare( "storage" ) == 0 ) {
												(*epmem_exp_output) << " numsearched=0";
												break;
											} else {
												it++;
												(*epmem_exp_output) << " numsearched=" << it->second;
												break;
											}
										}
									}

									(*epmem_exp_output) << std::endl;
								}
							}
						}
					}
				}
			}
		}

		symbol_remove_ref( my_agent, queries );
		symbol_remove_ref( my_agent, reps );
		symbol_remove_ref( my_agent, mod );
		symbol_remove_ref( my_agent, output );
		symbol_remove_ref( my_agent, format );
		symbol_remove_ref( my_agent, features );
		symbol_remove_ref( my_agent, cmds );
		symbol_remove_ref( my_agent, csv );
	}
}
#endif

/***************************************************************************
 * Function     : epmem_go
 * Author		: Nate Derbinsky
 * Notes		: The kernel calls this function to implement Soar-EpMem:
 * 				  consider new storage and respond to any commands
 **************************************************************************/
void epmem_go( agent *my_agent, bool allow_store )
{

	my_agent->epmem_timers->total->start();

#ifndef EPMEM_EXPERIMENT

	if ( allow_store )
	{
		epmem_consider_new_episode( my_agent );
	}
	epmem_respond_to_cmd( my_agent );
	//E587: backup
	//	my_agent->epmem_worker_p->epmem_db->backup( "debug.db", new std::string("Backup error"));

#else // EPMEM_EXPERIMENT

	_epmem_exp( my_agent );
	epmem_respond_to_cmd( my_agent );

#endif // EPMEM_EXPERIMENT

	my_agent->epmem_timers->total->stop();

}

bool epmem_backup_db( agent* my_agent, const char* file_name, std::string *err )
{
	bool return_val = false;

	// E587: AM:
	if ( my_agent->epmem_db->get_status() == soar_module::connected )
	{
		if ( my_agent->epmem_params->lazy_commit->get_value() == soar_module::on )
		{
			my_agent->epmem_stmts_master->commit->execute( soar_module::op_reinit );
		}

		return_val = my_agent->epmem_db->backup( file_name, err );

		if ( my_agent->epmem_params->lazy_commit->get_value() == soar_module::on )
		{
			my_agent->epmem_stmts_master->begin->execute( soar_module::op_reinit );
		}
	}
	else
	{
		err->assign( "Episodic database is not currently connected." );
	}

	return return_val;
}

// E587 JK create message, send to manager master, wait for response and handle
/*
void epmem_handle_query(
    agent *my_agent, Symbol *state, Symbol *pos_query, Symbol *neg_query, 
    epmem_time_list& prohibits, epmem_time_id before, epmem_time_id after, 
    epmem_symbol_set& currents, soar_module::wme_set& cue_wmes) 
    //,soar_module::symbol_triple_list& meta_wmes, soar_module::symbol_triple_list& retrieval_wmes) 
{
    MPI::Status status;
    int size = sizeof(int)*2+sizeof(EPMEM_MSG_TYPE)+sizeof(query_data);
    epmem_msg * msg = (epmem_msg*) malloc(size);
    query_data * data = (query_data*)msg->data;
    // needed data
    bool graph_match = (my_agent->epmem_params->graph_match->get_value() == soar_module::on);
    epmem_param_container::gm_ordering_choices gm_order = my_agent->epmem_params->gm_ordering->get_value();
    epmem_time_id before_time = my_agent->epmem_stats->time->get_value() - 1;
    double balance = my_agent->epmem_params->balance->get_value();
    
    data->graph_match = graph_match;
    data->gm_order = gm_order;
    data->before_time = before_time;
    data->balance = balance;
    memcpy(&data->pos_query, pos_query, sizeof(Symbol));
    memcpy(&data->neg_query, neg_query, sizeof(Symbol));
    data->before = before;
    data->after = after;
    msg->size = size;
    msg->type = SEARCH;
    msg->source = 0;
    //TODO serialize currents and cue_wmes
    
    //send msg to manager
    MPI::COMM_WORLD.Send(msg, size, MPI::CHAR, MANAGER_ID, 1);
    delete msg;
    // get response
    //blocking probe call (unknown message size)
    MPI::COMM_WORLD.Probe(MANAGER_ID, 1, status);
    
    //get source and size of incoming message
    int buffSize = status.Get_count(MPI::CHAR);
    
    epmem_msg * recMsg = (epmem_msg*) malloc(buffSize);
    MPI::COMM_WORLD.Recv(recMsg, buffSize, MPI::CHAR, MANAGER_ID, 1, status);
    // handle 
    query_rsp_data *rsp = (query_rsp_data*)recMsg->data;
    epmem_handle_search_result(state, my_agent, rsp);
    
}

//zzzz
void epmem_handle_search_result(Symbol *state, agent* my_agent, query_rsp_data* rsp)
{
// E587 JK at this point have found best episode and should message manager
    
    epmem_time_id best_episode = rsp->best_episode;    
    Symbol *pos_query =&rsp->pos_query;
    Symbol *neg_query =&rsp->neg_query;
    int leaf_literals_size = rsp->leaf_literals_size;
    double best_score =rsp->best_score;
    bool best_graph_matched =rsp->best_graph_matched;
    long int best_cardinality =rsp->best_cardinality;
    double perfect_score =rsp->perfect_score;
    bool do_graph_match =rsp->do_graph_match;
    int level = 3;
    
    //TODO serialize!
    epmem_literal_node_pair_map best_bindings;
    soar_module::symbol_triple_list meta_wmes;
    soar_module::symbol_triple_list retrieval_wmes;
    
    
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
	temp_sym = make_int_constant(my_agent, leaf_literals_size);//E587 JK
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
}
*/
