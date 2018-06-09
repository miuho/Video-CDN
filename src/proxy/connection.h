#pragma once

/*
  Header for a Connection

  A connection is an association between a browser and a server
  and a stream underneath for manipulating the stream requests and calculating
  bitrates.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../common/buffer.h"
#include "stream.h"

struct socket_t {
        int socket;
        struct buffer buf;
};

struct connection_t {
        char serverIP[INET6_ADDRSTRLEN];

        int video_next_response;
        struct socket_t browser, server;
        struct stream_t stream;
};

/*
  Create a dynamically allocated connection with browser and server sockets
  associated with the ones passed in.

  returns NULL if failed, a valid pointer otherwise
*/
struct connection_t *createConnection(int browser, int server);

/*
  Safely deallocate the connection

  return EXIT_SUCCESS or EXIT_FAILURE
*/
int deleteConnection(struct connection_t *connection);
