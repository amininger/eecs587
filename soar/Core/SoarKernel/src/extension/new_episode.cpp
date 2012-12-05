/*
 * new_episode.cpp
 *
 *  Created on: Nov 28, 2012
 *      Author: aaron
 */

#include "new_episode.h"


new_episode::new_episode(long time, int num_nodes_add, int num_nodes_remove, int num_edges_add, int num_edges_remove)
: time(time), 
	num_added_nodes(num_nodes_add), 
	num_removed_nodes(num_nodes_remove),
	num_added_edges(num_edges_add), 
	num_removed_edges(num_edges_remove)
{
		buffer_size = 5 + epmem_node_unique::NUM_LONGS*(num_nodes_add + num_nodes_remove) + 
			epmem_edge_unique::NUM_LONGS*(num_edges_add + num_edges_remove);

	buffer = new long[buffer_size];

	long* addr = buffer;

	// Time information
	*addr = time;
	addr += 1;

	// Added Nodes
	*addr = num_added_nodes;
	addr += 1;
	added_nodes = reinterpret_cast<epmem_node_unique*>(addr);
	addr += epmem_node_unique::NUM_LONGS * num_added_nodes;

	// Removed Nodes
	*addr = num_removed_nodes;
	addr += 1;
	removed_nodes = reinterpret_cast<epmem_node_unique*>(addr);
	addr += epmem_node_unique::NUM_LONGS * num_removed_nodes;

	// Added Edges
	*addr = num_added_edges;
	addr += 1;
	added_edges = reinterpret_cast<epmem_edge_unique*>(addr);
	addr += epmem_edge_unique::NUM_LONGS * num_added_edges;

	// Removed Edges
	*addr = num_removed_edges;
	addr += 1;
	removed_edges = reinterpret_cast<epmem_edge_unique*>(addr);
	addr += epmem_edge_unique::NUM_LONGS * num_removed_edges;
}

new_episode::new_episode(long* buffer):buffer(buffer){
	long* addr = buffer;

	// Time information
	time = *addr;
	addr += 1;

	// Added Nodes
	num_added_nodes = *addr;
	addr += 1;
	added_nodes = reinterpret_cast<epmem_node_unique*>(addr);
	addr += epmem_node_unique::NUM_LONGS * num_added_nodes;

	// Removed Nodes
	num_removed_nodes = *addr;
	addr += 1;
	removed_nodes = reinterpret_cast<epmem_node_unique*>(addr);
	addr += epmem_node_unique::NUM_LONGS * num_removed_nodes;

	// Added Edges
	num_added_edges = *addr;
	addr += 1;
	added_edges = reinterpret_cast<epmem_edge_unique*>(addr);
	addr += epmem_edge_unique::NUM_LONGS * num_added_edges;

	// Removed Edges
	num_removed_edges = *addr;
	addr += 1;
	removed_edges = reinterpret_cast<epmem_edge_unique*>(addr);
	addr += epmem_edge_unique::NUM_LONGS * num_removed_edges;

	buffer_size = 5 + epmem_node_unique::NUM_LONGS*(num_added_nodes + num_removed_nodes) + 
		epmem_edge_unique::NUM_LONGS*(num_added_edges + num_removed_edges);
}