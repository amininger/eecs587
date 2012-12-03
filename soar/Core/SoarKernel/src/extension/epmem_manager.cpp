/*************************************************************************
 *
 *  file:  epmem_manager.cpp
 *
 * =======================================================================
 *  
 *  
 *  
 *  
 *  
 *  
 * =======================================================================
 */

#include "epmem_manager.h"
#include "mpi.h"

#define FIRST_NODE 1

epmem_manager::epmem_manager()
{
    currentSize = 0;
}
void epmem_manager::initialize(int argc, char *argv[], 
		epmem_worker *epmem_worker, int size) {
    MPI::Init(argc, argv);
    
    numProc = MPI::COMM_WORLD.Get_size();
    id = MPI::COMM_WORLD.Get_rank();
    epmem_worker_p = epmem_worker; //initalize here instead??
    windowSize = size;
}


//where to add new episode

int epmem_manager::calc_ep_size(new_episode* episode)
{
    
}

void epmem_manager::add_new_episode(new_episode* episode, agent *my_agent)
{
    if (id == FIRST_NODE)
    {
	epmem_worker_p->add_new_episode(episode, my_agent);
	currentSize++;
	if (currentSize >= windowSize)
	{
	    new_episode *to_send = epmem_worker_p->remove_oldest_episode();
	    currentSize--;
	    pass_episode(to_send);
	}
	else
	{
	    //send blank episode to signify no episode passing necessary
	    new_episode ep;
	    ep.time = 0xDEAD;
	    pass_episode(&ep);
	}
    }
    else
    {
	//neighbor auto tries to recieve
	recieve_episode(my_agent);
    }
}

void epmem_manager::pass_episode(new_episode* episode)
{
    MPI::COMM_WORLD.Isend(episode, sizeof(new_episode), MPI::CHAR, id+1, 1);
}


void epmem_manager::recieve_episode(agent *my_agent)
{
    MPI::Status status;
    new_episode episode;
    
    MPI::COMM_WORLD.Recv(&episode, sizeof(episode), MPI::CHAR, id-1, 
			 1, status);
    
    if (episode.time == 0xDEAD)
    {
	//no updates needed, pass empty episode
	pass_episode(&episode);
	return;
    }
    epmem_worker_p->add_new_episode(&episode, my_agent);
    
    currentSize++;
    
    // if last processor keep hold of all episodes regardless of windowsize
    if (id >= (numProc - 1))
	return;
    
    if (currentSize >= windowSize)
    {
	new_episode *to_send = epmem_worker_p->remove_oldest_episode();
	currentSize--;
	pass_episode(to_send);
    }
    
}

void epmem_manager::update_windowSize(int newSize)
{
    windowSize = newSize;
}

void epmem_manager::finalize()
{
    MPI::Finalize();
}

// split search among processors
void epmem_manager::respond_to_cmd(agent *my_agent)
{
    epmem_respond_to_cmd(my_agent); //todo replace
}


void









