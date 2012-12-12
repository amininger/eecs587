

#ifndef EPMEM_EPISODE_H_
#define EPMEM_EPISODE_H_

#include "../episodic_memory.h"


struct epmem_episode{
	epmem_episode(epmem_time_id time, int num_nodes, int num_edges)
		:num_nodes(num_nodes), num_edges(num_edges)
	{
		buffer_length = 3 + num_nodes + num_edges;
        buffer_size = buffer_length * sizeof(epmem_node_id);
		buffer = new epmem_node_id[buffer_length];

        int index = 0;

		buffer[index] = time;
		index += 1;

		buffer[index] = num_nodes;
		index += 1;
		nodes = &buffer[index];
		index += num_nodes;

		buffer[index] = num_edges;
		index += 1;
		edges = &buffer[index];
		index += num_edges;
	}
	
	epmem_episode(int64_t* buffer):buffer(buffer)
	{
		int index = 0;

		time = buffer[index];
		index += 1;

		num_nodes = buffer[index];
		index += 1;
		nodes = &buffer[index];
		index += num_nodes;
		
		num_edges = buffer[index];
		index += 1;
		edges = &buffer[index];
		index += num_edges;

		buffer_length = 3 + num_nodes + num_edges;
        buffer_size = sizeof(epmem_node_id) * buffer_length;
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
	int buffer_size;        // Size of the buffer in bytes
    int buffer_length;      // Length of the buffer array in epmem_time_id
};


#endif //EPMEM_EPISODE_H_