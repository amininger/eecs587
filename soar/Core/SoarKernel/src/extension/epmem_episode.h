

#ifndef EPMEM_EPISODE_H_
#define EPMEM_EPISODE_H_

#include "../episodic_memory.h"

struct epmem_episode{
	epmem_episode(epmem_time_id time, int num_nodes, int num_edges)
		:num_nodes(num_nodes), num_edges(num_edges)
	{
		buffer_size = 3 + num_nodes + num_edges;
		buffer = new epmem_node_id[buffer_size];

		epmem_node_id* addr = buffer;

		*addr = time;
		addr += 1;

		*addr = num_nodes;
		addr += 1;
		nodes = addr;
		addr += num_nodes;

		*addr = num_edges;
		addr += 1;
		edges = addr;
		addr += num_edges;
	}
	
	epmem_episode(int64_t* buffer):buffer(buffer)
	{
		int64_t* addr = buffer;

		time = *addr;
		addr += 1;

		num_nodes = *addr;
		addr += 1;
		nodes = addr;
		addr += num_nodes;
		
		num_edges = *addr;
		addr += 1;
		edges = addr;
		addr += num_edges;

		buffer_size = 3 + num_nodes + num_edges;
	}

	~epmem_episode(){
		delete [] buffer;
	}

	epmem_time_id time;

	int num_nodes;
	epmem_node_id* nodes;

	int num_edges;
	epmem_node_id* edges;

	epmem_node_id* buffer;
	int buffer_size;
};


#endif //EPMEM_EPISODE_H_