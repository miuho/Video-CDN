/*
  Functions for manipulating connections
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/log.h"
#include "connection.h"
#include "proxy-core.h"

struct connection_t *createConnection(int browserSocket, int serverSocket)
{
        struct connection_t *temp;

        if (!(temp = calloc(1, sizeof(struct connection_t)))) {
                log(DEFAULT_LOG, "calloc for connection failed.\n");
                perror("calloc");
                return NULL;
        }

        if (create_new_stream(&(temp->stream)))
                return NULL;

        log(DEFAULT_LOG, "%d <--> %d\n", browserSocket, serverSocket);

        temp->browser.socket = browserSocket;
        temp->server.socket = serverSocket;

        return temp;
}

int deleteConnection(struct connection_t *connection)
{
        if (!connection)
                return EXIT_FAILURE;

        log(DEFAULT_LOG, "%d || %d\n", connection->browser.socket,
            connection->server.socket);

        streamDelete(&(connection->stream));
        closeSocket(connection->browser.socket);
        bufferDelete(&(connection->browser.buf));

        if (connection->browser.socket != connection->server.socket) {
                closeSocket(connection->server.socket);
                bufferDelete(&(connection->server.buf));
        }

        memset(connection, 0, sizeof(*connection));
        return EXIT_SUCCESS;
}
