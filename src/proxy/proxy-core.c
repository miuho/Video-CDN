/*
  Core functions for manipulating the proxy state
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#include "proxy-core.h"
#include "../common/log.h"
#include "mydns.h"

/*
  Given a new browser connection, create an internal connection that eventually
  will be associated with a server, if server is a valid destination.

  return EXIT_SUCCESS if successful, -1 if a fatal error occurred, and
  EXIT_FAILURE otherwise.
*/
static struct connection_t * createNewConnection(int listener,
                                                 struct config_t *config);

/*
  Store the data in the linked layer associated with the socket into a local
  buffer
*/
static void receiveConnection(struct config_t *config, int socket);

/*
  Bind the socket to a local IP address and port specified in config.
  The socket is used to communicate with the server.

  return EXIT_SUCCESS or EXIT_FAILURE.
*/
static int bindLocalPort(int socket, struct config_t *config);

/*
  Cleanly remove connection
*/
static void removeConnection(struct connection_t *connection);

/*
  Given the network address structure src, put the char representation
  of the address IP in dest, where n is the size of dest
*/
static void fillInIP(struct addrinfo *src, char *dest, size_t n);

static int connectionHaveContent(struct connection_t *c);

int closeSocket(int sock)
{
        if (sock <= 0)
                return EXIT_FAILURE;

        if (close(sock)) {
                log(DEFAULT_LOG, "close socket %d failed.\n", sock);
                perror("close");
                return EXIT_FAILURE;
        }

        log(DEFAULT_LOG, "close socket %d\n", sock);
        return EXIT_SUCCESS;
}

void handleReadyFds(struct config_t *config, fd_set *recvfds, fd_set *sendfds)
{
        int i;

        /*
          check every connection file descriptor and handle if it can be read
          or write to.
        */
        for (i = config->maxFd; i >= config->listener; i--) {
                if (FD_ISSET(i, sendfds)) {
                        sendConnection(i);
                } else if (FD_ISSET(i, recvfds)) {
                        if (i == config->listener)
                                createNewConnection(config->listener, config);
                        else
                                receiveConnection(config, i);
                }
        }
}

int createServerSock(struct config_t *config, char *ipBuf, size_t bufSize)
{
        struct addrinfo hints, *res, *tmp;
        int sockfd;
        const char *node = config->wwwIP ? config->wwwIP : config->hostname;

        log(DEFAULT_LOG, "Connection to video server: %s.\n", node);

        /* uses the new getaddrinfo way to fill in address information */
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

	if (config->wwwIP) {
		if (getaddrinfo(node, config->apachePort, &hints, &res)) {
			log(DEFAULT_LOG, "resolve failed.\n");
			return -1;
		}
	} else {
		if (resolve(config, node, config->apachePort, &hints, &res)) {
			log(DEFAULT_LOG, "resolve failed.\n");
			return -1;
		}
	}

        /*
          attempt to setup connection using every result from getaddrinfo,
          until it succeeds the first time
        */
        for (tmp = res; tmp; tmp = tmp->ai_next) {
                log(DEFAULT_LOG, "ai_family: (%d)\n",tmp->ai_family);
                log(DEFAULT_LOG, "ai_socktype: (%d)\n",tmp->ai_socktype);
                log(DEFAULT_LOG, "ai_protocol: (%d)\n",tmp->ai_protocol);

                if ((sockfd = socket(tmp->ai_family, tmp->ai_socktype,
                                     tmp->ai_protocol)) == -1) {
                        perror("socket");
                        continue;
                }

                if (bindLocalPort(sockfd, config)) {
                        closeSocket(sockfd);
                        continue;
                }

                log(DEFAULT_LOG, "sin_family: (%d)\n",
		    ((struct sockaddr_in *)tmp->ai_addr)->sin_family);
                log(DEFAULT_LOG, "sin_port: (%d)\n",
		    ntohs(((struct sockaddr_in *)tmp->ai_addr)->sin_port));

                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET,
			  &(((struct sockaddr_in *)tmp->ai_addr)->sin_addr),
			  str, INET_ADDRSTRLEN);

                log(DEFAULT_LOG, "ai_addr: (%s)\n", str);

                if (connect(sockfd, tmp->ai_addr, tmp->ai_addrlen)) {
                        perror("connect");
                        closeSocket(sockfd);
                        continue;
                } else {
                        fillInIP(tmp, ipBuf, bufSize);
                        break;
                }
        }

	if (!(config->wwwIP)) /*free ai_addr since freeaddrinfo doesn't do it*/
		free(res->ai_addr);

	freeaddrinfo(res);

        if (!tmp) {
                log(DEFAULT_LOG, "resolve server failed.\n");
                return -1;
        }

        return sockfd;
}

struct connection_t *createNewConnection(int listener, struct config_t *config)
{
        int clientSock, serverSock;
        socklen_t cliSize;
        struct sockaddr_in cliAddr;
        struct connection_t *connection;
        char ip[INET6_ADDRSTRLEN];

        log(DEFAULT_LOG, "new connection.\n");

        cliSize = sizeof(cliAddr);
        if ((clientSock = accept(listener, (struct sockaddr *) &cliAddr,
                                 &cliSize)) == -1) {
                switch(errno) {
                case ECONNABORTED:
                        log(DEFAULT_LOG, "accept failed.\n");
                default:
                        perror("accept");
                        return NULL;
                }
        }

        if ((serverSock = createServerSock(config, ip, sizeof(ip))) == -1) {
                log(DEFAULT_LOG, "create server sock failed.\n");
                closeSocket(clientSock);
                return NULL;
        }

        if (!(connection = createConnection(clientSock, serverSock))) {
                log(DEFAULT_LOG, "create connection failed.\n");
                closeSocket(serverSock);
                closeSocket(clientSock);
                return NULL;
        }

        memcpy(connection->serverIP, ip, sizeof(ip));
        monitorConnection(config, connection);

        return connection;
}

void receiveConnection(struct config_t *config, int socket)
{
        int bytesRecvd = 0;
        struct connection_t *connection;
        char chr[BUF_SIZE];

        if (!(connection = connections[socket]))
                return;

        memset(chr, 0, sizeof(chr));

        if ((bytesRecvd = recv(socket, chr, BUF_SIZE, 0)) == -1) {
                if (errno == ECONNRESET)
                        removeConnection(connection);
                fprintf(stderr, "fd %d ", socket);
                perror("recv");
        } else if (bytesRecvd == 0) {
                log(DEFAULT_LOG, "received 0 bytes.\n");
                if (!connectionHaveContent(connection))
                        removeConnection(connection);
        } else {
                dump_to_stream(socket, connection, chr, bytesRecvd, config);
        }
}

static int connectionHaveContent(struct connection_t *c)
{
        if (!c)
                return 0;

        return bufferHaveContent(&(c->browser.buf)) ||
                bufferHaveContent(&(c->server.buf));
}

void sendConnection(int socket)
{
        int bytesSent = 0;
        struct buffer *buf;
        struct connection_t *connection;

        if (!(connection = connections[socket]))
                return;

        /* determine which buffer contains data to be sent */
        if (socket == connection->browser.socket)
                buf = &(connection->browser.buf);
        else
                buf = &(connection->server.buf);

        if ((bytesSent = send(socket, buf->buf, buf->contentLength, 0)) == -1) {
                switch(errno) {
                case ECONNRESET:
                case EHOSTUNREACH:
                case EPIPE:
                        removeConnection(connection);
                default:
                        fprintf(stderr, "fd %d ", socket);
                        perror("send");
                        break;
                }
        } else {
                log(DEFAULT_LOG, "sent %d / %lu bytes\n",
                    bytesSent,
                    buf->contentLength);
                microtime(&((connection->stream).t_start));

                /* clear the buffer of the content that was sent */
                bufferRemoveContent(buf, bytesSent);
        }
}

void setFdSets(struct config_t *config, fd_set *recvfds, fd_set *sendfds)
{
        int i;
        struct connection_t *conn;

        FD_ZERO(recvfds);
        FD_ZERO(sendfds);

        for (i = config->maxFd; i > config->listener; i--) {
                if ((conn = connections[i])) {
                        if (bufferHaveContent(&(conn->browser.buf)))
                                FD_SET(conn->browser.socket, sendfds);

                        if (bufferHaveContent(&(conn->server.buf)))
                                FD_SET(conn->server.socket, sendfds);

                        FD_SET(conn->browser.socket, recvfds);
                        FD_SET(conn->server.socket, recvfds);
                }
        }

        FD_SET(config->listener, recvfds);
}

int setupListen(struct config_t *config)
{
        int listener = -1;
        struct addrinfo hints, *results, *tmp;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if (getaddrinfo(NULL, config->proxyPortChar, &hints, &results)) {
                log(DEFAULT_LOG, "getaddrinfo for local port failed.\n");
                perror("getaddrinfo");
                return EXIT_FAILURE;
        }

        for (tmp = results; tmp; tmp = tmp->ai_next) {
                if ((listener = socket(tmp->ai_family, tmp->ai_socktype,
                                       tmp->ai_protocol)) == -1) {
                        perror("socket");
                        continue;
                }

                if (bind(listener, tmp->ai_addr, tmp->ai_addrlen) == -1) {
                        perror("bind");
                        closeSocket(listener);
                        continue;
                } else {
                        break;
                }
        }

        freeaddrinfo(results);

        if (!tmp) {
                log(DEFAULT_LOG, "bind for listen failed.\n");
                return EXIT_FAILURE;
        }

        if (listen(listener, config->backlog)) {
                closeSocket(listener);
                log(DEFAULT_LOG, "listen call failed.\n");
                perror("listen");
                return EXIT_FAILURE;
        }

        signal(SIGPIPE, SIG_IGN); /* SIGPIPE should not crash program */
        config->listener = listener;
        config->maxFd = config->listener;

        //printf("listener %d %d\n", config->proxyPort, config->listener);

        return EXIT_SUCCESS;
}

void monitorConnection(struct config_t *config, struct connection_t *c)
{
        if (!c)
                return;

        connections[c->browser.socket] = c;
        connections[c->server.socket] = c;

        config->maxFd = max(c->browser.socket, config->maxFd);
        config->maxFd = max(c->server.socket, config->maxFd);
}

static void fillInIP(struct addrinfo *src, char *dest, size_t size)
{
        void *addr;
        struct sockaddr_in *ipv4;
        struct sockaddr_in6 *ipv6;

        if (src->ai_family == AF_INET) {
                ipv4 = (struct sockaddr_in *) src->ai_addr;
                addr = &(ipv4->sin_addr);
        } else {
                ipv6 = (struct sockaddr_in6 *) src->ai_addr;
                addr = &(ipv6->sin6_addr);
        }

        inet_ntop(src->ai_family, addr, dest, size);
        log(DEFAULT_LOG, "address: %s\n", dest);
}

static int bindLocalPort(int socket, struct config_t *config)
{
        struct addrinfo hints, *results, *tmp;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        /* Bind the socket to a specific IP (fake-ip) */
        if (getaddrinfo(config->fakeIP, INADDR_ANY,
                        &hints, &results)) {
                log(DEFAULT_LOG, "getaddrinfo for local port failed.\n");
                perror("getaddrinfo");
                return EXIT_FAILURE;
        }

        for (tmp = results; tmp; tmp = tmp->ai_next) {
                if (bind(socket, tmp->ai_addr, tmp->ai_addrlen) == -1) {
                        perror("bind");
                        continue;
                } else {
                        break;
                }
        }

        freeaddrinfo(results);

        if (!tmp) {
                log(DEFAULT_LOG, "bind to local port failed.\n");
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}

static void removeConnection(struct connection_t *connection)
{
        if (!connection)
                return;

        log(DEFAULT_LOG, "%d || %d\n", connection->browser.socket, connection->server.socket);
        connections[connection->browser.socket] = NULL;
        connections[connection->server.socket] = NULL;

        deleteConnection(connection);
}
