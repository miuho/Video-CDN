#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "stream.h"
#include "connection.h"
#include "proxy.h"
#include "../common/log.h"
#include "parse.h"

void streamDelete(struct stream_t *stream)
{
        free(stream->request_buffer);
        stream->request_buffer = NULL;
        free(stream->response_buffer);
        stream->response_buffer = NULL;
        memset(stream, 0, sizeof(*stream));
}

int create_new_stream(struct stream_t *stream)
{
        /* initiate buffer for http request */
        struct stream_buffer *request_buffer =
                calloc(1, sizeof(struct stream_buffer));
        if (request_buffer == NULL) {
                log(DEFAULT_LOG, "not enough memory to allocate.\n");
                return EXIT_FAILURE;
        }
        stream->request_buffer = request_buffer;

        /* initiate buffer for http response */
        struct stream_buffer *response_buffer =
                calloc(1, sizeof(struct stream_buffer));
        if (response_buffer == NULL) {
                log(DEFAULT_LOG, "not enough memory to allocate.\n");
                return EXIT_FAILURE;
        }
        stream->response_buffer = response_buffer;

        log(DEFAULT_LOG, "created a new stream.\n");
        return EXIT_SUCCESS;
}

void dump_to_stream(int recv_socket, struct connection_t *conn,
                    char *proxy_buffer, int bytes_received,
                    struct config_t *config)
{
        struct stream_buffer *buffer;
        char *new_recv_buf;

        /* differentiate the received data is http request or response */
        if (recv_socket == (conn->browser).socket) {
                buffer = ((conn->stream).request_buffer);
        } else {
                assert(recv_socket == (conn->server).socket);
                buffer = ((conn->stream).response_buffer);
        }

        /* dynamically allocate memory for recv_buf for every recv calls */
        /* avoid if recv_len + bytes_received > previously defined buf_size */
        new_recv_buf = calloc(buffer->recv_len + bytes_received, sizeof(char));
        if (new_recv_buf == NULL) {
                log(DEFAULT_LOG, "not enough memory to allocate.\n");
                return;
        }

        //log(DEFAULT_LOG, "recv_len: (%d) bytes_received: (%d)\n", buffer->recv_len,
        //    bytes_received);
        if (buffer->recv_buf != NULL) {
                /* move previous data in old recv buffer to new recv buffer*/
                memcpy(new_recv_buf, buffer->recv_buf, buffer->recv_len);
                free(buffer->recv_buf);
                buffer->recv_buf = NULL;
        }
        /* move the new received data to the new recv buffer */
        memcpy(new_recv_buf + buffer->recv_len, proxy_buffer, bytes_received);
        buffer->recv_len += bytes_received;
        buffer->recv_buf = new_recv_buf;

        /* parse the received data */
        parse_data(recv_socket, conn, config);

        return;
}

void get_manifest(int send_socket)
{
        char *normal_manifest;

        /* send the normal manifest version request to server, so that proxy */
        /* can parse the bitrates */
        normal_manifest =
        "GET /vod/big_buck_bunny.f4m  HTTP/1.0\r\nConnection: close\r\n\r\n";
        dump_to_proxy(send_socket, (uint8_t *) normal_manifest,
                      strlen(normal_manifest));
        log(DEFAULT_LOG, "self-generated a normal manifest request.\n");
}
