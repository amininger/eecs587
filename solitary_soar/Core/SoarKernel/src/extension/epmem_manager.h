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
     
private:
    int numProc;
	int msgCount;
    int id;
	int totalEpCnt;
    int windowSize;
    int currentSize;
	bool sendEpNextTime;
    epmem_worker *epmem_worker_p;
	bool worker_active;
    void update_windowSize(int sizeIncr);
	
	void receive_new_episode(int64_t *ep_buffer, int dataSize);
    int calc_ep_size(epmem_episode_diff* episode);
    void manager_message_handler();
    void worker_msg_handler();
    void pass_episode(epmem_episode_diff *episode); 
    void received_episode(epmem_episode_diff *episode);
    
};

// Definitions for constant values
#define WINDOW_SIZE_GROWTH_RATE 0
#define DEFAULT_WINDOW_SIZE 20
#define MAX_EPMEM_MSG_SIZE 1000 //todo what size should this be?

//may possibly have first worker be manager as well
#define AGENT_ID 0
#define MANAGER_ID 1
#define FIRST_WORKER_ID 2


//possible message types
typedef enum
{
    NEW_EP = 0,
	NEW_EP_NOTIFY,
	NEW_EP_EMPTY,
	INIT_WORKER,
    RESIZE_WINDOW,
    RESIZE_REQUEST,
    SEARCH,
    SEARCH_RESULT,
    TERMINATE_SEARCH,
    EXIT
} EPMEM_MSG_TYPE;

//message structure
typedef struct epmem_msg_struct
{
    EPMEM_MSG_TYPE type;
    int size;
    int source;
	int count;
    char data;
} epmem_msg;


typedef struct epmem_worker_init_data_struct
{
	int page_size;
	bool optimization;
	int buffsize;
	char str;
}epmem_worker_init_data;

/*
typedef struct query_data_struct
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
} query_data;
*/
  /*
typedef struct query_rsp_data_struct
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
    //epmem_literal_node_pair_map best_bindings;
    //soar_module::symbol_triple_list meta_wmes;
    //soar_module::symbol_triple_list retrieval_wmes;
} query_rsp_data;
  */

#endif
