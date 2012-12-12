#include "emp_mpi.h"
#include "epmem_manager.h"


emp_mpi::emp_mpi()
{
}


void emp_mpi::init(int startWindowSize, bool evenDivFlag)
{
    epmem_manager * ep_man = new epmem_manager(startWindowSize, evenDivFlag);
    
//start processing
    ep_man->initialize(); 
	
}
