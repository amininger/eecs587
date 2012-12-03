/*************************************************************************
 *
 *  file:  epmem_manager.h
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

// class for handling passing of episodes from processor to processor

#ifndef EPMEM_MANAGER_H
#define EPMEM_MANAGER_H

class epmem_manager;

#include "../episodic_memory.h"
#include "epmem_worker.h"

class epmem_manager {
public:
    epmem_manager();
    void initialize(int argc, char *argv[], epmem_worker *epmem_worker, 
		    int size);
    void add_new_episode(new_episode* episode, agent* my_agent);
    void finalize();
    void update_windowSize(int newSize);
    void respond_to_cmd(agent *my_agent);
    
private:
    int numProc;
    int id;
    int windowSize;
    int currentSize;
    epmem_worker *epmem_worker_p;
    
    void pass_episode(new_episode* episode);
    void recieve_episode(agent *my_agent);
};

#endif
