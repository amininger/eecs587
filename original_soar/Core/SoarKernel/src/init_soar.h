/*************************************************************************
 * PLEASE SEE THE FILE "license.txt" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION. 
 *************************************************************************/

/* ========================================================================
                              init_soar.h
======================================================================== */

#ifndef INIT_SOAR_H
#define INIT_SOAR_H

#ifdef __cplusplus
//extern "C"
//{
#endif

typedef char Bool;
typedef signed short goal_stack_level;
typedef union symbol_union Symbol;
typedef struct agent_struct agent;

/* added this prototype -ajc (5/3/02) */
extern void set_sysparam (agent* thisAgent, int param_number, int64_t new_value);

extern void reset_statistics (agent* thisAgent);
extern void setup_signal_handling (void);
//extern void load_init_file (Kernel* thisKernel, agent* thisAgent);

/* --- signal handler that gets invoked on SIGINT --- */
// (deprecated)
//extern void control_c_handler (int the_signal); 

/* Main pgm stuff -- moved here from soarkernel.h -ajc (5/7/02) */

//extern int  terminate_soar (void);

/* ---------------------------------------------------------------------
                            Exiting Soar

   Exit_soar() and abort_with_fatal_error() both terminate Soar, closing
   the log file before exiting.  Abort_with_fatal_error() also prints
   an error message before exiting.  Just_before_exit_soar() calls the
   Soar cleanup functions but does not actually exit.  This is useful
   for interfaces that do their own exiting.
--------------------------------------------------------------------- */

//extern void exit_soar (agent* thisAgent);
extern void abort_with_fatal_error (agent* thisAgent, const char *);
//extern void just_before_exit_soar (agent* thisAgent);

/* ---------------------------------------------------------------------
                     Adding and Removing Pwatchs

   Productions_being_traced is a (consed) list of all productions
   on which a pwatch has been set.  Pwatchs are added/removed via
   calls to add_pwatch() and remove_pwatch().
--------------------------------------------------------------------- */

extern void add_pwatch (agent* thisAgent, struct production_struct *prod);
extern void remove_pwatch (agent* thisAgent, struct production_struct *prod);

/* ---------------------------------------------------------------------
                         Reinitializing Soar

   Reinitialize_soar() does all the work for an init-soar.
--------------------------------------------------------------------- */

extern bool reinitialize_soar (agent* thisAgent);

/* ---------------------------------------------------------------------
                         Reset Timers
   This code was duplicated in three spots. It is now consolidated.
   This stops and sets all timers to zero.
--------------------------------------------------------------------- */
extern void reset_timers (agent* thisAgent);

/* ---------------------------------------------------------------------
                         Reset Timers
   This code used to be in the CLI, it resets the per-cycle max stats.
--------------------------------------------------------------------- */
extern void reset_max_stats (agent* thisAgent);

/* ---------------------------------------------------------------------
                         Reinitializing Soar

   This adds the top state (S1) and the io header symbols and wme's to the
   agent's working memory. (This is a modification for the gSKI project).
--------------------------------------------------------------------- */

extern void init_agent_memory (agent* thisAgent);

/* ---------------------------------------------------------------------
                            Running Soar

   Each of the following routines runs Soar for a certain duration,
   or until stop_soar gets set to TRUE.
     - Run_forever() runs Soar forever.
     - Run_for_n_phases() runs Soar for a given number (n) of top-level
       phases.  (If n==-1, it runs forever.)
     - Run_for_n_elaboration_cycles() runs Soar for a given number (n)
       of elaboration cycles.  (Here, decision phase is counted as
       an elaboration cycle.)  (If n==-1, it runs forever.)
     - Run_for_n_decision_cycles() runs Soar for a given number (n) of
       decision cycles.  (If n==-1, it runs forever.)
     - Run_for_n_modifications_of_output runs soar by decision cycle
       n times, where n starts at 0 and is incremented anytime the
       output link is modified by the agent.  n is not incremented when
       the output-link is created nor when the output-link is modified
       during the Input Cycle, ie when getting feedback from a simulator.
     - Run_for_n_selections_of_slot (agent*, int64_t n, Symbol *attr_of_slot): this
       runs Soar until the nth time a selection is made for a given
       type of slot.  Attr_of_slot should be either state_symbol or 
       operator_symbol.
     - Run_for_n_selections_of_slot_at_level (agent*, int64_t n, Symbol *attr_of_slot,
       goal_stack_level level):  this runs Soar for n selections of the
       given slot at the given level, or until the goal stack is popped
       so that level no longer exists.
--------------------------------------------------------------------- */

enum go_type_enum { GO_PHASE, GO_ELABORATION, GO_DECISION,
                    GO_STATE, GO_OPERATOR, GO_SLOT, GO_OUTPUT };

extern void run_forever (agent* thisAgent);
extern void run_for_n_phases (agent* thisAgent, int64_t n);
extern void run_for_n_elaboration_cycles (agent* thisAgent, int64_t n);
extern void run_for_n_decision_cycles (agent* thisAgent, int64_t n);
extern void run_for_n_modifications_of_output (agent* thisAgent, int64_t n);
extern void run_for_n_selections_of_slot (agent*, int64_t n, Symbol *attr_of_slot);
extern void run_for_n_selections_of_slot_at_level (agent* thisAgent, int64_t n,
                                                   Symbol *attr_of_slot,
                                                   goal_stack_level level);

extern void do_one_top_level_phase (agent* thisAgent); 

/* removed DETERMINE_LEVEL_PHASE for Soar 8.6 
 *  added PROPOSE and APPLY.   KJC May 2005
 */

enum top_level_phase { INPUT_PHASE = 0, 
		               PROPOSE_PHASE,
                       DECISION_PHASE,
		               APPLY_PHASE,
                       OUTPUT_PHASE, 
                       PREFERENCE_PHASE, 
                       WM_PHASE,
                       NUM_PHASE_TYPES
                     };

/* REW: end   05.05.97 */

#ifdef __cplusplus
//}
#endif

#endif

