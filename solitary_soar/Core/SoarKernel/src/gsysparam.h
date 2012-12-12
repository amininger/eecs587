/*************************************************************************
 * PLEASE SEE THE FILE "license.txt" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION. 
 *************************************************************************/

/* gsysparam.h */

/* ====================================================================
             Global System Parameters and Related Definitions

   A set of system parameters (sysparam's for short) affect many operations
   of Soar, including learning, tracing, deciding, etc.  In order to
   provide a simple, uniform update mechanism (a single callback routine that
   gets called when any parameter changes), we store these parameters in
   an array sysparams[].  Below, we #define various indices into this array
   corresponding to various system parameters.

   Most of the parameters are of type "int64_t".  A few parameters are more
   naturally handled as lists; for these, the array value is just a dummy,
   and callback routines must inspect a global variable to get the real 
   value.  Chunk_free_problem_spaces is an example of this.

   The array of sysparams[] can be read directly, but should be modified
   ONLY via calls to set_sysparam(), which is defined in init-soar.c.
==================================================================== */

#ifndef GSYSPARAM_H
#define GSYSPARAM_H

#ifdef __cplusplus
//extern "C"
//{
#endif

typedef char Bool;
typedef unsigned char byte;
typedef struct agent_struct agent;

/* -------------------------------
      Types of Productions
------------------------------- */

#define USER_PRODUCTION_TYPE 0
#define DEFAULT_PRODUCTION_TYPE 1
#define CHUNK_PRODUCTION_TYPE 2
#define JUSTIFICATION_PRODUCTION_TYPE 3
#define TEMPLATE_PRODUCTION_TYPE 4

#define NUM_PRODUCTION_TYPES 5
// Soar-RL assumes that the production types start at 0 and go to (NUM_PRODUCTION_TYPES-1) sequentially

/* ---------------------------------------
    Match Set print parameters
--------------------------------------- */

#define MS_ASSERT_RETRACT 0      /* print both retractions and assertions */
#define MS_ASSERT         1      /* print just assertions */
#define MS_RETRACT        2      /* print just retractions */

typedef byte ms_trace_type;   /* must be one of the above constants */

/* ---------------------------------------
    How much information to print about
    the wmes matching an instantiation
--------------------------------------- */

#define NONE_WME_TRACE    1      /* don't print anything */
#define TIMETAG_WME_TRACE 2      /* print just timetag */
#define FULL_WME_TRACE    3      /* print whole wme */
#define NO_WME_TRACE_SET  4

typedef byte wme_trace_type;   /* must be one of the above constants */

/* -------------------------------
      Ways to Do User-Select
------------------------------- */

#define USER_SELECT_BOLTZMANN	1		/* boltzmann algorithm, with respect to temperature */
#define USER_SELECT_E_GREEDY	2		/* with probability epsilon choose random, otherwise greedy */
#define USER_SELECT_FIRST  		3		/* just choose the first candidate item */
#define USER_SELECT_LAST   		4		/* choose the last item   AGR 615 */	
#define USER_SELECT_RANDOM 		5		/* pick one at random */
#define USER_SELECT_SOFTMAX		6		/* pick one at random, probabalistically biased by numeric preferences */
#define USER_SELECT_INVALID		7		/* should be 1+ last item, used for validity checking */

/* ---------------------------
   And now, the sysparam's
--------------------------- */

/* ====== Sysparams for what to trace === */

#define TRACE_CONTEXT_DECISIONS_SYSPARAM          1
#define TRACE_PHASES_SYSPARAM                     2

/* --- Warning: these next four MUST be consecutive and in the order of the
   production types defined above --- */
#define TRACE_FIRINGS_OF_USER_PRODS_SYSPARAM      3
#define TRACE_FIRINGS_OF_DEFAULT_PRODS_SYSPARAM   4
#define TRACE_FIRINGS_OF_CHUNKS_SYSPARAM          5
#define TRACE_FIRINGS_OF_JUSTIFICATIONS_SYSPARAM  6
#define TRACE_FIRINGS_OF_TEMPLATES_SYSPARAM		  7

#define TRACE_FIRINGS_WME_TRACE_TYPE_SYSPARAM     8
#define TRACE_FIRINGS_PREFERENCES_SYSPARAM        9
#define TRACE_WM_CHANGES_SYSPARAM                10
#define TRACE_CHUNK_NAMES_SYSPARAM               11
#define TRACE_JUSTIFICATION_NAMES_SYSPARAM       12
#define TRACE_CHUNKS_SYSPARAM                    13
#define TRACE_JUSTIFICATIONS_SYSPARAM            14
#define TRACE_BACKTRACING_SYSPARAM               15
/* ===== watch loading flag =====  KJC 7/96 */
#define TRACE_LOADING_SYSPARAM                   16

/* ====== Max Elaborations === */
#define MAX_ELABORATIONS_SYSPARAM                17

/* ====== Max Chunks === */
#define MAX_CHUNKS_SYSPARAM                      18

#define RESPOND_TO_LOAD_ERRORS_SYSPARAM          19

/* ====== Sysparams for control of learning === */
#define LEARNING_ON_SYSPARAM                     20
#define LEARNING_ONLY_SYSPARAM                   21
#define LEARNING_EXCEPT_SYSPARAM                 22
#define LEARNING_ALL_GOALS_SYSPARAM              23

/* ====== User Select === */
#define USER_SELECT_MODE_SYSPARAM                24

/* ====== Print Warnings === */
#define PRINT_WARNINGS_SYSPARAM                  25

/* AGR 627 begin */
/* ====== Whether to print out aliases as they're defined === */
#define PRINT_ALIAS_SYSPARAM                     26
/* AGR 627 end */

/* ===== explain_flag =====  KJC 7/96 */
#define EXPLAIN_SYSPARAM                         27

/* kjh(B14) */
#define USE_LONG_CHUNK_NAMES                     28

/* REW:  10.22.97 */
#define TRACE_OPERAND2_REMOVALS_SYSPARAM         29

/* RMJ */
#define REAL_TIME_SYSPARAM         				 30

/* RMJ */
#define ATTENTION_LAPSE_ON_SYSPARAM              31

/* KJC 3/01 limit number of cycles in run_til_output */
#define MAX_NIL_OUTPUT_CYCLES_SYSPARAM           32

#define TRACE_INDIFFERENT_SYSPARAM               33

/* rmarinie 11/04 */
#define TIMERS_ENABLED                           34

#define MAX_GOAL_DEPTH							 35

/* KJC 8/06:  generate warning and event if memory usage exceeds this value */
#define MAX_MEMORY_USAGE_SYSPARAM                36

/* NLD: auto-reduction of exploration parameters */
#define USER_SELECT_REDUCE_SYSPARAM				 37

/* NLD: Soar-RL trace information */
#define TRACE_RL_SYSPARAM						 38

/* JRV: Bug 1087: Chunk through local negations */
#define CHUNK_THROUGH_LOCAL_NEGATIONS_SYSPARAM	 39

/* New waterfall model: trace waterfall events */
#define TRACE_WATERFALL_SYSPARAM				 40

/* NLD: WMA trace information */
#define TRACE_WMA_SYSPARAM						 41

/* NLD: EpMem trace information */
#define TRACE_EPMEM_SYSPARAM					 42

/* NLD: SMem trace information */
#define TRACE_SMEM_SYSPARAM						 43

/* JRV: GDS */
#define TRACE_GDS_SYSPARAM						 44

/* JRV: Break on long decision cycle time */
#define DECISION_CYCLE_MAX_USEC_INTERRUPT        45

/* MMA: Chunk over evaluation rules in subgoals */
#define CHUNK_THROUGH_EVALUATION_RULES_SYSPARAM	 46

/* --- Warning: if you add sysparams, be sure to update the next line! --- */
#define HIGHEST_SYSPARAM_NUMBER                  46

/* -----------------------------------------
   Sysparams[] stores the parameters; set_sysparam()
   should be used to modify them.
----------------------------------------- */

extern void init_sysparams (agent* thisAgent);
extern void set_sysparam (agent* thisAgent, int param_number, int64_t new_value);

#define kChunkNamePrefixMaxLength  64  /* kjh (B14) */

#ifdef __cplusplus
//}
#endif

#endif
