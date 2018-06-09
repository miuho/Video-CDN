/*
  Functions for manipulating the state of the nameserver
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <assert.h>

#include "nameserver.h"
#include "nameserver-core.h"
#include "../common/mydnsparse.h"
#include "../common/log.h"
#include "../common/mytime.h"

#define BUF_SIZE 4096
#define FMT "%f %s %s %s\n" /* time client-ip query-name response-ip */

/**
   Start the DNS server.

   return 0 on success, -1 on failure.
*/
static int setupListen(struct dns_config_t *config);

/**
   Process the buffer buf of size len that was filled by a recvfrom call
*/
static void processRecvfrom(struct dns_config_t *config, uint8_t *buf,
			    ssize_t len, struct sockaddr *src_addr,
			    socklen_t addlen);

/**
   Start the DNS server
*/
static void dns_Start(struct dns_config_t *config);

int main(int argc, char **argv)
{
	struct dns_config_t config;

	memset(&config, 0, sizeof(config));
	if (dns_ParseConfig(&config, argc, argv)) {
		log(DEFAULT_LOG, "parse config failed.\n");
		return EXIT_FAILURE;
	}

	config.log = logSetup(config.logFilename);
	if (!(config.log)) {
		log(DEFAULT_LOG, "setup log failed.\n");
		return EXIT_FAILURE;
	}

	if (dns_ParseServers(&config)) {
		log(DEFAULT_LOG, "parse servers failed.\n");
		logClose(&(config.log));
		return EXIT_FAILURE;
	}

	if(config.lbType == GEO) {
		if (dns_ConstructGraph(&config)) {
			log(DEFAULT_LOG, "failed to construct graph.\n");
			logClose(&(config.log));
			return EXIT_FAILURE;
		}
	}

	config.socket = setupListen(&config);
	if (config.socket == -1) {
		freeServers(&config);
		log(DEFAULT_LOG, "start dns failed.\n");
		logClose(&(config.log));
		return EXIT_FAILURE;
	}

	log(DEFAULT_LOG, "DNS Starting...\n");

	dns_Start(&config);

	freeServers(&config);
	logClose(&(config.log));

	log(DEFAULT_LOG, "DNS Shutting Down...\n");

	return EXIT_SUCCESS;
}

static void dns_Start(struct dns_config_t *config)
{
	fd_set recvfds;
	struct sockaddr src_addr;
	socklen_t addrlen;
	uint8_t buf[BUF_SIZE];
	ssize_t size;

	while (1) {
		memset(&src_addr, 0, sizeof(src_addr));
		addrlen = sizeof(src_addr);

		FD_ZERO(&recvfds);
		FD_SET(config->socket, &recvfds);

		select(config->socket + 1, &recvfds, NULL, NULL, NULL);

		if (FD_ISSET(config->socket, &recvfds)) {
			if ((size = recvfrom(config->socket, buf, BUF_SIZE, 0,
					     &src_addr, &addrlen)) == -1)
				continue;

			processRecvfrom(config, buf, size, &src_addr, addrlen);
		}
	}
}

static void processRecvfrom(struct dns_config_t *config, uint8_t *buf,
			    ssize_t len, struct sockaddr *src_addr,
			    socklen_t addrlen)
{
	struct dns_t *dnsRequest;
	struct dns_t *dnsReply;
	const char *ip;
	uint8_t *response;
	ssize_t responseLen;
	char client[NI_MAXHOST];

	/* quiet gcc warning */
	len = len;

	if (!(dnsRequest = deserialize_dns(buf))) {
		log(DEFAULT_LOG, "deserialize dns failed.\n");
		return;
	}

	/* find numerical representation "127.0.0.1" of the client for logging
	   and geo load balancing purposes */
	if (getnameinfo(src_addr, addrlen, client, NI_MAXHOST,
			NULL, 0, NI_NUMERICHOST)) {
		perror("getnameinfo");
		log(DEFAULT_LOG, "failed to get hostname.\n");
	}

	if (config->lbType == RR) {
		ip = getRRIP(config);
	} else {
		ip = getGEOIP(config, client);
	}

	if (!ip) {
		log(DEFAULT_LOG, "failed to find server ip\n");
	} else {
		/* DNS only handles resolution for DOMAIN (in nameserver.h)*/
		if (strcmp(dnsRequest->query_name, VID_DOMAIN)) {
			free(dnsRequest->query_name);
			dnsRequest->query_name = NULL;

			log(DEFAULT_LOG, "request not for %s\n", VID_DOMAIN);
			if (!(dnsReply = generate_dns_message(dnsRequest->message_id,
							      RESPONSE, NULL, 1))) {
				log(DEFAULT_LOG, "generate dns failed.\n");
				return;
			}
		} else {
			if (!(dnsReply = generate_dns_message(dnsRequest->message_id,
							      RESPONSE,
							      (char *) ip, 0))) {
				log(DEFAULT_LOG, "generate dns failed for %s\n", ip);
				return;
			}
		}

		if (!(response = serialize_dns(dnsReply))) {
			log(DEFAULT_LOG, "serialize dns failed for %s\n", ip);
			return;
		}

		responseLen = dnsReply->len;

		/* prevent free_dns_message from freeing the response ip */
		dnsReply->response_ip = NULL;
		free_dns_message(dnsReply);

		log_activity(config->log, FMT,
			     microtime(NULL) / 1000000.0,
			     client, VID_DOMAIN, ip);

		if (sendto(config->socket, response, responseLen, 0, src_addr,
			   addrlen)
		    != responseLen) {
			log(DEFAULT_LOG, "did not send all the data.\n");
		}
	}
}

static int setupListen(struct dns_config_t *config)
{
	struct addrinfo hints;
	struct addrinfo *results;
	struct addrinfo *tmp;
	int s; /* socket */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;

	log(DEFAULT_LOG, "setup DNS listen on %s:%s\n", config->ip, config->port);

	if (getaddrinfo(config->ip, config->port, &hints, &results)) {
		perror("getaddrinfo");
		return -1;
	}

	for (tmp = results; tmp; tmp = tmp->ai_next) {
		if ((s = socket(tmp->ai_family, tmp->ai_socktype,
				tmp->ai_protocol)) == -1)
			continue;

		if (!bind(s, tmp->ai_addr, tmp->ai_addrlen))
			break;

		close(s);
	}

	freeaddrinfo(results);

	if (!tmp) {
		log(DEFAULT_LOG, "failed to create socket and bind for dns listen.\n");
		return -1;
	}

	return s;
}
