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
    //void add_new_episode(new_episode* episode, agent* my_agent);
    //void finalize();
    //void respond_to_cmd(agent *my_agent);
   
    
private:
    int numProc;
    int id;
    int windowSize;
    int currentSize;
    epmem_worker *epmem_worker_p;
    
    void update_windowSize(int newSize);
    int calc_ep_size(new_episode* episode);
    void manager_message_handler();
    void epmem_worker_message_handler();
    void pass_episode(new_episode *episode); 
    void received_episode(new_episode *episode);
    
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
    new_ep = 0,
    resize_window,
    resize_request,
    search,
    search_result,
    terminate
} EPMEM_MSG_TYPE;

//message structure
struct epmem_msg
{
    EPMEM_MSG_TYPE type;
    int size;
    int source;
    char * data;
} __attribute__((packed));

#endif
