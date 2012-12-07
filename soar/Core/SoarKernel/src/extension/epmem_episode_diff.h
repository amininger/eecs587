/*
 * epmem_episode_diff.h
 *
 *  Created on: Nov 28, 2012
 *      Author: aaron
 */

#ifndef EPMEM_EPISODE_DIFF_H_
#define EPMEM_EPISODE_DIFF_H_

struct epmem_episode_diff;

#include "../episodic_memory.h"
#include "../agent.h"


struct epmem_episode_diff{
	epmem_episode_diff(int64_t time, int num_nodes_add, int num_nodes_remove, int num_edges_add, int num_edges_remove);

	epmem_episode_diff(int64_t* buffer);
	epmem_episode_diff(int64_t* buffer, int size);

	~epmem_episode_diff(){
		delete [] buffer;
	}

	epmem_time_id time;

	epmem_node_unique* added_nodes;
	int num_added_nodes;

	epmem_node_unique* removed_nodes;
	int num_removed_nodes;

	epmem_edge_unique* added_edges;
	int num_added_edges;

	epmem_edge_unique* removed_edges;
	int num_removed_edges;

	int buffer_size;    // Size of the buffer in bytes
    int buffer_length;  // Length of the buffer array in int64_t
	int64_t* buffer;
	/* Buffer Structure (array of int64_ts)
		[time]							: 1 int64_t consisting of the time of the episode
		[num_added_nodes]		: 1 int64_t identifying the number of added nodes (size of next section)
		[added_nodes]				: array of epmem_node_unique of length num_added_nodes
		[num_removed_nodes] : 1 int64_t identifying the number of removed nodes (size of next section)
		[removed_nodes]			: array of epmem_node_unique of length num_removed_nodes
		[num_added_edges]		: 1 int64_t identifying the number of added edges (size of next section)
		[added_edges]				: array of epmem_edge_unique of length num_added_edges
		[num_removed_edges] : 1 int64_t identifying the number of removed edges (size of next section)
		[removed_edges]			: array of epmem_edge_unique of length num_removed_edges
		*/
};


#endif /* EPMEM_EPISODE_DIFF_H_ */
