/*************************************************************************
 *
 *  file:  epmem_manager.cpp
 *
 * =======================================================================
 *  
 *  @Author James Kirk
 *  
 *  @Notes file for epmem_manager class
 *         epmem_manager controlls the sending of messages between procs
 *         and the high level handling of those messages
 * 
 * =======================================================================
 */


// error print macro
#define ERROR(a) printf("Error Occured: %s. Proc Id: %d, File: %s, Line: %d\n", a, id, __FILE__,__LINE__) 


#include "epmem_manager.h"



/***************************************************************************
 * @Function : epmem_manager
 * @Author   : 
 * @Notes    : Construct intializes currentSize to 0, create epmem_worker
 **************************************************************************/
epmem_manager::epmem_manager()
{
    currentSize = 0;
    epmem_worker_p = new epmem_worker();
}


/***************************************************************************
 * @Function : initialize
 * @Author   : 
 * @Notes    : initialize id, numProcs, windowSize, and worker
 *             call appropriate handler functions for processor
 **************************************************************************/
void epmem_manager::initialize()//epmem_param_container* epmem_params)
{
    //this should be initialized early by agent
    //MPI::Init(argc, argv); //move
    
    numProc = MPI::COMM_WORLD.Get_size();
    id = MPI::COMM_WORLD.Get_rank();
    
        
    windowSize = DEFAULT_WINDOW_SIZE + id*WINDOW_SIZE_GROWTH_RATE;
    
    // launch appropraite handler function for processor/role
    switch(id)
    {
    case AGENT_ID:
    {
	ERROR("Agent is trying to run epmem_manager on its processor");
	return;
    }
    case MANAGER_ID:
    {
	// message handling loop for central manager
	manager_message_handler();
	break;
    }
    default:
	if (id < FIRST_WORKER_ID)
	{
	    ERROR("Invalid id");
	    return;
	}
	// initalize epmem_worker
	// JK COMMENT OUT FOR NOW
	//epmem_worker_p->initialize(epmem_params); 
	//handle all messages/commands to this worker processor
	epmem_worker_message_handler();
	break;
    }
    
}


/***************************************************************************
 * @Function : calc_ep_size
 * @Author   : 
 * @Notes    : calculates sizeof new_episode struct
 **************************************************************************/
int epmem_manager::calc_ep_size(new_episode* episode)
{
    int size = 0;
    size += sizeof(long);
    size+= sizeof(epmem_node_unique)*episode->num_added_nodes;
    size+= sizeof(epmem_node_unique)*episode->num_removed_nodes;
    size+= sizeof(epmem_edge_unique)*episode->num_added_edges;
    size+= sizeof(epmem_edge_unique)*episode->num_removed_edges;
    return size;
}



/***************************************************************************
 * @Function : pass_episode
 * @Author   : 
 * @Notes    : Send popped episode to next processor -uses blocking send
 **************************************************************************/
void epmem_manager::pass_episode(new_episode *episode)
{
    int ep_size = calc_ep_size(episode);
    int size =  ep_size + sizeof(int)*2 + sizeof(EPMEM_MSG_TYPE);
    epmem_msg *msg = (epmem_msg*)malloc(size);
    msg->source = id;
    msg->size = size;
    msg->type = new_ep;
    memcpy(msg->data, episode, ep_size);
    MPI::COMM_WORLD.Send(msg, size, MPI::CHAR, id+1, 1);
    
    delete msg;
    return;
}


/***************************************************************************
 * @Function : received_episode
 * @Author   : 
 * @Notes    : Received message with new ep, add to worker, and shift eps
 *             if needed
 **************************************************************************/
void epmem_manager::received_episode(new_episode *episode)
{
    epmem_worker_p->add_new_episode(episode);
    currentSize++;
    
    // if last processor keep hold of all episodes regardless of windowsize
    if (id >= (numProc - 1))
    {
	//TODO msg master or other node to consider resizing previous windows
	return;
    }
    
    if (currentSize >= windowSize)
    {
	new_episode *to_send = epmem_worker_p->remove_oldest_episode();
	currentSize--;
	pass_episode(to_send);
    }
    return;
}


/***************************************************************************
 * @Function : update_windowSize
 * @Author   : 
 * @Notes    : updates current value of windowSize to given
 **************************************************************************/
void epmem_manager::update_windowSize(int newSize)
{
    windowSize = newSize;
}


/***************************************************************************
 * @Function : finalize
 * @Author   : 
 * @Notes    : 
 **************************************************************************/
/*
void epmem_manager::finalize()
{
    //TODO this will probably not be here
    MPI::Finalize();
}
*/


/***************************************************************************
 * @Function : respond_to_cmd
 * @Author   : 
 * @Notes    : split search among procs
 **************************************************************************/
 /*
void epmem_manager::respond_to_cmd(agent *my_agent)
{
    //epmem_respond_to_cmd(my_agent); 
}
 */

/***************************************************************************
 * @Function : manager_message_handler
 * @Author   : 
 * @Notes    : agent should message this processor(central manager)
 **************************************************************************/
void epmem_manager::manager_message_handler()
{
    epmem_msg *msg = (epmem_msg*)malloc(MAX_EPMEM_MSG_SIZE);
    MPI::Status status;
    int buffSize;
    int src;
    
    //loop for central thread
    while(1)
    {
	//blocking probe call (unknown message size)
	MPI::COMM_WORLD.Probe(MPI::ANY_SOURCE, 1, status);
	
	//get source and size of incoming message
	buffSize = status.Get_count(MPI::CHAR);
	src = status.Get_source();
	MPI::COMM_WORLD.Recv(msg, buffSize, MPI::CHAR, src, 1, status);
	
	//make sure data in message received agrees
	if (msg->source != src || msg->size != buffSize)
	{
	    ERROR("Message source/size information conflicts");
	    continue;
	}
	
	int dataSize = buffSize - sizeof(int)*2 -sizeof(EPMEM_MSG_TYPE);
	
	//switch on message type can be :
	//      new_ep: need to add new episode
	//      resize_request: need to adjust window size
	//      search:  handle epmem search command
	//      search_result:  get results from workers processors
	
	switch(msg->type)
	{
	case new_ep:
	{
	    new_episode *episode = (new_episode*)msg->data;
	    if (dataSize != calc_ep_size(episode))
	    {
		ERROR("Msg data size for new episode incorrect");
	    	break;
	    }
	    //send new episode to first worker processor
	    pass_episode(episode);
	    
	    break;
	}
	case resize_request:
	{
	    //TODO
	    
	    break;
	}
	case search:
	{
	    // Broadcast search request to all workers
	    MPI::COMM_WORLD.Bcast(msg, buffSize, MPI::CHAR, id);
	    
	    break;
	}
	case search_result:
	{
	    // TODO
	    break;
	}
	
	default:
	    ERROR("Message type unknown");
	    break;
	}
    }
    
    delete msg;
}


/***************************************************************************
 * @Function : epmem_worker_message_handler
 * @Author   : 
 * @Notes    : message handler for epmem_workers on seperate procs
 **************************************************************************/
void epmem_manager::epmem_worker_message_handler()
{
    // malloc msg buffer size initially so dont have to keep remallocing
    // depending on range of sizes may be better to malloc each time
    epmem_msg *msg = (epmem_msg*)malloc(MAX_EPMEM_MSG_SIZE);
    MPI::Status status;
    int buffSize;
    int src;
    
    while(1)
    {
	//blocking probe call (unknown message size)
	MPI::COMM_WORLD.Probe(MPI::ANY_SOURCE, 1, status);
	
	//get source and size of incoming message
	buffSize = status.Get_count(MPI::CHAR);
	src = status.Get_source();
	MPI::COMM_WORLD.Recv(msg, buffSize, MPI::CHAR, src, 1, status);
	
	//make sure data in message received agrees
	if (msg->source != src || msg->size != buffSize)
	{
	    ERROR("Message source/size information conflicts");
	    continue;
	}
	
	int dataSize = buffSize - sizeof(int)*2 -sizeof(EPMEM_MSG_TYPE);
	
	//switch on message type can be :
	//      new_ep: need to add new episode and possibly shift last
	//      resize_window: need to adjust window size
	//      search:  handle epmem search command
	switch(msg->type)
	{
	case new_ep:
	{
	    new_episode *episode = (new_episode*)msg->data;
	    if (dataSize != calc_ep_size(episode))
	    {
		ERROR("Msg data size for new episode incorrect");
	    	break;
	    }

	    //handle adding episode and shift last episode if needed
	    received_episode(episode);
	    
	    break;
	}
	case resize_window:
	{
	    if (dataSize != sizeof(int))
	    {
		ERROR("Msg data size for resize window incorrect");
	    	break;
	    }
	    int * newsize = (int*) msg->data;
	    
	    if (*newsize < 1)
	    {
		ERROR("Illegal window size");
	    	break;
	    }
	    update_windowSize(*newsize);
	    
	    break;
	}
	case search:
	{
	    //TODO call search routine on epmem_worker
	    break;
	}
	default:
	    ERROR("Message type unknown");
	    break;
	}
    }
    
    delete msg;	
}









