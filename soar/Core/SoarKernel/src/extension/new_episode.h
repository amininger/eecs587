/*
 * new_episode.h
 *
 *  Created on: Nov 28, 2012
 *      Author: aaron
 */

#ifndef NEW_EPISODE_H_
#define NEW_EPISODE_H_

#include "../episodic_memory.h"
#include "../agent.h"

class new_episode{


};

// ADDING NODES/EDGES

// epmem_stmts_graph->add_node_now
// 	1. id
//  2. time
// add_node_now = "INSERT INTO node_now (id,start) VALUES (?,?)"


// node_mins[id] = time

// epmem_stmts_graph->add_edge_now
//  1. id
//  2. time
// add_edge_now = "INSERT INTO edge_now (id,start) VALUES (?,?)"

//edge_mins[id] = time

// epmem_stmts_graph->update_edge_unique_last
//  1. LONG_MAX
//  2. id
// update_edge_unique_last = "UPDATE edge_unique SET last=? WHERE parent_id=?"

// REMOVING NODES

// epmem_stmts_graph->delete_node_now
//  1. id
// delete_node_now = "DELETE FROM node_now WHERE id=?"

// range_start = node_mins[id]
// range_end = time

// if start == end
	// epmem_stmts_graph->add_node_point
	//  1. id
	//  2. start
	// add_node_point = "INSERT INTO node_point (id,start) VALUES (?,?)"
// else
	// epmem_rit_insert_interval(agent, start, end, id, epmem_rit_state_graph[STATE_NODE]
	// add_node_range = "INSERT INTO node_range (rit_node,start,end,id) VALUES (?,?,?,?)" );

// node_maxes[id] = true

// REMOVING EDGES

// epmem_stmts_graph->delete_edge_now
//	1. id
// delete_edge_now = "DELETE FROM edge_now WHERE id=?"

// range_start = edge_mins[id];
// range_end = time

// epmem_stmts_graph->update_edge_unique_last
//	1. range_end
//  2. id
// update_edge_unique_last = "UPDATE edge_unique SET last=? WHERE parent_id=?"

// if range_start == range_end
	// epmem_stmts_graph->add_edge_point
	//	1. id
	//  2. start
	// add_edge_point = "INSERT INTO edge_point (id,start) VALUES (?,?)"
// else
	// epmem_rit_insert_interval(agent, start, end, id, RIT_STATE_END)
	// add_edge_range = "INSERT INTO edge_range (rit_node,start,end,id) VALUES (?,?,?,?)" );


// edge_maxes[id] = true


// LTI PROMOTIONS
// epmem_promote_id(agent, epmem_promotion, time)
// symbol_remove_ref(agent, epmem_promotion

// ADD TIME
// epmem_stmts_graph->add_time
// 	1. time
// add_time = "INSERT INTO times (id) VALUES (?)"


// symbol_remove_ref(my_agent, my_time_sym)


#endif /* NEW_EPISODE_H_ */
