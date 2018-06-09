#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "nameserver-core.h"
#include "nameserver.h"
#include "../common/log.h"
#include "../common/linkedlist.h"
#include "graph.h"

#define OPT_STRING "r"
#define min(a,b) ((a < b) ? (a) : (b))
#define LSA_FMT "%s %d %s\n" /* <sender> <seq number> <neighbors> */
#define DEFAULT_IP "0.0.0.0"

static unsigned rrIndex; /* round robin index */

struct lsa {
	char *ip;
	int seqNum;
	char *neighbors;
};

/**
   Given a list to store parsed LSAs, look through the list and only store
   the neighbors for a given ip, based on the highest seqNum.

   If the ip does not exist in the list, add the ip to the list.  Therefore,
   the list is guaranteed to have the latest (highest sequence number) neighbors
   for the given ip.

   @param list the list of parsed LSA
   @param ip the ip portion of the LSA
   @param seqNum the seq number of the LSA
   @param neighbors the neights of the LSA

   @return 0 if store was successful, or 1 otherwise.
*/
static int storeLatestLSA(Node **list, char *ip, int seqNum, char *neighbors);

/**
   Construct a graph of the network's topology based on the list of LSA.

   @param config dns configuration. The graph will be attached to it.
   @param list a list of LSA, which will be mutated to be a list of graph nodes.

   @return 0 if the entire graph was constructed correctly, 1 otherwise.
*/
static int constructNetworkGraph(struct dns_config_t *config, Node **list);

/**
   Connect the nodes with id neighbor and id ip together.  The nodes are fetched
   from the list of graph nodes in config.  If the nodes are not found, they
   are created and added to the list of nodes.

   @param config dns configuration, with list of graph nodes.
   @param ip the id of a node
   @param neighbor the id of the other node

   @return 0 on successfully connection the nodes, 1 otherwise.
*/
static int connectNodes(struct dns_config_t *config, char *ip, char *neighbor);

/**
   Wrapper function for linked lists's find function.  Tests whether or not
   this has the same ip.

   @param this the lsa struct that's being tested
   @param ip the ip that the lsa's ip is tested against

   @return 0 if it is an exact match, 1 otherwise.
*/
static int cmpLsaByIp(const void *this, const void *ip);

/**
   Set all of the nodes in the list of graph nodes to infinite weight.
   This is called in preparation for the Djikstra's algorithm

   @param graphNodes a list of nodes in the graph
*/
static void infAllNodes(Node *graphNodes);

/**
   Return the linked list node containing the graph node with the smallest
   weight in list L.

   @param L the list of nodes to find minimum node from

   @return a valid list to node with the min weight if success, NULL otherwise.
*/
static Node *findMinWeight(Node *node);

/**
   After running Djikstra's algorithm to change the distance from a given
   client, return the numerical representation of the ip for the server closest
   to the client.

   @param config the dns configuration that contains the graph and list of
   servers

   @return the ip representation of the closest server
 */
static const char *minWeightedServer(struct dns_config_t *config);

int dns_ParseConfig(struct dns_config_t *config, int argc, char **argv)
{
	int opt;

	/* Determine Load Balancing Type */
	config->lbType = GEO;
	while((opt = getopt(argc, argv, OPT_STRING)) != -1) {
		switch(opt) {
		case 'r':
			config->lbType = RR;
			break;
		default: /* '?' */
			break;
		}
	}

	if ((config->lbType == RR && argc < 7) ||
	    (config->lbType == GEO && argc < 6)) {
		log(DEFAULT_LOG, "not enough arguments.\n");
		return EXIT_FAILURE;
	}

	if (config->lbType == RR) {
		/* round robin */
		config->logFilename = argv[2];
		config->ip = argv[3];
		config->port = argv[4];
		config->serversFile = argv[5];
		config->lsaFile = argv[6];
		rrIndex = 0;
	} else { /* geographic */
		config->logFilename = argv[1];
		config->ip = argv[2];
		config->port = argv[3];
		config->serversFile = argv[4];
		config->lsaFile = argv[5];
	}

	return EXIT_SUCCESS;
}

int dns_ParseServers(struct dns_config_t *config)
{
	FILE *file;
	char *ip = NULL;
	size_t ipSize;
	int length;

	if (!(file = fopen(config->serversFile, "r"))) {
		perror("fopen");
		log(DEFAULT_LOG, "fopen servers file failed.\n");
		return EXIT_FAILURE;
	}

	memset(&config->servers, 0, MAX_SERVERS * sizeof(config->servers[0]));
	config->serversCount = 0;

	/* read servers file one line at a time and store the ip address
	   in the servers array */
	while((length = getline(&ip, &ipSize, file)) != -1) {
		if (!(config->servers[config->serversCount] =
		      strndup(ip, length - 1))) {
			perror("strdup");
			fclose(file);
			freeServers(config);
			return EXIT_FAILURE;
		}

		log(DEFAULT_LOG, "servers %s\n",
		    config->servers[config->serversCount]);

		config->serversCount++;

		if (config->serversCount > MAX_SERVERS) {
			log(DEFAULT_LOG, "servers exceed limit of %d\n",
			    MAX_SERVERS);
			config->serversCount--;;
			fclose(file);
			freeServers(config);
			return EXIT_FAILURE;
		}
	}

	if (ip)
		free(ip);

	fclose(file);

	return EXIT_SUCCESS;
}

int dns_ConstructGraph(struct dns_config_t *config)
{
	FILE *file;
	char *line = NULL;
	size_t lineLen;
	char ip[NI_MAXHOST];
	int seqNum;
	char *neighbors;
	Node *list;
	int length;

	if (!(file = fopen(config->lsaFile, "r"))) {
		perror("fopen");
		log(DEFAULT_LOG, "fopen lsa file failed.\n");
		return EXIT_FAILURE;
	}

	list = NULL;
	while((length = getline(&line, &lineLen, file)) != -1) {
		log(DEFAULT_LOG, "%s\n", line);

		memset(ip, 0, NI_MAXHOST);
		neighbors = calloc(length, sizeof(char));
		if (!neighbors) {
			log(DEFAULT_LOG, "failed to calloc for lsa neighbors.\n");
			return EXIT_FAILURE;
		}

		/* break down line: ip sequence number neighbors */
		if (sscanf(line, LSA_FMT, ip, &seqNum, neighbors) < 3) {
			log(DEFAULT_LOG, "improper LSA format line.\n");
			free(neighbors);
			return EXIT_FAILURE;
		}

		log(DEFAULT_LOG, "parsed: %s %d %s\n", ip, seqNum, neighbors);

		if (storeLatestLSA(&list, ip, seqNum, neighbors)) {
			log(DEFAULT_LOG, "store latest LSA failed.\n");
			free(neighbors);
			return EXIT_FAILURE;
		}

		free(neighbors);
	}

	if (constructNetworkGraph(config, &list)) {
		log(DEFAULT_LOG, "failed to construct network graph.\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int constructNetworkGraph(struct dns_config_t *config, Node **list)
{
	Node *tmp;
	struct lsa *lsa;
	char *neighborTok;

	if (!list || !(*list))
		return EXIT_FAILURE;

	/* for each ip in the list, construct the ip's neighbors */
	for(tmp = *list; tmp; tmp = tmp->next) {
		lsa = tmp->data;

		/* parse individual neighbor */
		neighborTok = strtok(lsa->neighbors, ",\n");
		log(DEFAULT_LOG, "%s:%s\n", lsa->ip, neighborTok);

		while (neighborTok) {
			if (connectNodes(config, lsa->ip, neighborTok)) {
				log(DEFAULT_LOG, "failed to connect %s <--> %s",
				    lsa->ip, neighborTok);
				return EXIT_FAILURE;
			}

			neighborTok = strtok(NULL, ",\n");
		}
	}

#ifdef DEBUG
	struct graph_node_t *node;
	if (config->graphNodes) {
		for (tmp = config->graphNodes; tmp; tmp = tmp->next) {
			node = tmp->data;
			printGraph(node);
		}
	}
#endif
	return EXIT_SUCCESS;
}

static int connectNodes(struct dns_config_t *config, char *ip, char *neighborTok)
{
	Node *n1, *n2; /* linked list nodes */
	struct graph_node_t *v1, *v2; /* vertices */

	/* find or create graph node with id ip */
	n1 = node_find(config->graphNodes, cmpGraphById, ip);
	if (n1) {
		v1 = n1->data;
	} else {
		v1 = newGraphNode(ip);
		if (!v1) {
			log(DEFAULT_LOG, "failed to calloc graph_node.\n");
			return EXIT_FAILURE;
		}

		n1 = node_insert(&(config->graphNodes), v1);
		if (!n1) {
			log(DEFAULT_LOG, "failed to insert node to graph list.\n");
			free(v1);
			return EXIT_FAILURE;
		}
	}

	/* find or create graph node with id neighborTok */
	n2 = node_find(config->graphNodes, cmpGraphById, neighborTok);
	if (n2) {
		v2 = n2->data;
	} else {
		v2 = newGraphNode(neighborTok);
		if (!v2) {
			log(DEFAULT_LOG, "failed to calloc graph_node.\n");
			node_delete(&(config->graphNodes), n1, NULL);
			free(v1);
			return EXIT_FAILURE;
		}

		n2 = node_insert(&(config->graphNodes), v2);
		if (!n2) {
			log(DEFAULT_LOG, "failed to insert node to graph list.\n");
			node_delete(&(config->graphNodes), n1, NULL);
			free(v1);
			free(v2);
			return EXIT_FAILURE;
		}
	}

	if (!edgeExists(v1, v2)) {
		if (addEdge(v1, v2)) {
			log(DEFAULT_LOG, "failed to add edge.\n");
			node_delete(&(config->graphNodes), n1, NULL);
			node_delete(&(config->graphNodes), n2, NULL);
			free(v1);
			free(v2);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

static int cmpLsaByIp(const void *this, const void *that)
{
	struct lsa *lsa = (struct lsa *)this;
	return strcmp(lsa->ip, that);
}

static int storeLatestLSA(Node **list, char *ip, int seqNum, char *neighbors)
{
	Node *tmp;
	struct lsa *tmpLSA;

	if (list == NULL)
		return EXIT_FAILURE;

	tmp = node_find(*list, cmpLsaByIp, ip);

	if (!tmp) {
		/* ip has not been stored in the list */
		tmpLSA = calloc(1, sizeof(*tmpLSA));
		if (!tmpLSA) {
			log(DEFAULT_LOG, "calloc for lsa struct failed.\n");
			return EXIT_FAILURE;
		}

		tmpLSA->ip = strdup(ip);
		if (!(tmpLSA->ip)) {
			log(DEFAULT_LOG, "strdup failed for ip.\n");
			return EXIT_FAILURE;
		}

		tmpLSA->neighbors = strdup(neighbors);
		if (!(tmpLSA->neighbors)) {
			log(DEFAULT_LOG, "strdup failed for neighbors.\n");
			free(tmpLSA->ip);
			return EXIT_FAILURE;
		}

		tmpLSA->seqNum = seqNum;
		node_insert(list, tmpLSA);
	} else {
		/* new LSA is more current */
		tmpLSA = tmp->data;
		if (tmpLSA->seqNum < seqNum) {
			free(tmpLSA->neighbors);

			tmpLSA->seqNum = seqNum;
			tmpLSA->neighbors = strdup(neighbors);
			if (!(tmpLSA->neighbors)) {
				log(DEFAULT_LOG, "strdup failed for neighbors.\n");
				fflush(stdout);
				return EXIT_FAILURE;
			}
		}
	}

	log(DEFAULT_LOG, "stored %s:%s\n", tmpLSA->ip, tmpLSA->neighbors);
	return EXIT_SUCCESS;
}

void freeServers(struct dns_config_t *config)
{
	size_t i;
	for (i = 0; i < config->serversCount; i++) {
		free(config->servers[i]);
	}
}

const char *getRRIP(struct dns_config_t *config)
{
	if (config->serversCount == 0)
		return NULL;

	return config->servers[rrIndex++ % config->serversCount];
}

const char *getGEOIP(struct dns_config_t *config, char *ip)
{
	Node *unvisited; /* set of vertices not visited */
	Node *curr;
	Node *neighbor;
	struct graph_node_t *gcurr; /* current graph node */
	struct graph_node_t *gneighbor; /* neighbor graph node */
	int weight;

	if (!node_dup_list(config->graphNodes, &unvisited)) {
		log(DEFAULT_LOG, "failed to duplicte graph nodes.\n");
		return NULL;
	}

	log(DEFAULT_LOG, "running djikstra's on %s\n", ip);
	infAllNodes(unvisited); /* max weight all nodes */

	/* set the current node with weight 0 */
	curr = node_find(unvisited, cmpGraphById, ip);
	gcurr = curr->data;
	gcurr->weight = 0;

	while(unvisited) {
		curr = findMinWeight(unvisited);
		gcurr = curr->data;

		for(neighbor = gcurr->neighbors; neighbor;
		    neighbor = neighbor->next) {
			if (node_find(unvisited, cmpGraphById, gcurr->id)) {
				/* only consider the unvisited */
				gneighbor = neighbor->data;

				/* new weight = current + distance */
				weight = gcurr->weight + 1;
				if (weight < gneighbor->weight)
					gneighbor->weight = weight;
			}
		}

		node_delete(&unvisited, curr, NULL);
	}

	return minWeightedServer(config);
}

static void infAllNodes(Node *graphNodes)
{
	Node *tmp;
	struct graph_node_t *gnode;

	for(tmp = graphNodes; tmp; tmp = tmp->next) {
		gnode = tmp->data;
		gnode->weight = INT_MAX;
	}
}

static Node *findMinWeight(Node *node)
{
	struct graph_node_t *gnode;
	int minWeight;
	Node *min = node;

	minWeight = INT_MAX;
	for(; node; node = node->next) {
		gnode = node->data;

		if (gnode->weight < minWeight) {
			minWeight = gnode->weight;
			min = node;
		}
	}

	return min;
}

static const char *minWeightedServer(struct dns_config_t *config)
{
	size_t i;
	const char *ip;
	int minWeight;
	Node *node;
	struct graph_node_t *gnode;

	minWeight = INT_MAX;
	ip = DEFAULT_IP;
	for (i = 0; i < config->serversCount; ++i) {
		node = node_find(config->graphNodes, cmpGraphById,
				 config->servers[i]);
		gnode = node->data;

		if (gnode->weight < minWeight) {
			minWeight = gnode->weight;
			ip = config->servers[i];
		}
	}

	return ip;
}
