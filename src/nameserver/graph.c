/*
  Functions to manipulate a graph structure
*/

#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "../common/log.h"

#define DEFAULT_NEIGHBORS 4

/**
   Add neighbor to node.  This is not a symmetical operation. (i.e. node does
   not become neighbor of neighbor.)

   @param node the node that's getting a new neighbor
   @param neighbor the node that's becoming a new neighbor
   @return 0 on success, 1 otherwise
*/

static int addNeighbor(struct graph_node_t *node, struct graph_node_t *neighbor);

/**
   Find whether or not neighbor is in node's neighbors list.  The comparison is
   done by comparing the pointers, not the neighbors' ids.

   @param node the node whose list of neighbors will be checked
   @param neighbor the neighbor node
   @return 1 if n2 is a neighbor of n1, 0 otherwise
*/
static int isNeighbor(struct graph_node_t *node, struct graph_node_t *neighbor);

/**
   Given a graph node, prints a list of its neighbors, separated by spaces.

   @param node the graph node whose neighbors will be printed.
*/
static void printNeighbors(struct graph_node_t *node);

struct graph_node_t *newGraphNode(char *id)
{
	struct graph_node_t *node;

	if (!(node = calloc(1, sizeof(*node)))) {
		log(DEFAULT_LOG, "failed to calloc memory for graph node.\n");
		return NULL;
	}

	if (!(node->id = strdup(id))) {
		log(DEFAULT_LOG, "strdup failed.\n");
		free(node);
		return NULL;
	}

	return node;
}

int addEdge(struct graph_node_t *n1, struct graph_node_t *n2)
{
	if (!n1 || !n2)
		return EXIT_FAILURE;

	return addNeighbor(n1, n2) || addNeighbor(n2, n1);
}

int edgeExists(struct graph_node_t *n1, struct graph_node_t *n2)
{
	return isNeighbor(n1, n2) &&
		isNeighbor(n2, n1);
}

int cmpGraphById(const void *this, const void *id)
{
	struct graph_node_t *node = (struct graph_node_t *) this;
	return strcmp(node->id, id);
}

int cmpGraphByNode(const void *this, const void *that)
{
	return this != that;
}

void printGraph(struct graph_node_t *node)
{
	if (!node)
		return;

	log_activity(DEFAULT_LOG, "node: %s -> ", node->id);
	printNeighbors(node);
	log_activity(DEFAULT_LOG, "\n");
}

static void printNeighbors(struct graph_node_t *node)
{
	Node *tmp;
	struct graph_node_t *n;

	for (tmp = node->neighbors; tmp; tmp = tmp->next) {
		n = tmp->data;
		log_activity(DEFAULT_LOG, "%s ", n->id);
	}
}

static int isNeighbor(struct graph_node_t *node, struct graph_node_t *neighbor)
{
	if (node_find(node->neighbors, cmpGraphByNode, neighbor)) {
		return 1;
	}

	return 0;
}

static int addNeighbor(struct graph_node_t *node, struct graph_node_t *neighbor)
{
	if (!node_insert(&(node->neighbors), neighbor)) {
		log(DEFAULT_LOG, "failed to insert into neighbors.\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
