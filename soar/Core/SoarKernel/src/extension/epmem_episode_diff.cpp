/*
 * epmem_episode_diff.cpp
 *
 *  Created on: Nov 28, 2012
 *      Author: aaron
 */

#include "epmem_episode_diff.h"


epmem_episode_diff::epmem_episode_diff(int64_t time, int num_nodes_add, int num_nodes_remove, int num_edges_add, int num_edges_remove)
: time(time), 
	num_added_nodes(num_nodes_add), 
	num_removed_nodes(num_nodes_remove),
	num_added_edges(num_edges_add), 
	num_removed_edges(num_edges_remove)
{
		buffer_length = 5 + epmem_node_unique::NUM_LONGS*(num_nodes_add + num_nodes_remove) + 
			epmem_edge_unique::NUM_LONGS*(num_edges_add + num_edges_remove);
        buffer_size = buffer_length * sizeof(int64_t);

	buffer = new int64_t[buffer_length];

	int index = 0;

	// Time information
	buffer[index] = time;
	index += 1;

	// Added Nodes
	buffer[index] = num_added_nodes;
	index += 1;
	added_nodes = reinterpret_cast<epmem_node_unique*>(&buffer[index]);
	index += epmem_node_unique::NUM_LONGS * num_added_nodes;

	// Removed Nodes
	buffer[index] = num_removed_nodes;
	index += 1;
	removed_nodes = reinterpret_cast<epmem_node_unique*>(&buffer[index]);
	index += epmem_node_unique::NUM_LONGS * num_removed_nodes;

	// Added Edges
	buffer[index] = num_added_edges;
	index += 1;
	added_edges = reinterpret_cast<epmem_edge_unique*>(&buffer[index]);
	index += epmem_edge_unique::NUM_LONGS * num_added_edges;

	// Removed Edges
	buffer[index] = num_removed_edges;
	index += 1;
	removed_edges = reinterpret_cast<epmem_edge_unique*>(&buffer[index]);
	index += epmem_edge_unique::NUM_LONGS * num_removed_edges;
}

epmem_episode_diff::epmem_episode_diff(int64_t* buffer):buffer(buffer){
	int index = 0;

	// Time information
	time = buffer[index];
	index += 1;

	// Added Nodes
	num_added_nodes = buffer[index];
	index += 1;
	added_nodes = reinterpret_cast<epmem_node_unique*>(&buffer[index]);
	index += epmem_node_unique::NUM_LONGS * num_added_nodes;

	// Removed Nodes
	num_removed_nodes = buffer[index];
	index += 1;
	removed_nodes = reinterpret_cast<epmem_node_unique*>(&buffer[index]);
	index += epmem_node_unique::NUM_LONGS * num_removed_nodes;

	// Added Edges
	num_added_edges = buffer[index];
	index += 1;
	added_edges = reinterpret_cast<epmem_edge_unique*>(&buffer[index]);
	index += epmem_edge_unique::NUM_LONGS * num_added_edges;

	// Removed Edges
	num_removed_edges = buffer[index];
	index += 1;
	removed_edges = reinterpret_cast<epmem_edge_unique*>(&buffer[index]);
	index += epmem_edge_unique::NUM_LONGS * num_removed_edges;

	buffer_length = 5 + epmem_node_unique::NUM_LONGS*(num_added_nodes + num_removed_nodes) + 
		epmem_edge_unique::NUM_LONGS*(num_added_edges + num_removed_edges);
    buffer_size = sizeof(int64_t) * buffer_length;
}