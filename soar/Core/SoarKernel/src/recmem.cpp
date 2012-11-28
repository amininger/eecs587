#include <portability.h>

/*************************************************************************
 * PLEASE SEE THE FILE "license.txt" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION. 
 *************************************************************************/

/*************************************************************************
 *
 *  file:  recmem.cpp
 *
 * =======================================================================
 *  
 *             Recognition Memory (Firer and Chunker) Routines
 *                 (Does not include the Rete net)
 *
 * Init_firer() and init_chunker() should be called at startup time, to
 * do initialization.
 *
 * Do_preference_phase() runs the entire preference phase.  This is called
 * from the top-level control in main.c.
 *
 * Possibly_deallocate_instantiation() checks whether an instantiation
 * can be deallocated yet, and does so if possible.  This is used whenever
 * the (implicit) reference count on the instantiation decreases.
 * =======================================================================
 */

#include <stdlib.h>

#include "gdatastructs.h"
#include "instantiations.h"
#include "kernel.h"
#include "mem.h"
#include "symtab.h"
#include "agent.h"
#include "prefmem.h"
#include "rhsfun.h"
#include "rete.h"
#include "print.h"
#include "production.h"
#include "wmem.h"
#include "osupport.h"
#include "recmem.h"
#include "tempmem.h"
#include "reinforcement_learning.h"
#include "wma.h"
#include "xml.h"
#include "utilities.h"
#include "soar_TraceNames.h"
#include "consistency.h"
#include "misc.h"
#include "soar_module.h"

#include "assert.h"
#include <string> // SBW 8/4/08
#include <list>

using namespace soar_TraceNames;

#ifdef USE_MEM_POOL_ALLOCATORS
typedef std::list<instantiation*,
		soar_module::soar_memory_pool_allocator<instantiation*> > inst_mpool_list;
typedef std::list<condition*,
		soar_module::soar_memory_pool_allocator<condition*> > cond_mpool_list;
#else
typedef std::list< instantiation* > inst_mpool_list;
typedef std::list< condition* > cond_mpool_list;
#endif

/* Uncomment the following line to get instantiation printouts */
/* #define DEBUG_INSTANTIATIONS */

/* TEMPORARY HACK (Ideally this should be doable through
 the external kernel interface but for now using a
 couple of global STL lists to get this information
 from the rhs function to this preference adding code)*/
wme* glbDeepCopyWMEs = NULL;


/* --------------------------------------------------------------------------
                 Build context-dependent preference set

  This function will copy the CDPS from a slot to the backtrace info for the
  corresponding condition.  The copied CDPS will later be backtraced through.

  Note: Until prohibits are included explicitly as part of the CDPS, we will
  just copy them directly from the prohibits list so that there is no
  additional overhead.  Once the CDPS is on by default, we can eliminate the
  second half of the big else (and not call this function at all if the
  sysparam is not set.

 --------------------------------------------------------------------------*/

void build_CDPS(agent* thisAgent, instantiation *inst) {
  condition *cond;
  preference *pref, *new_pref;
  cons *CDPS;

  for (cond = inst->top_of_instantiated_conditions; cond != NIL;
      cond = cond->next) {
    cond->bt.CDPS = NIL;
    if (cond->type == POSITIVE_CONDITION && cond->bt.trace && cond->bt.trace->slot) {
        if (thisAgent->sysparams[CHUNK_THROUGH_EVALUATION_RULES_SYSPARAM]) {
        if (cond->bt.trace->slot->CDPS) {
          for (CDPS=cond->bt.trace->slot->CDPS; CDPS!=NIL; CDPS=CDPS->rest) {
            new_pref = NIL;
            pref = static_cast<preference *>(CDPS->first);
            if (pref->inst->match_goal_level == inst->match_goal_level
                && pref->in_tm) {
              push(thisAgent, pref, cond->bt.CDPS);
              preference_add_ref(pref);
            } else {
              new_pref = find_clone_for_level(pref, inst->match_goal_level);
              if (new_pref) {
                if (new_pref->in_tm) {
                  push(thisAgent, new_pref, cond->bt.CDPS);
                  preference_add_ref(new_pref);
                }
              }
            }
          }
        }
      } else {
        pref = cond->bt.trace->slot->preferences[PROHIBIT_PREFERENCE_TYPE];
        while (pref) {
          new_pref = NIL;
          if (pref->inst->match_goal_level == inst->match_goal_level && pref->in_tm) {
            push (thisAgent, pref, cond->bt.CDPS);
            preference_add_ref (pref);
          } else {
            new_pref = find_clone_for_level (pref, inst->match_goal_level);
            if (new_pref) {
              if (new_pref->in_tm) {
                push (thisAgent, new_pref, cond->bt.CDPS);
                preference_add_ref (new_pref);
              }
            }
          }
          pref = pref->next;
        }
      }
    }
  }
}

/* -----------------------------------------------------------------------
 Find Clone For Level

 This routines take a given preference and finds the clone of it whose
 match goal is at the given goal_stack_level.  (This is used to find the
 proper preference to backtrace through.)  If the given preference
 itself is at the right level, it is returned.  If there is no clone at
 the right level, NIL is returned.
 ----------------------------------------------------------------------- */

preference *find_clone_for_level(preference *p, goal_stack_level level) {
	preference *clone;

	if (!p) {
		/* --- if the wme doesn't even have a preference on it, we can't backtrace
		 at all (this happens with I/O and some architecture-created wmes --- */
		return NIL;
	}

	/* --- look at pref and all of its clones, find one at the right level --- */

	if (p->inst->match_goal_level == level)
		return p;

	for (clone = p->next_clone; clone != NIL; clone = clone->next_clone)
		if (clone->inst->match_goal_level == level)
			return clone;

	for (clone = p->prev_clone; clone != NIL; clone = clone->prev_clone)
		if (clone->inst->match_goal_level == level)
			return clone;

	/* --- if none was at the right level, we can't backtrace at all --- */
	return NIL;
}

/* =======================================================================

 Firer Utilities

 ======================================================================= */

/* -----------------------------------------------------------------------
 Find Match Goal

 Given an instantiation, this routines looks at the instantiated
 conditions to find its match goal.  It fills in inst->match_goal and
 inst->match_goal_level.  If there is a match goal, match_goal is set
 to point to the goal identifier.  If no goal was matched, match_goal
 is set to NIL and match_goal_level is set to ATTRIBUTE_IMPASSE_LEVEL.
 ----------------------------------------------------------------------- */

void find_match_goal(instantiation *inst) {
	Symbol *lowest_goal_so_far;
	goal_stack_level lowest_level_so_far;
	condition *cond;
	Symbol *id;

	lowest_goal_so_far = NIL;
	lowest_level_so_far = -1;
	for (cond = inst->top_of_instantiated_conditions; cond != NIL;
			cond = cond->next)
		if (cond->type == POSITIVE_CONDITION) {
			id = cond->bt.wme_->id;
			if (id->id.isa_goal)
				if (cond->bt.level > lowest_level_so_far) {
					lowest_goal_so_far = id;
					lowest_level_so_far = cond->bt.level;
				}
		}

	inst->match_goal = lowest_goal_so_far;
	if (lowest_goal_so_far)
		inst->match_goal_level = lowest_level_so_far;
	else
		inst->match_goal_level = ATTRIBUTE_IMPASSE_LEVEL;
}

/* -----------------------------------------------------------------------

 Executing the RHS Actions of an Instantiation

 Execute_action() executes a given RHS action.  For MAKE_ACTION's, it
 returns the created preference structure, or NIL if an error occurs.
 For FUNCALL_ACTION's, it returns NIL.

 Instantiate_rhs_value() returns the (symbol) instantiation of an
 rhs_value, or NIL if an error occurs.  It takes a new_id_level
 argument indicating what goal_stack_level a new id is to be created
 at, in case a gensym is needed for the instantiation of a variable.
 (although I'm not sure this is really needed.)

 As rhs unbound variables are encountered, they are instantiated with
 new gensyms.  These gensyms are then stored in the rhs_variable_bindings
 array, so if the same unbound variable is encountered a second time
 it will be instantiated with the same gensym.
 ----------------------------------------------------------------------- */

Symbol *instantiate_rhs_value(agent* thisAgent, rhs_value rv,
		goal_stack_level new_id_level, char new_id_letter,
		struct token_struct *tok, wme *w) {
	list *fl;
	list *arglist;
	cons *c, *prev_c, *arg_cons;
	rhs_function *rf;
	Symbol *result;
	Bool nil_arg_found;

	if (rhs_value_is_symbol(rv)) {

		result = rhs_value_to_symbol(rv);

		/*
		 Long-Winded Case-by-Case [Hopeful] Explanation

		 This has to do with long-term identifiers (LTIs) that exist within productions (including chunks/justifications).
		 The real issue is that identifiers, upon creation, require a goal level (used for promotion/demotion/garbage collection).
		 At the time of parsing a rule, we don't have this information, so we give it an invalid "unknown" value.
		 This is OK on the condition side of a rule, since the rete (we think) will just consider it another symbol used for matching.
		 However, it becomes hairy when LTIs are on the action side of a rule, with respect to the state of the LTI in working memory and the rule LHS.
		 Consider the following cases:

		 1. Identifier is LTI, does NOT exist as a LHS symbol
		 - we do NOT support this!!!  bad things will likely happen due to potential for adding an identifier to working memory
		 with an unknown goal level.

		 2. Attribute/Value is LTI, does NOT exist as a LHS symbol (!!!!!IMPORTANT CASE!!!!!)
		 - the caller of this function will supply new_id_level (probably based upon the level of the id).
		 - if this is valid (i.e. greater than 0), we use it.  else, ignore.
		 - we have a huge assert on add_wme_to_wm that will kill soar if we try to add an identifier to working memory with an invalid level.

		 3. Identifier/Attribute/Value is LTI, DOES exist as LHS symbol
		 - in this situation, we are *guaranteed* that the resulting LTI (since it is in WM) has a valid goal level.
		 - it should be noted that if a value, the level of the LTI may change during promotion/demotion/garbage collection,
		 but this is natural Soar behavior and outside our perview.

		 */
		if ((result->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE)
				&& (result->id.smem_lti != NIL)&&
				( result->id.level == SMEM_LTI_UNKNOWN_LEVEL ) &&
				( new_id_level > 0 ) ){
				result->id.level = new_id_level;
				result->id.promotion_level = new_id_level;
			}

		symbol_add_ref(result);
		return result;
	}

	if (rhs_value_is_unboundvar(rv)) {
		int64_t index;
		Symbol *sym;

		index = static_cast<int64_t>(rhs_value_to_unboundvar(rv));
		if (thisAgent->firer_highest_rhs_unboundvar_index < index)
			thisAgent->firer_highest_rhs_unboundvar_index = index;
		sym = *(thisAgent->rhs_variable_bindings + index);

		if (!sym) {
			sym = make_new_identifier(thisAgent, new_id_letter, new_id_level);
			*(thisAgent->rhs_variable_bindings + index) = sym;
			return sym;
		} else if (sym->common.symbol_type == VARIABLE_SYMBOL_TYPE) {
			new_id_letter = *(sym->var.name + 1);
			sym = make_new_identifier(thisAgent, new_id_letter, new_id_level);
			*(thisAgent->rhs_variable_bindings + index) = sym;
			return sym;
		} else {
			symbol_add_ref(sym);
			return sym;
		}
	}

	if (rhs_value_is_reteloc(rv)) {
		result = get_symbol_from_rete_loc(rhs_value_to_reteloc_levels_up(rv),
				rhs_value_to_reteloc_field_num(rv), tok, w);
		symbol_add_ref(result);
		return result;
	}

	fl = rhs_value_to_funcall_list(rv);
	rf = static_cast<rhs_function_struct *>(fl->first);

	/* --- build up a list of the argument values --- */
	prev_c = NIL;
	nil_arg_found = FALSE;
	arglist = NIL; /* unnecessary, but gcc -Wall warns without it */
	for (arg_cons = fl->rest; arg_cons != NIL; arg_cons = arg_cons->rest) {
		allocate_cons(thisAgent, &c);
		c->first = instantiate_rhs_value(thisAgent,
				static_cast<char *>(arg_cons->first), new_id_level,
				new_id_letter, tok, w);
		if (!c->first)
			nil_arg_found = TRUE;
		if (prev_c)
			prev_c->rest = c;
		else
			arglist = c;
		prev_c = c;
	}
	if (prev_c)
		prev_c->rest = NIL;
	else
		arglist = NIL;

	/* --- if all args were ok, call the function --- */

	if (!nil_arg_found) {
		// stop the kernel timer while doing RHS funcalls  KJC 11/04
		// the total_cpu timer needs to be updated in case RHS fun is statsCmd
#ifndef NO_TIMING_STUFF
		thisAgent->timers_kernel.stop();
		thisAgent->timers_cpu.stop();
		thisAgent->timers_total_kernel_time.update(thisAgent->timers_kernel);
		thisAgent->timers_total_cpu_time.update(thisAgent->timers_cpu);
		thisAgent->timers_cpu.start();
#endif

		result = (*(rf->f))(thisAgent, arglist, rf->user_data);

#ifndef NO_TIMING_STUFF  // restart the kernel timer
		thisAgent->timers_kernel.start();
#endif

	} else
		result = NIL;

	/* --- scan through arglist, dereference symbols and deallocate conses --- */
	for (c = arglist; c != NIL; c = c->rest)
		if (c->first)
			symbol_remove_ref(thisAgent, static_cast<Symbol *>(c->first));
	free_list(thisAgent, arglist);

	return result;
}

preference *execute_action(agent* thisAgent, action *a,
		struct token_struct *tok, wme *w) {
	Symbol *id, *attr, *value, *referent;
	char first_letter;

	if (a->type == FUNCALL_ACTION) {
		value = instantiate_rhs_value(thisAgent, a->value, -1, 'v', tok, w);
		if (value)
			symbol_remove_ref(thisAgent, value);
		return NIL;
	}

	attr = NIL;
	value = NIL;
	referent = NIL;

	id = instantiate_rhs_value(thisAgent, a->id, -1, 's', tok, w);
	if (!id)
		goto abort_execute_action;
	if (id->common.symbol_type != IDENTIFIER_SYMBOL_TYPE) {
		print_with_symbols(thisAgent,
				"Error: RHS makes a preference for %y (not an identifier)\n",
				id);
		goto abort_execute_action;
	}

	attr = instantiate_rhs_value(thisAgent, a->attr, id->id.level, 'a', tok, w);
	if (!attr)
		goto abort_execute_action;

	first_letter = first_letter_from_symbol(attr);

	value = instantiate_rhs_value(thisAgent, a->value, id->id.level,
			first_letter, tok, w);
	if (!value)
		goto abort_execute_action;

	if (preference_is_binary(a->preference_type)) {
		referent = instantiate_rhs_value(thisAgent, a->referent, id->id.level,
				first_letter, tok, w);
		if (!referent)
			goto abort_execute_action;
	}

	if (((a->preference_type != ACCEPTABLE_PREFERENCE_TYPE)
			&& (a->preference_type != REJECT_PREFERENCE_TYPE))
			&& (!(id->id.isa_goal && (attr == thisAgent->operator_symbol)))) {
		print_with_symbols(thisAgent,
				"\nError: attribute preference other than +/- for %y ^%y -- ignoring it.",
				id, attr);
		goto abort_execute_action;
	}

	return make_preference(thisAgent, a->preference_type, id, attr, value,
			referent);

	abort_execute_action: /* control comes here when some error occurred */
	if (id)
		symbol_remove_ref(thisAgent, id);
	if (attr)
		symbol_remove_ref(thisAgent, attr);
	if (value)
		symbol_remove_ref(thisAgent, value);
	if (referent)
		symbol_remove_ref(thisAgent, referent);
	return NIL;
}

/* -----------------------------------------------------------------------
 Fill In New Instantiation Stuff

 This routine fills in a newly created instantiation structure with
 various information.   At input, the instantiation should have:
 - preferences_generated filled in;
 - instantiated conditions filled in;
 - top-level positive conditions should have bt.wme_, bt.level, and
 bt.trace filled in, but bt.wme_ and bt.trace shouldn't have their
 reference counts incremented yet.

 This routine does the following:
 - increments reference count on production;
 - fills in match_goal and match_goal_level;
 - for each top-level positive cond:
 replaces bt.trace with the preference for the correct level,
 updates reference counts on bt.pref and bt.wmetraces and wmes
 - for each preference_generated, adds that pref to the list of all
 pref's for the match goal
 - fills in backtrace_number;
 - if "need_to_do_support_calculations" is TRUE, calculates o-support
 for preferences_generated;
 ----------------------------------------------------------------------- */

void fill_in_new_instantiation_stuff(agent* thisAgent, instantiation *inst,
		Bool need_to_do_support_calculations) {
	condition *cond;
	preference *p;
	goal_stack_level level;

	production_add_ref(inst->prod);

	find_match_goal(inst);

	level = inst->match_goal_level;

	/* Note: since we'll never backtrace through instantiations at the top
	 level, it might make sense to not increment the reference counts
	 on the wmes and preferences here if the instantiation is at the top
	 level.  As it stands now, we could gradually accumulate garbage at
	 the top level if we have a never-ending sequence of production
	 firings at the top level that chain on each other's results.  (E.g.,
	 incrementing a counter on every decision cycle.)  I'm leaving it this
	 way for now, because if we go to S-Support, we'll (I think) need to
	 save these around (maybe). */

	/* KJC 6/00:  maintaining all the top level ref cts does have a big
	 impact on memory pool usage and also performance (due to malloc).
	 (See tests done by Scott Wallace Fall 99.)	 Therefore added 
	 preprocessor macro so that by unsetting macro the top level ref cts are not 
	 incremented.  It's possible that in some systems, these ref cts may 
	 be desireable: they can be added by defining DO_TOP_LEVEL_REF_CTS
	 */

	for (cond = inst->top_of_instantiated_conditions; cond != NIL;
			cond = cond->next)
		if (cond->type == POSITIVE_CONDITION) {
#ifdef DO_TOP_LEVEL_REF_CTS
			wme_add_ref (cond->bt.wme_);
#else
			if (level > TOP_GOAL_LEVEL)
				wme_add_ref(cond->bt.wme_);
#endif
			/* --- if trace is for a lower level, find one for this level --- */
			if (cond->bt.trace) {
				if (cond->bt.trace->inst->match_goal_level > level) {
					cond->bt.trace = find_clone_for_level(cond->bt.trace,
							level);
				}
#ifdef DO_TOP_LEVEL_REF_CTS
				if (cond->bt.trace) preference_add_ref (cond->bt.trace);
#else
				if ((cond->bt.trace) && (level > TOP_GOAL_LEVEL))
					preference_add_ref(cond->bt.trace);
#endif
			}
		}

	if (inst->match_goal) {
		for (p = inst->preferences_generated; p != NIL; p = p->inst_next) {
			insert_at_head_of_dll(inst->match_goal->id.preferences_from_goal, p,
					all_of_goal_next, all_of_goal_prev);
			p->on_goal_list = TRUE;
		}
	}
	inst->backtrace_number = 0;

	if ((thisAgent->o_support_calculation_type == 0)
			|| (thisAgent->o_support_calculation_type == 3)
			|| (thisAgent->o_support_calculation_type == 4)) {
		/* --- do calc's the normal Soar 6 way --- */
		if (need_to_do_support_calculations)
			calculate_support_for_instantiation_preferences(thisAgent, inst);
	} else if (thisAgent->o_support_calculation_type == 1) {
		if (need_to_do_support_calculations)
			calculate_support_for_instantiation_preferences(thisAgent, inst);
		/* --- do calc's both ways, warn on differences --- */
		if ((inst->prod->declared_support != DECLARED_O_SUPPORT)
				&& (inst->prod->declared_support != DECLARED_I_SUPPORT)) {
			/* --- At this point, we've done them the normal way.  To look for
			 differences, save o-support flags on a list, then do Doug's
			 calculations, then compare and restore saved flags. --- */
			list *saved_flags;
			preference *pref;
			Bool difference_found;
			saved_flags = NIL;
			for (pref = inst->preferences_generated; pref != NIL;
					pref = pref->inst_next)
				push(thisAgent, (pref->o_supported ? pref : NIL), saved_flags);
			saved_flags = destructively_reverse_list(saved_flags);
			dougs_calculate_support_for_instantiation_preferences(thisAgent,
					inst);
			difference_found = FALSE;
			for (pref = inst->preferences_generated; pref != NIL;
					pref = pref->inst_next) {
				cons *c;
				Bool b;
				c = saved_flags;
				saved_flags = c->rest;
				b = (c->first ? TRUE : FALSE);
				free_cons(thisAgent, c);
				if (pref->o_supported != b)
					difference_found = TRUE;
				pref->o_supported = b;
			}
			if (difference_found) {
				print_with_symbols(thisAgent,
						"\n*** O-support difference found in production %y",
						inst->prod->name);
			}
		}
	} else {
		/* --- do calc's Doug's way --- */
		if ((inst->prod->declared_support != DECLARED_O_SUPPORT)
				&& (inst->prod->declared_support != DECLARED_I_SUPPORT)) {
			dougs_calculate_support_for_instantiation_preferences(thisAgent,
					inst);
		}
	}
}

/* =======================================================================

 Main Firer Routines

 Init_firer() should be called at startup time.  Do_preference_phase()
 is called from the top level to run the whole preference phase.

 Preference phase follows this sequence:

 (1) Productions are fired for new matches.  As productions are fired,
 their instantiations are stored on the list newly_created_instantiations,
 linked via the "next" fields in the instantiation structure.  No
 preferences are actually asserted yet.

 (2) Instantiations are retracted; their preferences are retracted.

 (3) Preferences (except o-rejects) from newly_created_instantiations
 are asserted, and these instantiations are removed from the
 newly_created_instantiations list and moved over to the per-production
 lists of instantiations of that production.

 (4) Finally, o-rejects are processed.

 Note: Using the O_REJECTS_FIRST flag, step (4) becomes step (2b)
 ======================================================================= */

void init_firer(agent* thisAgent) {
	init_memory_pool(thisAgent, &thisAgent->instantiation_pool,
			sizeof(instantiation), "instantiation");
}

/* --- Macro returning TRUE iff we're supposed to trace firings for the
 given instantiation, which should have the "prod" field filled in. --- */
#ifdef USE_MACROS
#define trace_firings_of_inst(thisAgent, inst) \
  ((inst)->prod && \
   (thisAgent->sysparams[TRACE_FIRINGS_OF_USER_PRODS_SYSPARAM+(inst)->prod->type] || \
    ((inst)->prod->trace_firings)))
#else
inline Bool trace_firings_of_inst(agent* thisAgent, instantiation * inst) {
	return ((inst)->prod
			&& (thisAgent->sysparams[TRACE_FIRINGS_OF_USER_PRODS_SYSPARAM
					+ (inst)->prod->type] || ((inst)->prod->trace_firings)));
}
#endif

/* -----------------------------------------------------------------------
 Create Instantiation

 This builds the instantiation for a new match, and adds it to
 newly_created_instantiations.  It also calls chunk_instantiation() to
 do any necessary chunk or justification building.
 ----------------------------------------------------------------------- */

void create_instantiation(agent* thisAgent, production *prod,
		struct token_struct *tok, wme *w) {
	instantiation *inst;
	condition *cond;
	preference *pref;
	action *a;
	cons *c;
	Bool need_to_do_support_calculations;
	Bool trace_it;
	int64_t index;
	Symbol **cell;

#ifdef BUG_139_WORKAROUND
	/* New waterfall model: this is now checked for before we call this function */
	assert(prod->type != JUSTIFICATION_PRODUCTION_TYPE);
	/* RPM workaround for bug #139: don't fire justifications */
	//if (prod->type == JUSTIFICATION_PRODUCTION_TYPE) {
	//    return;
	//}
#endif

	allocate_with_pool(thisAgent, &thisAgent->instantiation_pool, &inst);
	inst->next = thisAgent->newly_created_instantiations;
	thisAgent->newly_created_instantiations = inst;
	inst->prod = prod;
	inst->rete_token = tok;
	inst->rete_wme = w;
	inst->reliable = true;
	inst->in_ms = TRUE;

	/* REW: begin   09.15.96 */
	/*  We want to initialize the GDS_evaluated_already flag
	 *  when a new instantiation is created.
	 */

	inst->GDS_evaluated_already = FALSE;

	if (thisAgent->soar_verbose_flag == TRUE) {
		print_with_symbols(thisAgent, "\n   in create_instantiation: %y",
				inst->prod->name);
		char buf[256];
		SNPRINTF(buf, 254, "in create_instantiation: %s",
				symbol_to_string(thisAgent, inst->prod->name, true, 0, 0));
		xml_generate_verbose(thisAgent, buf);
	}
	/* REW: end   09.15.96 */

	thisAgent->production_being_fired = inst->prod;
	prod->firing_count++;
	thisAgent->production_firing_count++;

	/* --- build the instantiated conditions, and bind LHS variables --- */
	p_node_to_conditions_and_nots(thisAgent, prod->p_node, tok, w,
			&(inst->top_of_instantiated_conditions),
			&(inst->bottom_of_instantiated_conditions), &(inst->nots), NIL);

	/* --- record the level of each of the wmes that was positively tested --- */
	for (cond = inst->top_of_instantiated_conditions; cond != NIL;
			cond = cond->next) {
		if (cond->type == POSITIVE_CONDITION) {
			cond->bt.level = cond->bt.wme_->id->id.level;
			cond->bt.trace = cond->bt.wme_->preference;
		}
	}

	/* --- print trace info --- */
	trace_it = trace_firings_of_inst(thisAgent, inst);
	if (trace_it) {
		if (get_printer_output_column(thisAgent) != 1)
			print(thisAgent, "\n"); /* AGR 617/634 */
		print(thisAgent, "Firing ");
		print_instantiation_with_wmes(thisAgent, inst,
				static_cast<wme_trace_type>(thisAgent->sysparams[TRACE_FIRINGS_WME_TRACE_TYPE_SYSPARAM]),
				0);
	}

	/* --- initialize rhs_variable_bindings array with names of variables
	 (if there are any stored on the production -- for chunks there won't
	 be any) --- */
	index = 0;
	cell = thisAgent->rhs_variable_bindings;
	for (c = prod->rhs_unbound_variables; c != NIL; c = c->rest) {
		*(cell++) = static_cast<symbol_union *>(c->first);
		index++;
	}
	thisAgent->firer_highest_rhs_unboundvar_index = index - 1;

	/* 7.1/8 merge: Not sure about this.  This code in 704, but not in either 7.1 or 703/soar8 */
	/* --- Before executing the RHS actions, tell the user that the -- */
	/* --- phase has changed to output by printing the arrow --- */
	if (trace_it && thisAgent->sysparams[TRACE_FIRINGS_PREFERENCES_SYSPARAM]) {
		print(thisAgent, " -->\n");
		xml_object(thisAgent, kTagActionSideMarker);
	}

	/* --- execute the RHS actions, collect the results --- */
	inst->preferences_generated = NIL;
	need_to_do_support_calculations = FALSE;
	for (a = prod->action_list; a != NIL; a = a->next) {

		if (prod->type != TEMPLATE_PRODUCTION_TYPE) {
			pref = execute_action(thisAgent, a, tok, w);
		} else {
			pref = NIL;
			/*Symbol *result = */rl_build_template_instantiation(thisAgent,
					inst, tok, w);
		}

		/* SoarTech changed from an IF stmt to a WHILE loop to support GlobalDeepCpy */
		while (pref) {
			/* The parser assumes that any rhs preference of the form
			 *
			 * (<s> ^operator <o> = <x>)
			 *
			 * is a binary indifferent preference, because it assumes <x> is an
			 * operator. However, it could be the case that <x> is actually bound to
			 * a number, which would make this a numeric indifferent preference. The
			 * parser had no way of easily figuring this out, but it's easy to check
			 * here.
			 *
			 * jzxu April 22, 2009
			 */
			if ((pref->type == BINARY_INDIFFERENT_PREFERENCE_TYPE)
					&& ((pref->referent->var.common_symbol_info.symbol_type
							== FLOAT_CONSTANT_SYMBOL_TYPE)
							|| (pref->referent->var.common_symbol_info.symbol_type
									== INT_CONSTANT_SYMBOL_TYPE))) {
				pref->type = NUMERIC_INDIFFERENT_PREFERENCE_TYPE;
			}

			pref->inst = inst;
			insert_at_head_of_dll(inst->preferences_generated, pref, inst_next,
					inst_prev);
			if (inst->prod->declared_support == DECLARED_O_SUPPORT)
				pref->o_supported = TRUE;
			else if (inst->prod->declared_support == DECLARED_I_SUPPORT) {
				pref->o_supported = FALSE;
			} else {

				pref->o_supported =
						(thisAgent->FIRING_TYPE == PE_PRODS) ? TRUE : FALSE;
				/* REW: end   09.15.96 */
			}

			/* TEMPORARY HACK (Ideally this should be doable through
			 the external kernel interface but for now using a
			 couple of global STL lists to get this information
			 from the rhs function to this preference adding code)

			 Getting the next pref from the set of possible prefs
			 added by the deep copy rhs function */
			if (glbDeepCopyWMEs != 0) {
				wme* tempwme = glbDeepCopyWMEs;
				pref = make_preference(thisAgent, a->preference_type,
						tempwme->id, tempwme->attr, tempwme->value, 0);
				glbDeepCopyWMEs = tempwme->next;
				deallocate_wme(thisAgent, tempwme);
			} else {
				pref = 0;
			}
		}
	}

	/* --- reset rhs_variable_bindings array to all zeros --- */
	index = 0;
	cell = thisAgent->rhs_variable_bindings;
	while (index++ <= thisAgent->firer_highest_rhs_unboundvar_index)
		*(cell++) = NIL;

	/* --- fill in lots of other stuff --- */
	fill_in_new_instantiation_stuff(thisAgent, inst,
			need_to_do_support_calculations);

	/* --- print trace info: printing preferences --- */
	/* Note: can't move this up, since fill_in_new_instantiation_stuff gives
	 the o-support info for the preferences we're about to print */
	if (trace_it && thisAgent->sysparams[TRACE_FIRINGS_PREFERENCES_SYSPARAM]) {
		for (pref = inst->preferences_generated; pref != NIL;
				pref = pref->inst_next) {
			print(thisAgent, " ");
			print_preference(thisAgent, pref);
		}
	}

	/* Copy any context-dependent preferences for conditions of this instantiation */
	build_CDPS(thisAgent, inst);

	thisAgent->production_being_fired = NIL;

	/* --- build chunks/justifications if necessary --- */
	chunk_instantiation(thisAgent, inst, false,
			&(thisAgent->newly_created_instantiations));

	/* MVP 6-8-94 */
	if (!thisAgent->system_halted) {
		/* --- invoke callback function --- */
		soar_invoke_callbacks(thisAgent, FIRING_CALLBACK,
				static_cast<soar_call_data>(inst));

	}
}

/**
 * New waterfall model:
 * Returns true if the function create_instantiation should run for this production.
 * Used to delay firing of matches in the inner preference loop.
 */
Bool shouldCreateInstantiation(agent* thisAgent, production *prod,
		struct token_struct *tok, wme *w) {
	if (thisAgent->active_level == thisAgent->highest_active_level) {
		return TRUE;
	}

	if (prod->type == TEMPLATE_PRODUCTION_TYPE) {
		return TRUE;
	}

	// Scan RHS identifiers for their levels, don't fire those at or higher than the change level
	action* a = NIL;
	for (a = prod->action_list; a != NIL; a = a->next) {
		if (a->type == FUNCALL_ACTION) {
			continue;
		}

		// skip unbound variables
		if (rhs_value_is_unboundvar(a->id)) {
			continue;
		}

		// try to make a symbol
		Symbol* sym = NIL;
		if (rhs_value_is_symbol(a->id)) {
			sym = rhs_value_to_symbol(a->id);
		} else {
			if (rhs_value_is_reteloc(a->id)) {
				sym = get_symbol_from_rete_loc(
						rhs_value_to_reteloc_levels_up(a->id),
						rhs_value_to_reteloc_field_num(a->id), tok, w);
			}
		}
		assert(sym != NIL);

		// check level for legal change
		if (sym->id.level <= thisAgent->change_level) {
			if (thisAgent->sysparams[TRACE_WATERFALL_SYSPARAM]) {
				print_with_symbols(thisAgent,
						"*** Waterfall: aborting firing because (%y * *)", sym);
				print(thisAgent,
						" level %d is on or higher (lower int) than change level %d\n",
						sym->id.level, thisAgent->change_level);
			}
			return FALSE;
		}
	}
	return TRUE;
}
/* -----------------------------------------------------------------------
 Deallocate Instantiation

 This deallocates the given instantiation.  This should only be invoked
 via the possibly_deallocate_instantiation() macro.
 ----------------------------------------------------------------------- */

void deallocate_instantiation(agent* thisAgent, instantiation *inst) {
	condition *cond;

	/* mvp 5-17-94 */
	list *c, *c_old;
	preference *pref;
	goal_stack_level level;

#ifdef USE_MEM_POOL_ALLOCATORS
	cond_mpool_list cond_stack = cond_mpool_list(
			soar_module::soar_memory_pool_allocator<condition*>(thisAgent));
	inst_mpool_list inst_list = inst_mpool_list(
			soar_module::soar_memory_pool_allocator<instantiation*>(thisAgent));
#else
	cond_mpool_list cond_stack;
	inst_mpool_list inst_list;
#endif

	inst_list.push_back(inst);
	inst_mpool_list::iterator next_iter = inst_list.begin();

	while (next_iter != inst_list.end()) {
		inst = *next_iter;
		assert(inst);
		++next_iter;

#ifdef DEBUG_INSTANTIATIONS
		if (inst->prod)
		print_with_symbols (thisAgent, "\nDeallocate instantiation of %y",inst->prod->name);
#endif

		level = inst->match_goal_level;

		for (cond = inst->top_of_instantiated_conditions; cond != NIL; cond =
				cond->next) {
			if (cond->type == POSITIVE_CONDITION) {

			  if (cond->bt.CDPS) {
          c_old = c = cond->bt.CDPS;
          cond->bt.CDPS = NIL;
          for (; c != NIL; c = c->rest) {
            pref = static_cast<preference *>(c->first);
#ifdef DO_TOP_LEVEL_REF_CTS
            if (level > TOP_GOAL_LEVEL)
#endif
            {
              preference_remove_ref(thisAgent, pref);
            }
          }
          free_list(thisAgent, c_old);
        }

				/*	voigtjr, nlderbin:
				 We flattened out the following recursive loop in order to prevent a stack
				 overflow that happens when the chain of backtrace instantiations is very long:

				 retract_instantiation
				 possibly_deallocate_instantiation
				 loop start:
				 deallocate_instantiation (here)
				 preference_remove_ref
				 possibly_deallocate_preferences_and_clones
				 deallocate_preference
				 possibly_deallocate_instantiation
				 goto loop start
				 */
#ifndef DO_TOP_LEVEL_REF_CTS
				if (level > TOP_GOAL_LEVEL)
#endif
				{
					wme_remove_ref(thisAgent, cond->bt.wme_);
					if (cond->bt.trace) {
						cond->bt.trace->reference_count--;
						if (cond->bt.trace->reference_count == 0) {
							preference *clone;

							if (cond->bt.trace->reference_count) {
								continue;
							}
							bool has_active_clones = false;
							for (clone = cond->bt.trace->next_clone;
									clone != NIL; clone = clone->next_clone) {
								if (clone->reference_count) {
									has_active_clones = true;
								}
							}
							if (has_active_clones) {
								continue;
							}
							for (clone = cond->bt.trace->prev_clone;
									clone != NIL; clone = clone->prev_clone) {
								if (clone->reference_count) {
									has_active_clones = true;
								}
							}
							if (has_active_clones) {
								continue;
							}

							// The clones are hopefully a simple case so we just call deallocate_preference on them.
							// Someone needs to create a test case to push this boundary...
							{
								preference* clone = cond->bt.trace->next_clone;
								preference* next;
								while (clone) {
									next = clone->next_clone;
									deallocate_preference(thisAgent, clone);
									clone = next;
								}
								clone = cond->bt.trace->prev_clone;
								while (clone) {
									next = clone->prev_clone;
									deallocate_preference(thisAgent, clone);
									clone = next;
								}
							}

							/* --- deallocate pref --- */
							/* --- remove it from the list of bt.trace's for its match goal --- */
							if (cond->bt.trace->on_goal_list) {
								remove_from_dll(
										cond->bt.trace->inst->match_goal->id.preferences_from_goal,
										cond->bt.trace, all_of_goal_next,
										all_of_goal_prev);
							}

							/* --- remove it from the list of bt.trace's from that instantiation --- */
							remove_from_dll(
									cond->bt.trace->inst->preferences_generated,
									cond->bt.trace, inst_next, inst_prev);
							if ((!cond->bt.trace->inst->preferences_generated)
									&& (!cond->bt.trace->inst->in_ms)) {
								next_iter = inst_list.insert(next_iter,
										cond->bt.trace->inst);
							}

							cond_stack.push_back(cond);
						} // if
					} // if
				} // if
				/* voigtjr, nlderbin end */
			} // if
		} // for
	} // while

	// free condition symbols and pref
	while (!cond_stack.empty()) {
		condition* temp = cond_stack.back();
		cond_stack.pop_back();

		/* --- dereference component symbols --- */
		symbol_remove_ref(thisAgent, temp->bt.trace->id);
		symbol_remove_ref(thisAgent, temp->bt.trace->attr);
		symbol_remove_ref(thisAgent, temp->bt.trace->value);
		if (preference_is_binary(temp->bt.trace->type)) {
			symbol_remove_ref(thisAgent, temp->bt.trace->referent);
		}

		if (temp->bt.trace->wma_o_set) {
			wma_remove_pref_o_set(thisAgent, temp->bt.trace);
		}

		/* --- free the memory --- */
		free_with_pool(&thisAgent->preference_pool, temp->bt.trace);
	}

	// free instantiations in the reverse order
	inst_mpool_list::reverse_iterator riter = inst_list.rbegin();
	while (riter != inst_list.rend()) {
		instantiation* temp = *riter;
		++riter;

		deallocate_condition_list(thisAgent,
				temp->top_of_instantiated_conditions);
		deallocate_list_of_nots(thisAgent, temp->nots);
		if (temp->prod) {
			production_remove_ref(thisAgent, temp->prod);
		}
		free_with_pool(&thisAgent->instantiation_pool, temp);
	}
}

/* -----------------------------------------------------------------------
 Retract Instantiation

 This retracts the given instantiation.
 ----------------------------------------------------------------------- */

void retract_instantiation(agent* thisAgent, instantiation *inst) {
	preference *pref, *next;
	Bool retracted_a_preference;
	Bool trace_it;

	/* --- invoke callback function --- */
	soar_invoke_callbacks(thisAgent, RETRACTION_CALLBACK,
			static_cast<soar_call_data>(inst));

	retracted_a_preference = FALSE;

	trace_it = trace_firings_of_inst(thisAgent, inst);

	/* --- retract any preferences that are in TM and aren't o-supported --- */
	pref = inst->preferences_generated;

	while (pref != NIL) {
		next = pref->inst_next;
		if (pref->in_tm && (!pref->o_supported)) {

			if (trace_it) {
				if (!retracted_a_preference) {
					if (get_printer_output_column(thisAgent) != 1)
						print(thisAgent, "\n"); /* AGR 617/634 */
					print(thisAgent, "Retracting ");
					print_instantiation_with_wmes(thisAgent, inst,
							static_cast<wme_trace_type>(thisAgent->sysparams[TRACE_FIRINGS_WME_TRACE_TYPE_SYSPARAM]),
							1);
					if (thisAgent->sysparams[TRACE_FIRINGS_PREFERENCES_SYSPARAM]) {
						print(thisAgent, " -->\n");
						xml_object(thisAgent, kTagActionSideMarker);
					}
				}
				if (thisAgent->sysparams[TRACE_FIRINGS_PREFERENCES_SYSPARAM]) {
					print(thisAgent, " ");
					print_preference(thisAgent, pref);
				}
			}

			remove_preference_from_tm(thisAgent, pref);
			retracted_a_preference = TRUE;
		}
		pref = next;
	}

	/* --- remove inst from list of instantiations of this production --- */
	remove_from_dll(inst->prod->instantiations, inst, next, prev);

	/* --- if retracting a justification, excise it --- */
	/*
	 * if the reference_count on the production is 1 (or less) then the
	 * only thing supporting this justification is the instantiation, hence
	 * it has already been excised, and doing it again is wrong.
	 */
	production* prod = inst->prod;
	if (prod->type == JUSTIFICATION_PRODUCTION_TYPE
			&& prod->reference_count > 1) {
		excise_production(thisAgent, prod, FALSE);
	} else if (prod->type == CHUNK_PRODUCTION_TYPE) {
		rl_param_container::apoptosis_choices apoptosis =
				thisAgent->rl_params->apoptosis->get_value();

		// we care about production history of chunks if...
		// - we are dealing with a non-RL rule and all chunks are subject to apoptosis OR
		// - we are dealing with an RL rule that...
		//   - has not been updated by RL AND
		//   - is not in line to be updated by RL
		if (apoptosis != rl_param_container::apoptosis_none) {
			if ((!prod->rl_rule
					&& (apoptosis == rl_param_container::apoptosis_chunks))
					|| (prod->rl_rule
							&& (static_cast<int64_t>(prod->rl_update_count) == 0)
							&& (prod->rl_ref_count == 0))) {
				thisAgent->rl_prods->reference_object(prod, 1);
			}
		}
	}

	/* --- mark as no longer in MS, and possibly deallocate  --- */
	inst->in_ms = FALSE;
	possibly_deallocate_instantiation(thisAgent, inst);
}

/* -----------------------------------------------------------------------
 Assert New Preferences

 This routine scans through newly_created_instantiations, asserting
 each preference generated except for o-rejects.  It also removes
 each instantiation from newly_created_instantiations, linking each
 onto the list of instantiations for that particular production.
 O-rejects are buffered and handled after everything else.

 Note that some instantiations on newly_created_instantiations are not
 in the match set--for the initial instantiations of chunks/justifications,
 if they don't match WM, we have to assert the o-supported preferences
 and throw away the rest.
 ----------------------------------------------------------------------- */

void assert_new_preferences(agent* thisAgent, pref_buffer_list& bufdeallo) {
	instantiation *inst, *next_inst;
	preference *pref, *next_pref;
	preference *o_rejects;

	o_rejects = NIL;

	/* REW: begin 09.15.96 */
	if (thisAgent->soar_verbose_flag == TRUE) {
		printf("\n   in assert_new_preferences:");
		xml_generate_verbose(thisAgent, "in assert_new_preferences:");
	}
	/* REW: end   09.15.96 */

#ifdef O_REJECTS_FIRST
	{

		//slot *s;
		//preference *p, *next_p;

		/* Do an initial loop to process o-rejects, then re-loop
		 to process normal preferences.
		 */
		for (inst = thisAgent->newly_created_instantiations; inst != NIL; inst =
				next_inst) {
			next_inst = inst->next;

			for (pref = inst->preferences_generated; pref != NIL; pref =
					next_pref) {
				next_pref = pref->inst_next;
				if ((pref->type == REJECT_PREFERENCE_TYPE)
						&& (pref->o_supported)) {
					/* --- o-reject: just put it in the buffer for later --- */
					pref->next = o_rejects;
					o_rejects = pref;
				}
			}
		}

		if (o_rejects)
			process_o_rejects_and_deallocate_them(thisAgent, o_rejects,
					bufdeallo);

		//               s = find_slot(pref->id, pref->attr);
		//               if (s) {
		//                   /* --- remove all pref's in the slot that have the same value --- */
		//                   p = s->all_preferences;
		//                   while (p) {
		//                       next_p = p->all_of_slot_next;
		//                       if (p->value == pref->value)
		//                           remove_preference_from_tm(thisAgent, p);
		//                       p = next_p;
		//                   }
		//               }
		////preference_remove_ref (thisAgent, pref);
		//           }
		//       }
		//   }

	}
#endif

	for (inst = thisAgent->newly_created_instantiations; inst != NIL; inst =
			next_inst) {
		next_inst = inst->next;
		if (inst->in_ms)
			insert_at_head_of_dll(inst->prod->instantiations, inst, next, prev);

		/* REW: begin 09.15.96 */
		if (thisAgent->soar_verbose_flag == TRUE) {
			print_with_symbols(thisAgent,
					"\n      asserting instantiation: %y\n", inst->prod->name);
			char buf[256];
			SNPRINTF(buf, 254, "asserting instantiation: %s",
					symbol_to_string(thisAgent, inst->prod->name, true, 0, 0));
			xml_generate_verbose(thisAgent, buf);
		}
		/* REW: end   09.15.96 */

		for (pref = inst->preferences_generated; pref != NIL; pref =
				next_pref) {
			next_pref = pref->inst_next;
			if ((pref->type == REJECT_PREFERENCE_TYPE) && (pref->o_supported)) {
#ifndef O_REJECTS_FIRST
				/* --- o-reject: just put it in the buffer for later --- */
				pref->next = o_rejects;
				o_rejects = pref;
#endif

				/* REW: begin 09.15.96 */
				/* No knowledge retrieval necessary in Operand2 */
				/* REW: end   09.15.96 */

			} else if (inst->in_ms || pref->o_supported) {
				/* --- normal case --- */
				if (add_preference_to_tm(thisAgent, pref)) {
					/* REW: begin 09.15.96 */
					/* No knowledge retrieval necessary in Operand2 */
					/* REW: end   09.15.96 */

					if (wma_enabled(thisAgent)) {
						wma_activate_wmes_in_pref(thisAgent, pref);
					}
				} else {
					// NLD: the preference was o-supported, at
					// the top state, and was asserting an acceptable 
					// preference for a WME that was already
					// o-supported. hence unnecessary.

					preference_add_ref(pref);
					preference_remove_ref(thisAgent, pref);
				}
			} else {
				/* --- inst. is refracted chunk, and pref. is not o-supported:
				 remove the preference --- */

				/* --- first splice it out of the clones list--otherwise we might
				 accidentally deallocate some clone that happens to have refcount==0
				 just because it hasn't been asserted yet --- */

				if (pref->next_clone)
					pref->next_clone->prev_clone = pref->prev_clone;
				if (pref->prev_clone)
					pref->prev_clone->next_clone = pref->next_clone;
				pref->next_clone = pref->prev_clone = NIL;

				/* --- now add then remove ref--this should result in deallocation */
				preference_add_ref(pref);
				preference_remove_ref(thisAgent, pref);
			}
		}
	}
#ifndef O_REJECTS_FIRST
	if (o_rejects)
	process_o_rejects_and_deallocate_them (thisAgent, o_rejects, bufdeallo);
#endif
}

/* -----------------------------------------------------------------------
 Do Preference Phase

 This routine is called from the top level to run the preference phase.
 ----------------------------------------------------------------------- */

void do_preference_phase(agent* thisAgent) {
	instantiation *inst = 0;

	/* AGR 617/634:  These are 2 bug reports that report the same problem,
	 namely that when 2 chunk firings happen in succession, there is an
	 extra newline printed out.  The simple fix is to monitor
	 get_printer_output_column and see if it's at the beginning of a line
	 or not when we're ready to print a newline.  94.11.14 */

	if (thisAgent->sysparams[TRACE_PHASES_SYSPARAM]) {
		if (thisAgent->current_phase == APPLY_PHASE) { /* it's always IE for PROPOSE */
			xml_begin_tag(thisAgent, kTagSubphase);
			xml_att_val(thisAgent, kPhase_Name,
					kSubphaseName_FiringProductions);
			switch (thisAgent->FIRING_TYPE) {
			case PE_PRODS:
				print(thisAgent,
						"\t--- Firing Productions (PE) For State At Depth %d ---\n",
						thisAgent->active_level); // SBW 8/4/2008: added active_level
				xml_att_val(thisAgent, kPhase_FiringType, kPhaseFiringType_PE);
				break;
			case IE_PRODS:
				print(thisAgent,
						"\t--- Firing Productions (IE) For State At Depth %d ---\n",
						thisAgent->active_level); // SBW 8/4/2008: added active_level
				xml_att_val(thisAgent, kPhase_FiringType, kPhaseFiringType_IE);
				break;
			}
			std::string levelString;
			to_string(thisAgent->active_level, levelString);
			xml_att_val(thisAgent, kPhase_LevelNum, levelString.c_str()); // SBW 8/4/2008: active_level for XML output mode
			xml_end_tag(thisAgent, kTagSubphase);
		}
	}

	if (wma_enabled(thisAgent)) {
		wma_activate_wmes_tested_in_prods(thisAgent);
	}

	/* New waterfall model: */
	// Save previous active level for usage on next elaboration cycle.
	thisAgent->highest_active_level = thisAgent->active_level;
	thisAgent->highest_active_goal = thisAgent->active_goal;

	thisAgent->change_level = thisAgent->highest_active_level;
	thisAgent->next_change_level = thisAgent->highest_active_level;

	// Temporary list to buffer deallocation of some preferences until
	// the inner elaboration loop is over.
#ifdef USE_MEM_POOL_ALLOCATORS
	pref_buffer_list bufdeallo = pref_buffer_list(
			soar_module::soar_memory_pool_allocator<preference*>(thisAgent));
#else
	pref_buffer_list bufdeallo;
#endif

	// inner elaboration cycle
	for (;;) {
		thisAgent->change_level = thisAgent->next_change_level;

		if (thisAgent->sysparams[TRACE_WATERFALL_SYSPARAM]) {
			print(thisAgent, "\n--- Inner Elaboration Phase, active level %d",
					thisAgent->active_level);
			if (thisAgent->active_goal) {
				print_with_symbols(thisAgent, " (%y)", thisAgent->active_goal);
			}
			print(thisAgent, " ---\n");
		}

		thisAgent->newly_created_instantiations = NIL;

		Bool assertionsExist = FALSE;
		production *prod = 0;
		struct token_struct *tok = 0;
		wme *w = 0;
		Bool once = TRUE;
		while (postpone_assertion(thisAgent, &prod, &tok, &w)) {
			assertionsExist = TRUE;

			if (thisAgent->max_chunks_reached) {
				consume_last_postponed_assertion(thisAgent);
				thisAgent->system_halted = TRUE;
				soar_invoke_callbacks(thisAgent, AFTER_HALT_SOAR_CALLBACK, 0);
				return;
			}

			if (prod->type == JUSTIFICATION_PRODUCTION_TYPE) {
				consume_last_postponed_assertion(thisAgent);

				// don't fire justifications
				continue;
			}

			if (shouldCreateInstantiation(thisAgent, prod, tok, w)) {
				once = FALSE;
				consume_last_postponed_assertion(thisAgent);
				create_instantiation(thisAgent, prod, tok, w);
			}
		}

		// New waterfall model: something fired or is pending to fire at this level,
		// so this active level becomes the next change level.
		if (assertionsExist) {
			if (thisAgent->active_level > thisAgent->next_change_level) {
				thisAgent->next_change_level = thisAgent->active_level;
			}
		}

		// New waterfall model: push unfired matches back on to the assertion lists
		restore_postponed_assertions(thisAgent);

		assert_new_preferences(thisAgent, bufdeallo);

		// Update accounting
		thisAgent->inner_e_cycle_count++;

		if (thisAgent->active_goal == NIL) {
			if (thisAgent->sysparams[TRACE_WATERFALL_SYSPARAM]) {
				print(thisAgent,
						" inner elaboration loop doesn't have active goal.\n");
			}
			break;
		}

		if (thisAgent->active_goal->id.lower_goal == NIL) {
			if (thisAgent->sysparams[TRACE_WATERFALL_SYSPARAM]) {
				print(thisAgent, " inner elaboration loop at bottom goal.\n");
			}
			break;
		}

		if (thisAgent->current_phase == APPLY_PHASE) {
			thisAgent->active_goal = highest_active_goal_apply(thisAgent,
					thisAgent->active_goal->id.lower_goal, TRUE);
		} else {
			assert(thisAgent->current_phase == PROPOSE_PHASE);
			thisAgent->active_goal = highest_active_goal_propose(thisAgent,
					thisAgent->active_goal->id.lower_goal, TRUE);
		}

		if (thisAgent->active_goal != NIL) {
			thisAgent->active_level = thisAgent->active_goal->id.level;
		} else {
			if (thisAgent->sysparams[TRACE_WATERFALL_SYSPARAM]) {
				print(thisAgent,
						" inner elaboration loop finished but not at quiescence.\n");
			}
			break;
		}
	} // end inner elaboration loop

	// Deallocate preferences delayed during inner elaboration loop.
	for (pref_buffer_list::iterator iter = bufdeallo.begin();
			iter != bufdeallo.end(); ++iter) {
		preference_remove_ref(thisAgent, *iter);
	}

	// Restore previous active level
	thisAgent->active_level = thisAgent->highest_active_level;
	thisAgent->active_goal = thisAgent->highest_active_goal;
	/* End new waterfall model */

	while (get_next_retraction(thisAgent, &inst))
		retract_instantiation(thisAgent, inst);

	/* REW: begin 08.20.97 */
	/*  In Waterfall, if there are nil goal retractions, then we want to
	 retract them as well, even though they are not associated with any
	 particular goal (because their goal has been deleted). The
	 functionality of this separate routine could have been easily
	 combined in get_next_retraction but I wanted to highlight the
	 distinction between regualr retractions (those that can be
	 mapped onto a goal) and nil goal retractions that require a
	 special data strucutre (because they don't appear on any goal)
	 REW.  */

	if (thisAgent->nil_goal_retractions) {
		while (get_next_nil_goal_retraction(thisAgent, &inst))
			retract_instantiation(thisAgent, inst);
	}
	/* REW: end   08.20.97 */

}

