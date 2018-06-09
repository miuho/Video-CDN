/*
  The Video-Streaming Proxy
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "proxy.h"
#include "proxy-core.h"
#include "mydns.h"
#include "../common/log.h"
#include "connection.h"

/*
  Query the server for a manifest file.

  It is a wrapper function around the underneath stream structure, by setting up
  the appropriate connection sockets.

  return EXIT_SUCCESS or EXIT_FAILURE.
*/
static int getManifestWrapper(struct config_t *config);

int main(int argc, char **argv)
{
        struct config_t proxyConfig;
	int port;

        if (parseConfig(&proxyConfig, argc, argv)) {
                log(DEFAULT_LOG, "failed to parse command line arguments.\n");
                return EXIT_FAILURE;
        }

        if (!(proxyConfig.logFile = logSetup(proxyConfig.logFilename))) {
		log(DEFAULT_LOG, "log setup failed.\n");
		return EXIT_FAILURE;
	}

	errno = 0;
	port = strtol(proxyConfig.dnsPort, NULL, 10);
	if (errno) {
		perror("strtol");
		return EXIT_FAILURE;
	}

	if (init_mydns(proxyConfig.dnsIP, port)) {
		log(DEFAULT_LOG, "init mydns failed.\n");
		return EXIT_FAILURE;
	}

        proxyStart(&proxyConfig);
        logClose(&(proxyConfig.logFile));

	return EXIT_SUCCESS;
}

void proxyStart(struct config_t *config)
{
        int readyFds;
        fd_set recvfds, sendfds;

	log(DEFAULT_LOG, "Proxy Starting...\n");

        if (setupListen(config)) {
                log(DEFAULT_LOG, "setup listen failed.\n");
                return;
        }

        if (getManifestWrapper(config)) {
                log(DEFAULT_LOG, "get manifest failed.\n");
                return;
        }

        while (1) {
                setFdSets(config, &recvfds, &sendfds);
                readyFds = 0;

                if ((readyFds = select(config->maxFd + 1, &recvfds, &sendfds,
                                       NULL, NULL)) == -1) {
                        if (errno == EINTR)
                                continue;
                        fprintf(stderr, "select error.\n");
                        perror("select");
                }

                handleReadyFds(config, &recvfds, &sendfds);
        }
}

int dump_to_proxy(int socket, uint8_t *buffer, size_t length)
{
        struct connection_t *connection = connections[socket];

        if (socket == connection->browser.socket) {
                if (bufferAppend(&(connection->browser.buf), buffer, length)) {
                        log(DEFAULT_LOG, "append to browser buffer failed.\n");
                        return EXIT_FAILURE;
                }
        } else if (socket == connection->server.socket) {
                if (bufferAppend(&(connection->server.buf), buffer, length)) {
                        log(DEFAULT_LOG, "append to server buffer failed.\n");
                        return EXIT_FAILURE;
                }
        } else {
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}

static int getManifestWrapper(struct config_t *config)
{
        int socket;
        struct connection_t *connection;
        char ip[INET6_ADDRSTRLEN];

        if ((socket = createServerSock(config, ip, sizeof(ip))) == -1) {
                log(DEFAULT_LOG, "failed to create manifest socket.\n");
                return EXIT_FAILURE;
        }

        if (!(connection = createConnection(0, socket))) {
                log(DEFAULT_LOG, "failed to create manifest connection.\n");
                return EXIT_FAILURE;
        }

        log(DEFAULT_LOG, "manifest socket fd %d\n", socket);

        memcpy(connection->serverIP, ip, sizeof ip);
        monitorConnection(config, connection);
        get_manifest(socket);

        return EXIT_SUCCESS;
}
