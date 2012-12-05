/*
 * new_episode.h
 *
 *  Created on: Nov 28, 2012
 *      Author: aaron
 */

#ifndef NEW_EPISODE_H_
#define NEW_EPISODE_H_

struct new_episode;

#include "../episodic_memory.h"
#include "../agent.h"


struct new_episode{
	new_episode(long time, int num_nodes_add, int num_nodes_remove, int num_edges_add, int num_edges_remove);

	new_episode(long* buffer):buffer(buffer);

	~new_episode(){
		delete [] buffer;
	}

	long time;

	epmem_node_unique* added_nodes;
	int num_added_nodes;

	epmem_node_unique* removed_nodes;
	int num_removed_nodes;

	epmem_edge_unique* added_edges;
	int num_added_edges;

	epmem_edge_unique* removed_edges;
	int num_removed_edges;

	int buffer_size;
	long* buffer;
	/* Buffer Structure (array of longs)
		[time]							: 1 long consisting of the time of the episode
		[num_added_nodes]		: 1 long identifying the number of added nodes (size of next section)
		[added_nodes]				: array of epmem_node_unique of length num_added_nodes
		[num_removed_nodes] : 1 long identifying the number of removed nodes (size of next section)
		[removed_nodes]			: array of epmem_node_unique of length num_removed_nodes
		[num_added_edges]		: 1 long identifying the number of added edges (size of next section)
		[added_edges]				: array of epmem_edge_unique of length num_added_edges
		[num_removed_edges] : 1 long identifying the number of removed edges (size of next section)
		[removed_edges]			: array of epmem_edge_unique of length num_removed_edges
		*/
};


#endif /* NEW_EPISODE_H_ */
