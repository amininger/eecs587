/*************************************************************************
 * PLEASE SEE THE FILE "COPYING" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION.
 *************************************************************************/

/*************************************************************************
 *
 *  file:  episodic_memory.h
 *
 * =======================================================================
 */

#ifndef EPISODIC_MEMORY_H
#define EPISODIC_MEMORY_H

#include <portability.h>

// E587: AM:
#include <stdint.h>
//#define USE_MPI 1

#include <map>
#include <list>
#include <stack>
#include <set>
#include <queue>

#include "soar_module.h"
#include "soar_db.h"


//////////////////////////////////////////////////////////
// EpMem Experimentation
//
// If defined, we hijack the main EpMem function
// for tight-loop experimentation/timing.
//
//////////////////////////////////////////////////////////

//#define EPMEM_EXPERIMENT

//////////////////////////////////////////////////////////
// EpMem Constants
//////////////////////////////////////////////////////////

enum epmem_variable_key
{
	// E583: AM: only on worker
	var_rit_offset_1, var_rit_leftroot_1, var_rit_rightroot_1, var_rit_minstep_1,
	var_rit_offset_2, var_rit_leftroot_2, var_rit_rightroot_2, var_rit_minstep_2,
	var_next_id
};

// algorithm constants
#define EPMEM_MEMID_NONE							0
#define EPMEM_NODEID_ROOT							0
#define EPMEM_NODEID_BAD							-1
#define EPMEM_HASH_ACCEPTABLE						1

#define EPMEM_NODE_POS								0
#define EPMEM_NODE_NEG								1
#define EPMEM_RANGE_START							0
#define EPMEM_RANGE_END								1
#define EPMEM_RANGE_EP								0
#define EPMEM_RANGE_NOW								1
#define EPMEM_RANGE_POINT							2

#define EPMEM_LN_2									0.693147180559945

#define EPMEM_DNF									2


//////////////////////////////////////////////////////////
// EpMem Typedefs
//////////////////////////////////////////////////////////

// represents a unique node identifier in the episodic store
typedef int64_t epmem_node_id;

// represents a unique temporal hash in the episodic store
typedef uint64_t epmem_hash_id;

// represents a unique episode identifier in the episodic store
typedef uint64_t epmem_time_id;

//////////////////////////////////////////////////////////
// EpMem Parameters
//////////////////////////////////////////////////////////

class epmem_path_param;

class epmem_param_container: public soar_module::param_container
{
	public:
		
		// storage
		enum db_choices { memory, file };
		
		// encoding
		enum phase_choices { phase_output, phase_selection };
		enum trigger_choices { none, output, dc };
		enum force_choices { remember, ignore, force_off };

		// performance
		enum page_choices { page_1k, page_2k, page_4k, page_8k, page_16k, page_32k, page_64k };
		enum opt_choices { opt_safety, opt_speed };

		// experimental
		enum gm_ordering_choices { gm_order_undefined, gm_order_dfs, gm_order_mcv };
		enum merge_choices { merge_none, merge_add };

		////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////

		soar_module::boolean_param *learning;	
		
		// encoding
		soar_module::constant_param<phase_choices> *phase;
		soar_module::constant_param<trigger_choices> *trigger;
		soar_module::constant_param<force_choices> *force;
		soar_module::sym_set_param *exclusions;

		// storage
		soar_module::constant_param<db_choices> *database;
		epmem_path_param *path;
		soar_module::boolean_param *lazy_commit;

		// retrieval
		soar_module::boolean_param *graph_match;
		soar_module::decimal_param *balance;

		// performance
		soar_module::constant_param<page_choices> *page_size;
		soar_module::integer_param *cache_size;
		soar_module::constant_param<opt_choices> *opt;
		soar_module::constant_param<soar_module::timer::timer_level> *timers;

		// experimental
		soar_module::constant_param<gm_ordering_choices>* gm_ordering;
		soar_module::constant_param<merge_choices>* merge;

		epmem_param_container( agent *new_agent );
};

class epmem_path_param: public soar_module::string_param
{
	protected:
		agent *my_agent;

	public:
		epmem_path_param( const char *new_name, const char *new_value, soar_module::predicate<const char *> *new_val_pred, soar_module::predicate<const char *> *new_prot_pred, agent *new_agent );
		virtual void set_value( const char *new_value );
};

template <typename T>
class epmem_db_predicate: public soar_module::agent_predicate<T>
{
	public:
		epmem_db_predicate( agent *new_agent );
		bool operator() ( T val );
};


//////////////////////////////////////////////////////////
// EpMem Statistics
//////////////////////////////////////////////////////////

typedef soar_module::primitive_stat<epmem_time_id> epmem_time_id_stat;
typedef soar_module::primitive_stat<epmem_node_id> epmem_node_id_stat;

class epmem_db_lib_version_stat;
class epmem_mem_usage_stat;
class epmem_mem_high_stat;

class epmem_stat_container: public soar_module::stat_container
{
	public:
		epmem_time_id_stat *time;
		epmem_db_lib_version_stat* db_lib_version;
		epmem_mem_usage_stat *mem_usage;
		epmem_mem_high_stat *mem_high;
		soar_module::integer_stat *cbr;
		soar_module::integer_stat *nexts;
		soar_module::integer_stat *prevs;
		soar_module::integer_stat *ncb_wmes;

		soar_module::integer_stat *qry_pos;
		soar_module::integer_stat *qry_neg;
		epmem_time_id_stat *qry_ret;
		soar_module::integer_stat *qry_card;
		soar_module::integer_stat *qry_lits;

		epmem_node_id_stat *next_id;

		soar_module::integer_stat *rit_offset_1;
		soar_module::integer_stat *rit_left_root_1;
		soar_module::integer_stat *rit_right_root_1;
		soar_module::integer_stat *rit_min_step_1;

		soar_module::integer_stat *rit_offset_2;
		soar_module::integer_stat *rit_left_root_2;
		soar_module::integer_stat *rit_right_root_2;
		soar_module::integer_stat *rit_min_step_2;

		epmem_stat_container( agent *my_agent );
};

//

class epmem_db_lib_version_stat: public soar_module::primitive_stat< const char* >
{
	protected:
		agent* my_agent;

	public:
		epmem_db_lib_version_stat( agent* new_agent, const char* new_name, const char* new_value, soar_module::predicate< const char* >* new_prot_pred );
		const char* get_value();
};

//

class epmem_mem_usage_stat: public soar_module::integer_stat
{
	protected:
		agent *my_agent;

	public:
		epmem_mem_usage_stat( agent *new_agent, const char *new_name, int64_t new_value, soar_module::predicate<int64_t> *new_prot_pred );
		int64_t get_value();
};

//

class epmem_mem_high_stat: public soar_module::integer_stat
{
	protected:
		agent *my_agent;

	public:
		epmem_mem_high_stat( agent *new_agent, const char *new_name, int64_t new_value, soar_module::predicate<int64_t> *new_prot_pred );
		int64_t get_value();
};


//////////////////////////////////////////////////////////
// EpMem Timers
//////////////////////////////////////////////////////////

class epmem_timer_container: public soar_module::timer_container
{
	public:
		soar_module::timer *total;
		soar_module::timer *storage;
		soar_module::timer *ncb_retrieval;
		soar_module::timer *query;
		soar_module::timer *api;
		soar_module::timer *trigger;
		soar_module::timer *init;
		soar_module::timer *next;
		soar_module::timer *prev;
		soar_module::timer *hash;
		soar_module::timer *wm_phase;

		soar_module::timer *ncb_edge;
		soar_module::timer *ncb_edge_rit;
		soar_module::timer *ncb_node;
		soar_module::timer *ncb_node_rit;

		soar_module::timer *query_dnf;
		soar_module::timer *query_walk;
		soar_module::timer *query_walk_edge;
		soar_module::timer *query_walk_interval;
		soar_module::timer *query_graph_match;
		soar_module::timer *query_result;
		soar_module::timer *query_cleanup;

		soar_module::timer *query_sql_edge;
		soar_module::timer *query_sql_start_ep;
		soar_module::timer *query_sql_start_now;
		soar_module::timer *query_sql_start_point;
		soar_module::timer *query_sql_end_ep;
		soar_module::timer *query_sql_end_now;
		soar_module::timer *query_sql_end_point;

		epmem_timer_container( agent *my_agent );
};

class epmem_timer_level_predicate: public soar_module::agent_predicate<soar_module::timer::timer_level>
{
	public:
		epmem_timer_level_predicate( agent *new_agent );
		bool operator() ( soar_module::timer::timer_level val );
};

class epmem_timer: public soar_module::timer
{
	public:
		epmem_timer( const char *new_name, agent *new_agent, timer_level new_level );
};





//////////////////////////////////////////////////////////
// Common Types
//////////////////////////////////////////////////////////

// represents a vector of times
typedef std::vector<epmem_time_id> epmem_time_list;

// represents a list of wmes
typedef std::list<wme *> epmem_wme_list;



//////////////////////////////////////////////////////////
// Soar Integration Types
//////////////////////////////////////////////////////////

// list used primarily like a stack
#ifdef USE_MEM_POOL_ALLOCATORS
typedef std::list< preference*, soar_module::soar_memory_pool_allocator< preference* > > epmem_wme_stack;
#else
typedef std::list< preference* > epmem_wme_stack;
#endif

// data associated with each state
typedef struct epmem_data_struct
{
	uint64_t last_ol_time;									// last update to output-link
	uint64_t last_ol_count;									// last count of output-link

	uint64_t last_cmd_time;									// last update to epmem.command
	uint64_t last_cmd_count;								// last update to epmem.command

	epmem_time_id last_memory;								// last retrieved memory

	epmem_wme_stack* epmem_wmes;							// preferences generated in last epmem
} epmem_data;

// lookup tables to facilitate shared identifiers
typedef std::map<epmem_node_id, Symbol *> epmem_id_mapping;

// types/structures to facilitate re-use of identifiers
typedef std::pair<epmem_node_id, epmem_node_id> epmem_id_pair;
typedef std::list<epmem_id_pair> epmem_id_pool;
typedef std::map<epmem_node_id, epmem_id_pool *> epmem_hashed_id_pool;
typedef std::map<epmem_node_id, epmem_hashed_id_pool *> epmem_parent_id_pool;
typedef std::map<epmem_node_id, epmem_id_pool *> epmem_return_id_pool;

#ifdef USE_MEM_POOL_ALLOCATORS
typedef std::set< wme*, std::less< wme* >, soar_module::soar_memory_pool_allocator< wme* > > epmem_wme_set;
typedef std::list< Symbol*, soar_module::soar_memory_pool_allocator< Symbol* > > epmem_symbol_stack;

// types/structures to facilitate incremental storage
typedef std::map< epmem_node_id, bool, std::less< epmem_node_id >, soar_module::soar_memory_pool_allocator< std::pair< epmem_node_id, bool > > > epmem_id_removal_map;
typedef std::set< Symbol*, std::less< Symbol* >, soar_module::soar_memory_pool_allocator< Symbol* > > epmem_symbol_set;

#else
typedef std::set< wme* > epmem_wme_set;
typedef std::list< Symbol* > epmem_symbol_stack;

// types/structures to facilitate incremental storage
typedef std::map<epmem_node_id, bool> epmem_id_removal_map;
typedef std::set< Symbol* > epmem_symbol_set;
#endif
typedef std::map<epmem_node_id, epmem_wme_set*> epmem_id_ref_counter;

typedef struct epmem_id_reservation_struct
{
	epmem_node_id my_id;
	epmem_hash_id my_hash;
	epmem_id_pool *my_pool;
} epmem_id_reservation;

// represents a graph edge (i.e. identifier)
// follows cs theory notation of finite automata: q1 = d( q0, w )
typedef struct epmem_edge_struct
{
	
	epmem_node_id q0;							// id
	Symbol *w;						            // attr
	epmem_node_id q1;							// value

	bool val_is_short_term;
	char val_letter;
	uint64_t val_num;

} epmem_edge;

//////////////////////////////////////////////////////////
// Parameter Functions (see cpp for comments)
//////////////////////////////////////////////////////////

// shortcut for determining if EpMem is enabled
extern bool epmem_enabled( agent *my_agent );

//////////////////////////////////////////////////////////
// Soar Functions (see cpp for comments)
//////////////////////////////////////////////////////////

// init, end
extern void epmem_reset( agent *my_agent, Symbol *state = NULL );
extern void epmem_close( agent *my_agent );

// perform epmem actions
extern void epmem_go( agent *my_agent, bool allow_store = true );
extern bool epmem_backup_db( agent* my_agent, const char* file_name, std::string *err );
extern void epmem_schedule_promotion( agent* my_agent, Symbol* id );

// visualization
extern void epmem_visualize_episode( agent* my_agent, epmem_time_id memory_id, std::string* buf );
extern void epmem_print_episode( agent* my_agent, epmem_time_id memory_id, std::string* buf );

extern epmem_hash_id epmem_temporal_hash( agent *my_agent, Symbol *sym, bool add_on_fail );

struct epmem_node_unique{
	static const int NUM_LONGS = 4;
	int64_t id;
	int64_t parent_id;
	int64_t attribute;
	int64_t value;
	epmem_node_unique(){}
	epmem_node_unique(soar_module::sqlite_statement* stmt)
		:	id(stmt->column_int(0)), parent_id(stmt->column_int(1)), 
			attribute(stmt->column_int(2)), value(stmt->column_int(3)){}
};

struct epmem_edge_unique{
	static const int NUM_LONGS = 4;
	int64_t id;
	int64_t parent_id;
	int64_t attribute;
	int64_t child_id;
	epmem_edge_unique(){}
	epmem_edge_unique(soar_module::sqlite_statement* stmt)
		:	id(stmt->column_int(0)), parent_id(stmt->column_int(1)), 
			attribute(stmt->column_int(2)), child_id(stmt->column_int(3)){}
};



#endif
