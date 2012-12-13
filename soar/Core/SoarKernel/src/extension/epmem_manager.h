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


typedef enum
{
	EXP_DIV = 0,
	EVEN_DIV,
	DYNAMIC_DIV
} EPMEM_DIV_TYPE;


class epmem_manager {
public:
    epmem_manager(int startWindowSize, EPMEM_DIV_TYPE divType,
		double tuningParam);
    void initialize();
     
private:
    
	int numProc;
	int msgCount;
    int id;
	
	double totalEpCnt;
    int windowSize;
    int currentSize;
	bool sendEpNextTime;
	
	double split;
	int divisionType;
	double tuning;
	
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
//#define DEFAULT_WINDOW_SIZE 4
#define MAX_EPMEM_MSG_SIZE 100000 //todo what size should this be?

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
	UPDATE_SPLIT,
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


#endif
