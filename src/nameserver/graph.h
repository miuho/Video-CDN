#pragma once
/*
  A general graph structure for LSA.  The nodes are identified by strings and
  the edges are implied based on the list of neighbors it has.
*/

#include <stdlib.h>
#include "../common/linkedlist.h"

struct graph_node_t {
	char *id;
	Node *neighbors; /* list of neighboring nodes */
	int weight;
};

/**
   Create a graph node with id id and no neighbors.  The id can be freed
   afterwards, since newGraphNode allocates memory internally.

   @param id the new node's id
   @return an allocated graph node on success, NULL otherwise.
*/
struct graph_node_t *newGraphNode(char *id);

/**
   Add an edge between two nodes.  In otherwise, the two nodes become neighbors.
   It does not check the existence of an edge already.

   @param n1 node 1
   @param n2 node 2
   @return 0 on success, 1 otherwise.
*/
int addEdge(struct graph_node_t *n1, struct graph_node_t *n2);

/**
   Test to see if an edge exists between two nodes. An edge exists if n1 has n2
   as an neighbor and n2 has n1 as an neighbor.

   @param n1 node 1
   @param n2 node 2
   @return 1 if an edge does exist, 0 otherwise.
*/
int edgeExists(struct graph_node_t *n1, struct graph_node_t *n2);

/**
   Wrapper function for linked lists's find function.  Tests whether or not
   this has the same id.

   @param this the graph node that's being tested
   @param id the id that the graph node's id is tested against

   @return 0 if it is an exact match, 1 otherwise.
*/
int cmpGraphById(const void *this, const void *ip);

/**
   Print the entire graph starting at a specific node.

   @param node the node that's the entry to the graph.
*/
void printGraph(struct graph_node_t *node);
