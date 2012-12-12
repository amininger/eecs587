

#ifndef EPMEM_QUERY_H_
#define EPMEM_QUERY_H_

#include "../episodic_memory.h"
#include "soar_module.h"


#ifdef USE_MPI
#include "epmem_manager.h"
#else
typedef enum
{
    NEW_EP = 0,
	INIT_WORKER,
    RESIZE_WINDOW,
    RESIZE_REQUEST,
    SEARCH,
    SEARCH_RESULT,
    TERMINATE_SEARCH
} EPMEM_MSG_TYPE;

//message structure
typedef struct epmem_msg_struct
{
    EPMEM_MSG_TYPE type;
    int size;
    int source;
    char data;
} epmem_msg;

#endif

//////////////////////////////////////////////////////////
// cue structures - used as substitutions on the worker processors
//    for actual symbols and wme's in working memory
//////////////////////////////////////////////////////////

typedef struct epmem_cue_wme_struct epmem_cue_wme;
typedef struct epmem_cue_symbol_struct epmem_cue_symbol;
typedef struct epmem_packed_cue_wme_struct epmem_packed_cue_wme;
typedef struct epmem_cue_symbol_table_struct epmem_cue_symbol_table;

typedef struct epmem_query_struct epmem_query;
typedef struct query_rsp_data_struct query_rsp_data;

// STL containers for wmes and symbols
typedef std::vector<epmem_cue_wme*> epmem_cue_wme_list;
typedef std::vector<epmem_cue_symbol*> epmem_cue_symbol_list;

typedef std::set<epmem_cue_wme*> epmem_cue_wme_set;
typedef std::set<epmem_cue_symbol*> epmem_cue_symbol_set;

typedef std::vector<epmem_packed_cue_wme> epmem_packed_cue_wme_list;
typedef std::vector<epmem_cue_symbol> epmem_cue_symbols_packed_list;

typedef std::map<Symbol*, int> epmem_cue_symbol_table_index;


//////////////////////////////////////////////////////////
// Episodic Memory Search
//////////////////////////////////////////////////////////

// defined below
typedef struct epmem_triple_struct epmem_triple;
typedef struct epmem_literal_struct epmem_literal;
typedef struct epmem_pedge_struct epmem_pedge;
typedef struct epmem_uedge_struct epmem_uedge;
typedef struct epmem_interval_struct epmem_interval;

// pairs
typedef struct std::pair<epmem_cue_symbol*, epmem_literal*> epmem_symbol_literal_pair;
typedef struct std::pair<epmem_cue_symbol*, epmem_node_id> epmem_symbol_node_pair;
typedef struct std::pair<epmem_literal*, epmem_node_id> epmem_literal_node_pair;
typedef struct std::pair<epmem_node_id, epmem_node_id> epmem_node_pair;

// collection classes
typedef std::deque<epmem_literal*> epmem_literal_deque;
typedef std::deque<epmem_node_id> epmem_node_deque;
typedef std::map<epmem_cue_symbol*, int> epmem_symbol_int_map;
typedef std::map<epmem_literal*, epmem_node_pair> epmem_literal_node_pair_map;
typedef std::map<epmem_literal_node_pair, int> epmem_literal_node_pair_int_map;
typedef std::map<epmem_node_id, epmem_cue_symbol*> epmem_node_symbol_map;
typedef std::map<epmem_node_id, int> epmem_node_int_map;
typedef std::map<epmem_symbol_literal_pair, int> epmem_symbol_literal_pair_int_map;
typedef std::map<epmem_symbol_node_pair, int> epmem_symbol_node_pair_int_map;
typedef std::map<epmem_triple, epmem_pedge*> epmem_triple_pedge_map;
typedef std::map<epmem_cue_wme*, epmem_literal*> epmem_wme_literal_map;
typedef std::set<epmem_literal*> epmem_literal_set;
typedef std::set<epmem_pedge*> epmem_pedge_set;

typedef std::map<epmem_triple, epmem_uedge*> epmem_triple_uedge_map;
typedef std::set<epmem_interval*> epmem_interval_set;
typedef std::set<epmem_node_id> epmem_node_set;
typedef std::set<epmem_node_pair> epmem_node_pair_set;


// structs
struct epmem_triple_struct {
	epmem_node_id q0;
	epmem_node_id w;
	epmem_node_id q1;
	bool operator<(const epmem_triple& other) const {
		if (q0 != other.q0) {
			return (q0 < other.q0);
		} else if (w != other.w) {
			return (w < other.w);
		} else {
			return (q1 < other.q1);
		}
	}
};

// E587: AM: Changed to use epmem_cue_symbol
struct epmem_literal_struct {
	epmem_cue_symbol* id_sym;
	epmem_cue_symbol* value_sym;
	int is_neg_q;
	int value_is_id;
	bool is_leaf;
	bool is_current;
	epmem_node_id w;
	epmem_node_id q1;
	double weight;
	epmem_literal_set parents;
	epmem_literal_set children;
	epmem_node_pair_set matches;
	epmem_node_int_map values;
    bool is_lti;
    epmem_time_id promotion_time;
};

struct epmem_pedge_struct {
	epmem_triple triple;
	int value_is_id;
	bool has_noncurrent;
	epmem_literal_set literals;
	soar_module::pooled_sqlite_statement* sql;
	epmem_time_id time;
    bool is_lti;
    epmem_time_id promotion_time;
};

struct epmem_uedge_struct {
	epmem_triple triple;
	int value_is_id;
	bool has_noncurrent;
	int activation_count;
	epmem_pedge_set pedges;
	int intervals;
	bool activated;
};

struct epmem_interval_struct {
	epmem_uedge* uedge;
	int is_end_point;
	soar_module::pooled_sqlite_statement* sql;
	epmem_time_id time;
};

// priority queues and comparison functions
struct epmem_pedge_comparator {
	bool operator()(const epmem_pedge *a, const epmem_pedge *b) const {
		if (a->time != b->time) {
			return (a->time < b->time);
		} else {
			return (a < b);
		}
	}
};
typedef std::priority_queue<epmem_pedge*, std::vector<epmem_pedge*>, epmem_pedge_comparator> epmem_pedge_pq;
struct epmem_interval_comparator {
	bool operator()(const epmem_interval *a, const epmem_interval *b) const {
		if (a->time != b->time) {
			return (a->time < b->time);
		} else if (a->is_end_point == b->is_end_point) {
			return (a < b);
		} else {
			// arbitrarily put starts before ends
			return (a->is_end_point == EPMEM_RANGE_START);
		}
	}
};
typedef std::priority_queue<epmem_interval*, std::vector<epmem_interval*>, epmem_interval_comparator> epmem_interval_pq;


//////////////////////////////////////////////////////////
// Structures defined for EECS 587
//////////////////////////////////////////////////////////
// Defines structures sent back and forth as messages

struct epmem_query_struct{
    epmem_time_id before;
    epmem_time_id after;
    bool do_graph_match;
	epmem_param_container::gm_ordering_choices gm_order;
    int pos_query_index;
    int neg_query_index;
    int level;
    epmem_time_list prohibits;
    std::vector<int> currents;
    epmem_packed_cue_wme_list wmes;
    epmem_cue_symbols_packed_list symbols;

    epmem_msg* pack();
    void unpack(epmem_msg* msg);

};//__attribute__((packed));

struct query_rsp_data_struct
{
    epmem_time_id best_episode;
    double best_score;
    double perfect_score;
    bool best_graph_matched;
    long best_cardinality;
    int leaf_literals_size;

    epmem_msg* pack();
    void unpack(epmem_msg* msg);
};//__attribute__((packed));

// Represents a symbol in working memory, can be an identifier, lti, or value
struct epmem_cue_symbol_struct{
    epmem_cue_symbol_struct(Symbol* sym, agent* my_agent);
    
    bool is_id;
    epmem_node_id id;
    bool is_lti;
    epmem_time_id promotion_time;
    epmem_cue_wme_list* children;
};//__attribute__((packed));

// A temporary symbol table used to index into a list of symbols
struct epmem_cue_symbol_table_struct{
    epmem_cue_symbol_table_struct(epmem_cue_symbols_packed_list* symbol_list)
        :symbols(symbol_list){}

    // returns the index of the epmem_cue_symbol version of the given Symbol
    //   if it's not in the table yet, it inserts it
    int get_cue_symbol(Symbol* sym, agent* my_agent);

    epmem_cue_symbols_packed_list* symbols;
    epmem_cue_symbol_table_index sym_index;
};

// Represents a packed version of a wme, with indices into the symbol list instead of pointers
struct epmem_packed_cue_wme_struct{
    epmem_packed_cue_wme_struct(wme* w, agent* my_agent, epmem_cue_symbol_table& sym_table);
    int id_index;
    int attr_index;
    int value_index;
    double activation;
};//__attribute__((packed));

// Simple wme structure with an (id ^attr value)
struct epmem_cue_wme_struct{
    epmem_cue_wme_struct(epmem_packed_cue_wme* packed_wme, epmem_query* query);
    epmem_cue_symbol* id;
    epmem_cue_symbol* attr;
    epmem_cue_symbol* value;
    double activation;
};

#endif // EPMEM_QUERY_H_
