#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mydns.h"
#include "../common/mydnsparse.h"
#include "../common/log.h"

static struct dnsConfig dns_config;

int init_mydns(const char *dns_ip, unsigned int dns_port)
{
	if (!dns_ip)
		return -1;

	memset(&dns_config, 0, sizeof(dns_config));
	dns_config.ip = dns_ip;
	dns_config.port = dns_port;
	return 0;
}

/**
 * Send a serialized dns request to the dns server.
 *
 * @param  sock  The proxy listening udp socket.
 * @param  dns_request  The struct pointer that stores the dns request data.
 *
 * @return 0 on success, -1 otherwise
 */
static int send_to_dns_server(int sock, struct dns_t *dns_request)
{
	uint8_t *dns;
	struct sockaddr_in dns_addr;

	dns = serialize_dns(dns_request);
	if (dns == NULL) {
		log(DEFAULT_LOG, "serializing dns request failed\n");
		return -1;
	}

	/* put the dns server connection info into dns_addr */
	bzero(&dns_addr, sizeof(dns_addr));
	dns_addr.sin_family = AF_INET;
	inet_aton(dns_config.ip,
		  (struct in_addr *) &dns_addr.sin_addr.s_addr);
	dns_addr.sin_port = htons((uint16_t)dns_config.port);

	if (dns_request->len != sendto(sock, dns, dns_request->len, 0,
			               (struct sockaddr *) &(dns_addr),
			               sizeof(dns_addr))) {
		log(DEFAULT_LOG, "sending dns request failed\n");
		return -1;
	}

	free(dns);
	return 0;
}

/**
 * Received a deserialized dns response from the dns server.
 *
 * @param  sock  The proxy listening udp socket.
 *
 * @return the struct pointer that stores the dns response data, NULL if
 * unsuccessful
 */
static struct dns_t *recv_from_dns_server(int sock)
{
	struct sockaddr_in from;
	socklen_t fromlen;
	char buf[DNS_BUF_SIZE];
	struct dns_t *dns_response;
	int response_len;

	fromlen = sizeof(from);
	if ((response_len = recvfrom(sock, buf, DNS_BUF_SIZE, 0,
				     (struct sockaddr *) &from,
				     &fromlen)) <= 0) {
		log(DEFAULT_LOG, "receiving dns response failed\n");
		return NULL;
	}

	dns_response = deserialize_dns((uint8_t *)buf);
	if (dns_response == NULL) {
		log(DEFAULT_LOG, "deserializing dns response failed\n");
		return NULL;
	}

	return dns_response;
}

/**
 * Received a deserialized dns response from the dns server.
 *
 * @param  dns_response  The response struct from dns server.
 * @param  res The addrinfo pointer to fill in connection setup data.
 */
static void addrinfo_from_response(struct dns_t *dns_response,
				   struct addrinfo **res,
				   const char *node,
				   const char *service)
{
	struct addrinfo *tmp;
	struct sockaddr_in *server_addr;
	char *cannonname;

	tmp = calloc(1, sizeof(struct addrinfo));

	if (!tmp) {
		*res = NULL;
		return;
	}

	server_addr = calloc(1, sizeof(struct sockaddr_in));
	if (!server_addr) {
		free(tmp);
		*res = NULL;
		return;
	}

	cannonname = strdup(node);
	if (!cannonname) {
		free(tmp);
		free(server_addr);
		*res = NULL;
		return;
	}

	/* fill in sockaddr_in */
	server_addr->sin_family = AF_INET;
	inet_aton(dns_response->response_ip,
		  (struct in_addr *) &((server_addr->sin_addr).s_addr));
	server_addr->sin_port = htons((uint16_t)atoi(service)); /* "8080" */

	/* fill in addrinfo */
	tmp->ai_family = AF_INET;
	tmp->ai_socktype = SOCK_STREAM;
	tmp->ai_protocol = IPPROTO_TCP;
	tmp->ai_addrlen = sizeof(struct sockaddr_in);
	tmp->ai_addr = (struct sockaddr *) server_addr;
	tmp->ai_canonname = cannonname; /* "video.cs.cmu.edu" */
	tmp->ai_next = NULL;

	*res = tmp;
}

int resolve(struct config_t *proxy_config, const char *node,
	    const char *service, const struct addrinfo *hints,
	    struct addrinfo **res)
{
	struct sockaddr_in myaddr;
	fd_set readfds;
	struct timeval tv; /* timeouts for select */
	int sock;
	int nfds;
	struct dns_t *dns_request;
	struct dns_t *dns_response;

	/* gcc compilation unused variable */
	hints = hints;

	if ((!node) || (!service) || (!res) ||
	    (strcmp(node, "video.cs.cmu.edu")) ||
	    (strcmp(service, "8080"))) {
		log(DEFAULT_LOG, "resolve had invalid input\n");
		return -1;
	}

	/* create the listening udp socket for proxy */
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
		log(DEFAULT_LOG, "resolve could not create socket\n");
		return -1;
	}

	bzero(&myaddr, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	inet_aton(proxy_config->fakeIP,
		  (struct in_addr *) &myaddr.sin_addr.s_addr);
	myaddr.sin_port = htons((uint16_t)0);

	/* the listening udp socket binds to the fake-ip and an ephemeral port */
	if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
		log(DEFAULT_LOG, "resolve could not bind socket\n");
		return -1;
	}

	/* use select to avoid recvfrom blocks forever */
	FD_SET(sock, &readfds);

	/* select has timeout of 5 seconds */
	tv.tv_sec = 5;
        tv.tv_usec = 0;

	/* generate a query for dns server */
	if ((dns_request = generate_dns_message(0, QUERY, NULL, 0)) == NULL) {
		log(DEFAULT_LOG, "resolve could not generate request\n");
		return -1;
	}

	/* send the query to dns server */
	if (send_to_dns_server(sock, dns_request) == -1) {
		log(DEFAULT_LOG, "resolve could not sendto\n");
		return -1;
	}
	free_dns_message(dns_request);
	dns_request = NULL;

	nfds = select(sock+1, &readfds, NULL, NULL, &tv);

	if (nfds > 0) {
		if ((dns_response = recv_from_dns_server(sock)) == NULL) {
			log(DEFAULT_LOG, "resolve could not recvfrom\n");
			return -1;
		}

		if (dns_response->invalid_request) {
			log(DEFAULT_LOG, "dns server has no answer\n");
			return -1;
		}

		/* put the response ip address into addrinfo */
		addrinfo_from_response(dns_response, res, node, service);

		free_dns_message(dns_response);
		dns_response = NULL;
		return 0;
	} else if (nfds == 0) {
		log(DEFAULT_LOG, "resolve select times out\n");
		return -1;
	}

	return -1;
}
