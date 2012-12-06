/*************************************************************************
 *
 *  file:  epmem_manager.h
 *
 * =======================================================================
 *  
 *  
 *  @Author James Kirk
 *  
 *  @Notes header file for epmem_manager class
 *         epmem_manager controlls the sending of messages between procs
 *         and the high level handling of those messages
 * =======================================================================
 */

// class for handling passing of episodes from processor to processor

#ifndef EPMEM_MANAGER_H
#define EPMEM_MANAGER_H

class epmem_manager;

#include "../episodic_memory.h"
#include "epmem_worker.h"

class epmem_manager {
public:
    epmem_manager();
    void initialize();//epmem_param_container* epmem_params);
    //void add_epmem_episode_diff(epmem_episode_diff* episode, agent* my_agent);
    //void finalize();
    //void respond_to_cmd(agent *my_agent);
   
    
private:
    int numProc;
    int id;
    int windowSize;
    int currentSize;
    epmem_worker *epmem_worker_p;
    
    void update_windowSize(int newSize);
    int calc_ep_size(epmem_episode_diff* episode);
    void manager_message_handler();
    void epmem_worker_message_handler();
    void pass_episode(epmem_episode_diff *episode); 
    void received_episode(epmem_episode_diff *episode);
    
};

// Definitions for constant values
#define WINDOW_SIZE_GROWTH_RATE 0
#define DEFAULT_WINDOW_SIZE 30
#define MAX_EPMEM_MSG_SIZE 1000 //todo what size should this be?

//may possibly have first worker be manager as well
#define AGENT_ID 0
#define MANAGER_ID 1
#define FIRST_WORKER_ID 2


//possible message types
typedef enum
{
    NEW_EP = 0,
    RESIZE_WINDOW,
    RESIZE_REQUEST,
    SEARCH,
    SEARCH_RESULT,
    TERMINATE_SEARCH
} EPMEM_MSG_TYPE;

//message structure
struct epmem_msg
{
    EPMEM_MSG_TYPE type;
    int size;
    int source;
    char * data;
} __attribute__((packed));



typedef struct query_data
{
    bool graph_match;
    epmem_param_container::gm_ordering_choices gm_order;
    epmem_time_id before_time;
    double balance;
    Symbol pos_query;
    Symbol neg_query;
    epmem_time_id before;
    epmem_time_id after;
    //TODO serialize
    //epmem_time_list  prohibits;
    //epmem_symbol_set  currents;
    //soar_module::wme_set  cue_wmes;
} __attribute__((packed));


typedef struct query_rsp_data
{
    epmem_time_id best_episode;
    Symbol pos_query;
    Symbol neg_query;
    int leaf_literals_size;
    double best_score;
    bool best_graph_matched;
    long int best_cardinality;
    double perfect_score;
    bool do_graph_match;
    //TODO serialize
    epmem_literal_node_pair_map best_bindings;
    soar_module::symbol_triple_list meta_wmes;
    soar_module::symbol_triple_list retrieval_wmes;
} __attribute__((packed));

#endif
