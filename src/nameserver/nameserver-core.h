#pragma once

#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>

struct dns_config_t;

/**
   Take the arguments provided through the command line arguments to get
   DNS configuration parameters.

   return 0 for success, 1 for failure.
*/
int dns_ParseConfig(struct dns_config_t *config, int argc, char **argv);

/**
   Parse the list of servers in the servers list in config.

   return 0 for success, 1 for failure
*/
int dns_ParseServers(struct dns_config_t *config);

/**
   free all of the server addressed stored in the servers array in config
*/
void freeServers(struct dns_config_t *config);

/**
   Given a client identified by ip, find the closest video server
   based on geographical difference.

   return a valid ip if one is successfully found, NULL otherwise.
*/
const char *getGEOIP(struct dns_config_t *config, char *ip);

/**
   Return the next video ip address available based on round robin selecting

   return a valid ip if one is successfully found, NULL otherwise.
*/
const char *getRRIP(struct dns_config_t *config);

/**
   Parse the list of LSAs in config's lsa file list and construct the network graph
   from them.

   @param config the dns configuration that has the lsa list and where the network graph will be saved to.

   @return 0 if the graph was constructed successfully, 1 otherwise.
*/
int dns_ConstructGraph(struct dns_config_t *config);
