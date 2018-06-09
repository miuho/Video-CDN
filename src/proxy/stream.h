#pragma once

/*
  A stream associates a video data transfer
*/

#include <time.h>

#include "../common/buffer.h"
#include "../common/mytime.h"
#include "config.h"

struct connection_t;

struct stream_t {
        mytime_t t_start, t_final; /* the start time and end time for a chunk */
        struct stream_buffer *request_buffer; /* buffer to write and read requests */
        struct stream_buffer *response_buffer; /* buffer to write and read requests */
};

/*
  Create a new Stream
*/
int create_new_stream(struct stream_t *stream);

/*
Prepare the forwarding data for proxy.
*/
void dump_to_stream(int recv_socket, struct connection_t *conn,
                    char *proxy_buffer, int bytes_received,
                    struct config_t *config);

/*
Generate a normal version manifest file request to server.
*/
void get_manifest(int send_socket);

/*
  Delete a Stream
*/
void streamDelete(struct stream_t *stream);
