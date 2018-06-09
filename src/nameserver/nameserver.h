#pragma once
/*
  DNS Nameserver to resolve video.cs.cmu.edu
*/

#include <stdio.h>
#include <inttypes.h>
#include "../common/linkedlist.h"

#define VID_DOMAIN "video.cs.cmu.edu"

/*
  Representation of a dns packet
*/
enum message_type {
        QUERY = 0,
        RESPONSE = 1
};

struct dns_t {
        /* use generate_dns_message to generate the struct */
        /* use free_dns_message to free the struct */
        uint16_t message_id;

        enum message_type type;
        int len; /* length of the complete dns request or response */
        /* length of request should always be DNS_REQUEST_LEN */
        /* length of response should always be DNS_RESPONSE_LEN */

        /**** exists if message is query ****/
        char *query_name; /* should always be "video.cs.cmu.edu" */

        /**** exists if message is response ****/
	char *response_name; /* should always be "video.cs.cmu.edu"  */
	char *response_ip; /* ip resolved eg. "4.0.0.1" */

        int invalid_request; /* flagged if the request is invalid */
};

/**
   Load balancing type the DNS uses when queried for entry
*/
enum load_balance_t {
	RR, /* round-robin */
	GEO /* geographic distance */
};

/**
  DNS configuration setup

  Constructed from command line arguments:
  ./nameserver [-r] <log> <ip> <port> <servers> <LSAs>
*/
struct dns_config_t {
	FILE *log;

	/* File names */
	const char *logFilename;
	const char *serversFile;
	const char *lsaFile;

	const char *ip;
	const char *port;
	int socket;

	enum load_balance_t lbType;

#define MAX_SERVERS 100
	char *servers[MAX_SERVERS];
	size_t serversCount;

	Node *graphNodes;
};
