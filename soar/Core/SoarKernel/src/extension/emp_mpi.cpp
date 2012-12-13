#include "emp_mpi.h"
#include "epmem_manager.h"


emp_mpi::emp_mpi()
{
}


void emp_mpi::init(int startWindowSize, int divType, double tuning)
{
    epmem_manager * ep_man = new epmem_manager(startWindowSize, 
											   (EPMEM_DIV_TYPE)divType,tuning);
    
//start processing
    ep_man->initialize(); 
	
}
