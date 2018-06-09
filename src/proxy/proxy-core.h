#pragma once

/*
  Header file for the proxy's core functions
*/

#include "connection.h"
#include "config.h"

/* A list of connections maintained by the proxy */
struct connection_t *connections[FD_SETSIZE];

/*
  Modify the recvfds and sendfds to reflect which file descriptors need to
  be monitored by select, for example.
*/
void setFdSets(struct config_t *config, fd_set *recvfds, fd_set *sendfds);

/*
  Flush bytes in the local buffer associated with the socket into the socket.
  The internal buffer may not be completely empty in a single call.
*/
void sendConnection(int socket);

/*
  Handle the file descripters that are ready to be read or write to, according
  to the select loop.
*/
void handleReadyFds(struct config_t *config, fd_set *recvfds, fd_set *sendfds);

/*
  Get a listening socket ready to receive browser requests

  return EXIT_SUCCESS if successful, EXIT_FAILURE otherwise
*/
int setupListen(struct config_t *config);

/*
  Create a socket facing the server based either on the www-ip or querying via
  DNS. It will also fill in the ip of the server, for logging purposes.

  return -1 on failure, or a non-negative for valid socket
*/
int createServerSock(struct config_t *config, char *ipBuf, size_t bufSize);

/*
  Close the indicated socket with proper logging

  return EXIT_SUCCESS or EXIT_FAILURE
*/
int closeSocket(int sock);

/*
  Add connection c to the list of connections the proxy monitors
*/
void monitorConnection(struct config_t *, struct connection_t *c);

#define max(a,b) a < b ? b : a
