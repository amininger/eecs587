/*************************************************************************
 * PLEASE SEE THE FILE "license.txt" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION. 
 *************************************************************************/

/*************************************************************************
 *
 *  file:  reinforcement_learning.h
 *
 * =======================================================================
 */

#ifndef REINFORCEMENT_LEARNING_H
#define REINFORCEMENT_LEARNING_H

#include <map>
#include <string>
#include <list>
#include <vector>

#include "soar_module.h"

#include "chunk.h"
#include "production.h"

//////////////////////////////////////////////////////////
// RL Constants
//////////////////////////////////////////////////////////

// more specific forms of no change impasse types
// made negative to never conflict with impasse constants
#define STATE_NO_CHANGE_IMPASSE_TYPE -1
#define OP_NO_CHANGE_IMPASSE_TYPE -2

//////////////////////////////////////////////////////////
// RL Parameters
//////////////////////////////////////////////////////////

class rl_learning_param;
class rl_apoptosis_param;
class rl_apoptosis_thresh_param;

template <typename T>
class param_accessor {
    public:
        virtual void set_param(production * const prod, T value) const = 0;
        virtual T get_param(const production * const prod) const = 0;
        void set_param(production * const prod, std::string value_str) const {
            T value;
            std::istringstream iss(value_str);
            iss >> value;
            set_param(prod, value);
        }
};

class rl_updates_accessor : public param_accessor<double> {
    virtual void set_param(production * const prod, double value) const {
        prod->rl_update_count = value;
    }
    virtual double get_param(const production * const prod) const {
        return prod->rl_update_count;
    }
};

class rl_dbd_h_accessor : public param_accessor<double> {
    virtual void set_param(production * const prod, double value) const {
        prod->rl_delta_bar_delta_h = value;
    }
    virtual double get_param(const production * const prod) const {
        return prod->rl_delta_bar_delta_h;
    }
};

class rl_param_container: public soar_module::param_container
{
	public:
		enum learning_choices { sarsa, q };
        
        // How the learning rate cools over time.
        // normal_decay: default, same learning rate for each rule
        // exponential_decay: rate = rate / # updates for this rule
        // logarithmic_decay: rate = rate / log(# updates for this rule)
        // Miller, 11/14/2011
        enum decay_choices { normal_decay, exponential_decay, logarithmic_decay, delta_bar_delta_decay };

		enum apoptosis_choices { apoptosis_none, apoptosis_chunks, apoptosis_rl };
		
		rl_learning_param *learning;
		soar_module::decimal_param *discount_rate;
		soar_module::decimal_param *learning_rate;
        soar_module::decimal_param *meta_learning_rate; // For delta bar delta
		soar_module::constant_param<learning_choices> *learning_policy;
		soar_module::constant_param<decay_choices> *decay_mode;
		soar_module::decimal_param *et_decay_rate;
		soar_module::decimal_param *et_tolerance;
		soar_module::boolean_param *temporal_extension;
		soar_module::boolean_param *hrl_discount;
		soar_module::boolean_param *temporal_discount;

		soar_module::boolean_param *chunk_stop;
		soar_module::boolean_param *meta; // Whether doc strings are used for storing metadata.
        soar_module::string_param *update_log_path; // If non-null and size > 0, log all RL updates to this file.

		rl_apoptosis_param *apoptosis;
		soar_module::decimal_param *apoptosis_decay;
		rl_apoptosis_thresh_param *apoptosis_thresh;

		rl_param_container( agent *new_agent );

        // For writing parameters to a rule's documentation string.
        static const std::vector<std::pair<std::string, param_accessor<double> * > > &get_documentation_params();
};

class rl_learning_param: public soar_module::boolean_param
{
	protected:
		agent *my_agent;

	public:
		rl_learning_param( const char *new_name, soar_module::boolean new_value, soar_module::predicate<soar_module::boolean> *new_prot_pred, agent *new_agent );
		void set_value( soar_module::boolean new_value );
};

class rl_apoptosis_param: public soar_module::constant_param< rl_param_container::apoptosis_choices >
{
	protected:
		agent* my_agent;

	public:
		rl_apoptosis_param( const char *new_name, rl_param_container::apoptosis_choices new_value, soar_module::predicate<rl_param_container::apoptosis_choices> *new_prot_pred, agent *new_agent );
		void set_value( rl_param_container::apoptosis_choices new_value );
};

class rl_apoptosis_thresh_param: public soar_module::decimal_param
{
	public:
		rl_apoptosis_thresh_param( const char *new_name, double new_value, soar_module::predicate<double> *new_val_pred, soar_module::predicate<double> *new_prot_pred );
		void set_value( double new_value );
};

template <typename T>
class rl_apoptosis_predicate: public soar_module::agent_predicate<T>
{
	public:
		rl_apoptosis_predicate( agent *new_agent );
		bool operator() ( T val );
};

//////////////////////////////////////////////////////////
// RL Statistics
//////////////////////////////////////////////////////////

class rl_stat_container: public soar_module::stat_container
{
	public:	
		soar_module::decimal_stat *update_error;
		soar_module::decimal_stat *total_reward;
		soar_module::decimal_stat *global_reward;
				
		rl_stat_container( agent *new_agent );
};


//////////////////////////////////////////////////////////
// RL Types
//////////////////////////////////////////////////////////

// map of eligibility traces
#ifdef USE_MEM_POOL_ALLOCATORS
typedef std::map< production*, double, std::less< production* >, soar_module::soar_memory_pool_allocator< std::pair< production*, double > > > rl_et_map;
#else
typedef std::map< production*, double > rl_et_map;
#endif

// list of rules associated with the last operator
#ifdef USE_MEM_POOL_ALLOCATORS
typedef std::list< production*, soar_module::soar_memory_pool_allocator< production* > > rl_rule_list;
#else
typedef std::list< production* > rl_rule_list;
#endif

// rl data associated with each state
typedef struct rl_data_struct {
	rl_et_map *eligibility_traces;			// traces associated with productions
	rl_rule_list *prev_op_rl_rules;			// rl rules associated with the previous operator
	
	double previous_q;						// q-value of the previous state
	double reward;							// accumulated discounted reward

	unsigned int gap_age;					// the number of steps since a cycle containing rl rules
	unsigned int hrl_age;					// the number of steps in a subgoal
} rl_data;

typedef std::map< Symbol*, Symbol* > rl_symbol_map;
typedef std::set< rl_symbol_map > rl_symbol_map_set;

// used to manage apoptosis
typedef soar_module::bla_object_memory< production, 10, 50 > rl_production_memory;

//////////////////////////////////////////////////////////
// Maintenance
//////////////////////////////////////////////////////////

// remove Soar-RL references to productions
extern void rl_remove_refs_for_prod( agent *my_agent, production *prod );
extern void rl_clear_refs( Symbol* goal );

//////////////////////////////////////////////////////////
// Parameter Get/Set/Validate
//////////////////////////////////////////////////////////

// shortcut for determining if Soar-RL is enabled
extern bool rl_enabled( agent *my_agent );

//////////////////////////////////////////////////////////
// Production Validation
//////////////////////////////////////////////////////////

// validate template
extern bool rl_valid_template( production *prod );

// validate rl rule
extern bool rl_valid_rule( production *prod );

// sets rl meta-data from a production documentation string
extern void rl_rule_meta( agent* my_agent, production* prod );

// template instantiation
extern int rl_get_template_id( const char *prod_name );

//////////////////////////////////////////////////////////
// Template Tracking
//////////////////////////////////////////////////////////

// initializes agent's tracking of template-originated rl-rules
extern void rl_initialize_template_tracking( agent *my_agent );

// updates the agent's tracking of template-originated rl-rules
extern void rl_update_template_tracking( agent *my_agent, const char *rule_name );

// get the next id for a template (increments internal counter)
extern int rl_next_template_id( agent *my_agent );

// reverts internal counter
extern void rl_revert_template_id( agent *my_agent );

//////////////////////////////////////////////////////////
// Template Behavior
//////////////////////////////////////////////////////////

// builds a new Soar-RL rule from a template instantiation
extern Symbol *rl_build_template_instantiation( agent *my_agent, instantiation *my_template_instance, struct token_struct *tok, wme *w );

// creates an incredibly simple action
extern action *rl_make_simple_action( agent *my_gent, Symbol *id_sym, Symbol *attr_sym, Symbol *val_sym, Symbol *ref_sym );

// adds a test to a condition list for goals or impasses contained within the condition list
extern void rl_add_goal_or_impasse_tests_to_conds(agent *my_agent, condition *all_conds);

//////////////////////////////////////////////////////////
// Reward
//////////////////////////////////////////////////////////

// tabulation of a single goal's reward
extern void rl_tabulate_reward_value_for_goal( agent *my_agent, Symbol *goal );

// tabulation of all agent goal reward
extern void rl_tabulate_reward_values( agent *my_agent );

//////////////////////////////////////////////////////////
// Updates
//////////////////////////////////////////////////////////

// Store and update data that will be needed later to perform a Bellman update for the current operator
extern void rl_store_data( agent *my_agent, Symbol *goal, preference *cand );

// update the value of Soar-RL rules
extern void rl_perform_update( agent *my_agent, double op_value, bool op_rl, Symbol *goal, bool update_efr = true );

// clears eligibility traces in accordance with watkins
extern void rl_watkins_clear( agent *my_agent, Symbol *goal );

#endif
