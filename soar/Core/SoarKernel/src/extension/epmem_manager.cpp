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

//#define DEBUG(a) printf("PROC %d:: %s. File: %s, Line: %d\n\n", id, a, __FILE__,__LINE__) 
#define DEBUG(a) ;

#define INCR_AMNT 2

#include <list>

#include "epmem_manager.h"
#include "epmem_query.h"


/***************************************************************************
 * @Function : epmem_manager
 * @Author   : 
 * @Notes    : Construct intializes currentSize to 0, create epmem_worker
 **************************************************************************/
epmem_manager::epmem_manager(int startWindowSize, EPMEM_DIV_TYPE divType,
	double tuningParam)
{
    currentSize = 0;
	totalEpCnt = 0;
	windowSize = startWindowSize;
	divisionType = divType;
	tuning = tuningParam;
	//evenDiv = evenDivFlag;
	msgCount = 1;
	sendEpNextTime = false;
    epmem_worker_p = new epmem_worker();
}


/***************************************************************************
 * @Function : initialize
 * @Author   : 
 * @Notes    : initialize id, numProcs, windowSize, and worker
 *             call appropriate handler functions for processor
 **************************************************************************/
void epmem_manager::initialize()
{    
    numProc = MPI::COMM_WORLD.Get_size();
    id = MPI::COMM_WORLD.Get_rank();
      
	if(numProc == 3){
		divisionType = EVEN_DIV;
	}
   if(divisionType == EXP_DIV){
	split = .75;
	tuning = 0; 
   } else if(divisionType == EVEN_DIV){
	split = 1.0/(numProc-2);
	tuning = 0;
   } else {
        split = 1.0/(numProc-2);
   } 
    
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
		//windowSize = DEFAULT_WINDOW_SIZE + (id-2)*WINDOW_SIZE_GROWTH_RATE;
		if (id < FIRST_WORKER_ID)
		{
			ERROR("Invalid id");
			return;
		}
		worker_active = false;
		
		//handle all messages/commands to this worker processor
		worker_msg_handler();
		break;
    }   
}


/***************************************************************************
 * @Function : calc_ep_size
 * @Author   : 
 * @Notes    : calculates sizeof new_episode struct
 **************************************************************************/
int epmem_manager::calc_ep_size(epmem_episode_diff* episode)
{
    return episode->buffer_size;
}



/***************************************************************************
 * @Function : pass_episode
 * @Author   : 
 * @Notes    : Send popped episode to next processor -uses blocking send
 **************************************************************************/
void epmem_manager::pass_episode(epmem_episode_diff *episode)
{
    int ep_size = calc_ep_size(episode);
    int size =  ep_size + sizeof(epmem_msg);
    epmem_msg *msg = (epmem_msg*)malloc(size);
    msg->source = id;
    msg->size = size;
    msg->type = NEW_EP;
    memcpy(&(msg->data), episode->buffer, ep_size);
    MPI::COMM_WORLD.Send(msg, size, MPI::CHAR, id+1, 2); //episodes sent on 2
    
    delete msg;
    return;
}


/***************************************************************************
 * @Function : received_episode
 * @Author   : 
 * @Notes    : Received message with new ep, add to worker, and shift eps
 *             if needed
 **************************************************************************/
void epmem_manager::received_episode(epmem_episode_diff *episode)
{
    epmem_worker_p->add_epmem_episode_diff(episode);
    currentSize++;
    
    // if last processor keep hold of all episodes regardless of windowsize    
    if ((currentSize >= windowSize) && (id < (numProc - 1)))
	{
		sendEpNextTime = true;
    }
	
    return;
}


/***************************************************************************
 * @Function : update_windowSize
 * @Author   : 
 * @Notes    : updates current value of windowSize by given
 **************************************************************************/
void epmem_manager::update_windowSize(int sizeIncr)
{
    windowSize += sizeIncr;
}


/***************************************************************************
 * @Function : manager_message_handler
 * @Author   : 
 * @Notes    : agent should message this processor(central manager)
 **************************************************************************/
void epmem_manager::manager_message_handler()
{
    //epmem_msg *msg = (epmem_msg*)malloc(MAX_EPMEM_MSG_SIZE);
	char* cmsg = (char*)malloc(MAX_EPMEM_MSG_SIZE);
    MPI::Status status;
    int buffSize;
    int src;
    bool keep_running = true;
    
    //loop for central thread
    while(keep_running)
    {
		//blocking probe call (unknown message size)
		MPI::COMM_WORLD.Probe(MPI::ANY_SOURCE, 1, status);
        DEBUG("RECEIVED");
		
        //get source and size of incoming message
		buffSize = status.Get_count(MPI::CHAR);
	
		src = status.Get_source();
		MPI::COMM_WORLD.Recv(cmsg, buffSize, MPI::CHAR, src, 1, status);
		epmem_msg *msg = (epmem_msg*) cmsg;
		
		//make sure data in message received agrees
		if (msg->source != src || msg->size != buffSize)
		{
			printf("msg size %d, buffSize %d, src %d, msgsrc %d\n", msg->size,
				   buffSize, src, msg->source); 
			ERROR("Message source/size information conflicts");
			continue;
		}
				
		//switch on message type can be :
		//      new_ep: need to add new episode
		//      resize_request: need to adjust window size
		//      search:  handle epmem search command
		//      search_result:  get results from workers processors
		
		switch(msg->type)
		{
		case EXIT:
		{
			DEBUG("Exiting Program");
			keep_running = false;
			for(int i = id+1; i < numProc; i++){
				MPI::COMM_WORLD.Send(msg, buffSize, MPI::CHAR, i, 1);	
			} 
			break;
		}
		case NEW_EP:
		{
			DEBUG("Received new episode");
			
			// first broadcast so that all episodes know there is a new ep
			msg->type = NEW_EP_NOTIFY;
			msg->size = sizeof(epmem_msg);
			for (int i = id+1; i < numProc; i++)
				MPI::COMM_WORLD.Send(msg, msg->size, MPI::CHAR, i, 1);

			msg->size = buffSize;
			msg->type = NEW_EP;
			MPI::COMM_WORLD.Send(msg, buffSize, MPI::CHAR, id+1, 1);
			
			//wait for receivie acknowledge from last process to add episode
			//MPI::COMM_WORLD.Recv(cmsg, sizeof(epmem_msg), MPI::CHAR, 
			//					 MPI::ANY_SOURCE, 2);		
			MPI::COMM_WORLD.Barrier();
			break;
		}
		case INIT_WORKER:
		{
			DEBUG("Received worker initialization");
			for (int i = id+1; i < numProc; i++)
				MPI::COMM_WORLD.Send(msg, buffSize, MPI::CHAR, i, 1);
			break;
		}
		case RESIZE_REQUEST:
		{
			//TODO
			
			break;
		}
		case SEARCH:
		{
			// Broadcast search request to all workers			
			msg->count = msgCount;
			DEBUG("Received search request");
			//MPI::COMM_WORLD.Bcast(msg, buffSize, MPI::CHAR, id);
			
			for (int i = id+1; i < numProc; i++)
				MPI::COMM_WORLD.Send(msg, buffSize, MPI::CHAR, i, 1);
			break;
		}
		case SEARCH_RESULT:
		{
			if (msg->count != msgCount) //dont process old results
				break;
						
			DEBUG("Received search result");
			//first search results, set as temporary best
			int best_size = buffSize;
			epmem_msg *best_msg = (epmem_msg*) malloc(MAX_EPMEM_MSG_SIZE);
			memcpy(best_msg, msg, best_size);
			query_rsp_data *best_rsp = new query_rsp_data();
			best_rsp->unpack(best_msg);
			bool best_found = false;
			int received = 1;
			query_rsp_data *rsp;
			bool do_graph_match = true; //default always true
									
			//if graph match found by first worker, this is best
			if (best_msg->source == 2 && best_rsp->best_graph_matched && 
				do_graph_match)
			{
				best_found = true;
			}
			std::list<int> notrec;
			for (int i = FIRST_WORKER_ID; i < numProc; i++)
				notrec.push_back(i);
			notrec.remove(best_msg->source);
			
			while(!best_found && (received < (numProc-2)))
			{
				//blocking probe call 
				MPI::COMM_WORLD.Probe(MPI::ANY_SOURCE, 1, status);
				buffSize = status.Get_count(MPI::CHAR);
				src = status.Get_source();
				MPI::COMM_WORLD.Recv(msg, buffSize, MPI::CHAR, src, 1, status);
								
				if ((msg->type != SEARCH_RESULT) || (msg->count != msgCount))
					continue;
				
				DEBUG("Received search result");
				received++;
				
				rsp = new query_rsp_data();
				rsp->unpack(msg);
				
				notrec.remove(msg->source);
				
				//response is best if either 
				//  first ep to have full graph match and no earlier eps left
				//  earliest ep to have full graph match and no earlier eps left
				
				if (rsp->best_graph_matched && 
					(!(best_rsp->best_graph_matched) ||
					 (best_rsp->best_graph_matched && 
					  msg->source < best_msg->source)) && 
					do_graph_match)
				{
					best_found = true;
					std::list<int>::const_iterator it;
					for(it=notrec.begin(); it!=notrec.end(); it++)
					{
						if (*it < msg->source)
						{
							best_found = false;
							break;
						}
					}
				}
				
				if (best_found)
				{
					best_size = buffSize;
					memcpy(best_msg, msg, best_size);
					free(best_rsp);
					best_rsp = rsp;
					break;
				}

				
				if (best_rsp->best_graph_matched)
				{
					best_found = true;
					std::list<int>::const_iterator it;
					for(it=notrec.begin(); it!=notrec.end(); it++)
					{
						if (*it < best_msg->source)
						{
							best_found = false;
							break;
						}
					}
					if (best_found)
					{
						free(rsp);
						break;
					}
				}
								
                // result is better if:
				//     first to find episode
				//     score is better
				//     score is same but lower id
				//     graph match on lower id
				//     first graph match

				
				if (rsp->best_episode == EPMEM_MEMID_NONE &&
					best_rsp->best_episode != EPMEM_MEMID_NONE)
				{
					free(rsp); //cant be best
					continue;
				}
				
				if ((rsp->best_episode != EPMEM_MEMID_NONE &&
					 best_rsp->best_episode == EPMEM_MEMID_NONE) ||
					
					(rsp->best_score > best_rsp->best_score) ||
					
					(rsp->best_score == best_rsp->best_score &&
					 msg->source < best_msg->source && 
					 !best_rsp->best_graph_matched) ||
					
					(do_graph_match && msg->source < best_msg->source
					 && rsp->best_graph_matched) ||
					
					(do_graph_match && 
					 !best_rsp->best_graph_matched &&
					 rsp->best_graph_matched))
				{
					best_size = buffSize;
					memcpy(best_msg, msg, best_size);
					free(best_rsp);
					best_rsp = rsp;
				}
				else
					free(rsp);
				
			}
			//respond with data to agent
			msgCount++;
			DEBUG("Responding with best result");
			
			MPI::COMM_WORLD.Send(best_msg, best_size, MPI::CHAR, AGENT_ID, 1);
			

			if (divisionType == DYNAMIC_DIV)
			{
				double p = (double)numProc;
				if(best_msg->source == p - 1 || (do_graph_match && !best_rsp->best_graph_matched)){
					split *= (1 - tuning * .5);
					if(split < 1.0/(p-2)){
						split = 1.0/(p - 2);
					}
				} else {	
					split += (1 - split) * tuning * (p - best_msg->source - 1)/(p - 3);
				}
					
				//double epLocPercentile = 1.0 - (((double)best_msg->source - 2.0)/((double)(numProc)-3.0));
				//use the location of result to update split value
				
				//split = split*tuning + epLocPercentile*(1-tuning);
				//message new split value to all workers
				// TODO May have deadlock msg issue
				epmem_msg *smsg = (epmem_msg*)malloc(sizeof(epmem_msg) + sizeof(double));
				std::cout << " New split value: " << split << std::endl;
				smsg->source = id;
				smsg->size = sizeof(epmem_msg) + sizeof(double);
				smsg->type = UPDATE_SPLIT;
				memcpy(&(smsg->data), &split, sizeof(double));
				for (int i = id+1; i < numProc; i++)
					MPI::COMM_WORLD.Send(smsg, smsg->size, MPI::CHAR, i, 1);
				
			}
			
			delete best_msg;
			break;
		}
		default:
			ERROR("Message type unknown");
			break;
		}
    }
    
    delete cmsg;
}


/***************************************************************************
 * @Function : worker_msg_handler
 * @Author   : 
 * @Notes    : message handler for epmem_workers on seperate procs
 **************************************************************************/

void epmem_manager::worker_msg_handler()
{
    // malloc msg buffer size initially so dont have to keep remallocing
    // depending on range of sizes may be better to malloc each time
    
	epmem_msg *msg = (epmem_msg*)malloc(MAX_EPMEM_MSG_SIZE);
    MPI::Status status;
    int buffSize;
    int src;
    bool keep_running = true;    
    while(keep_running)
    {
		//blocking probe call (unknown message size)
		MPI::COMM_WORLD.Probe(MPI::ANY_SOURCE, 1, status);
		buffSize = status.Get_count(MPI::CHAR);
		src = status.Get_source();
		MPI::COMM_WORLD.Recv(msg, buffSize, MPI::CHAR, src, 1, status);
		
		DEBUG("RECEIVED");
		//make sure data in message received agrees
		if (msg->size != buffSize)
		{
			printf("msg size %d, buffSize %d, src %d, msgsrc %d\n", msg->size,
				   buffSize, src, msg->source); 
			ERROR("Message source/size information conflicts");
			continue;
		}
		//switch on message type can be :
		//      new_ep: need to add new episode and possibly shift last
		//      resize_window: need to adjust window size
		//      search:  handle epmem search command
		switch(msg->type)
		{
		case EXIT:
		{
			keep_running = false;
			break;
		}
		case NEW_EP:
		{
			if (!worker_active)
				break;
			DEBUG("Received new episode");
			receive_new_episode((int64_t*) &(msg->data), 
								buffSize - sizeof(epmem_msg));
			MPI::COMM_WORLD.Barrier();
			break;
		}
		case NEW_EP_NOTIFY:
		{
			if (!worker_active)
				break;
			if (sendEpNextTime)
			{
				DEBUG("Sending oldest episode to neighbor");
				epmem_episode_diff *to_send = epmem_worker_p->remove_oldest_episode();
				currentSize--;
				pass_episode(to_send);
				delete to_send;	
				sendEpNextTime = false;
			}
			else if (id < (numProc -1))
			{
				//send null episode
				epmem_msg *msgt = new epmem_msg();
				msgt->type = NEW_EP_EMPTY;
				msgt->size = sizeof(epmem_msg);
				MPI::COMM_WORLD.Send(msgt,msgt->size, MPI::CHAR, id+1, 2);
				delete msgt;
			}
			
			if (id == FIRST_WORKER_ID)//will get episode from manager
				break;
			// receive episode (tag 2) here to prevent getting query before
			// episode received
			MPI::COMM_WORLD.Probe(MPI::ANY_SOURCE, 2, status);
			//get source and size of incoming message
			buffSize = status.Get_count(MPI::CHAR);
			src = status.Get_source();
			MPI::COMM_WORLD.Recv(msg, buffSize, MPI::CHAR, src, 2, status);
			if (msg->type == NEW_EP_EMPTY)
			{
				MPI::COMM_WORLD.Barrier();
				break;
			}
			receive_new_episode((int64_t*) &(msg->data), 
								buffSize - sizeof(epmem_msg));
			MPI::COMM_WORLD.Barrier();
		}
		case INIT_WORKER:
		{
			DEBUG("Received initialization");
			epmem_worker_init_data* data = (epmem_worker_init_data*) &(msg->data);
			if (worker_active)//already initialized
				break;
			epmem_worker_p->initialize(data->page_size, data->optimization, 
									   &(data->str), true);
			worker_active = true;
			break;
		}
		case UPDATE_SPLIT:
		{
			memcpy(&split, &(msg->data), sizeof(double)); 
			break;
		}
		case RESIZE_WINDOW:
		{
			//int * newsize = (int*) (&msg->data);
			update_windowSize(INCR_AMNT);//*newsize);
			//std::cout << id << " new window size " << windowSize << std::endl;
			break;
		}

		case SEARCH:
		{
			if (!worker_active)
				break;
			
			DEBUG("Received search request");
			msgCount = msg->count;
			epmem_query* query = new epmem_query();
			query->unpack(msg);
			
			//get response
			query_rsp_data* rsp = epmem_worker_p->epmem_perform_query(query);
			delete query;
			
			epmem_msg* rspMsg = rsp->pack();
			rspMsg->source = id;
			rspMsg->count = msgCount;
			DEBUG("Responding with search result");
			MPI::COMM_WORLD.Send(rspMsg, rspMsg->size, MPI::CHAR, MANAGER_ID, 1);	
			//if (divisionType == DYNAMIC_DIV)
			{
				std::cout << id << " Window size " << windowSize << std::endl;
			}
			delete rspMsg;
			delete rsp;
			
			break;
		}
		
		default:
			ERROR("Message type unknown");
			break;
		}
	}
	
	delete msg;	
	
}

void epmem_manager::receive_new_episode(int64_t *ep_buffer, int dataSize)
{
	epmem_episode_diff *episode = new epmem_episode_diff(ep_buffer, dataSize);
	if (dataSize != calc_ep_size(episode))
	{
		printf("datasize %d episode size %d\n", 
			   dataSize, calc_ep_size(episode));
		ERROR("Msg data size for new episode incorrect");
		return;
	}
	// determine if this episode addition also needs a window size increase
	///qqqqqq
	if(id == numProc - 1){
		windowSize++;
	} else {
		double ratio = (1-split)/(static_cast<double>(numProc) - 3);
		double mod = (ratio * ((double)numProc - id - 1) + split) / ratio;
		totalEpCnt++;
	
		if (totalEpCnt > mod)
		{
			windowSize++;
			totalEpCnt -=mod;
		}
	}		
	//handle adding episode and shift last episode if needed
	received_episode(episode);
	delete episode;
}









