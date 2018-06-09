#include <stdlib.h>
#include <errno.h>

#include "proxy.h"
#include "config.h"
#include "../common/log.h"

#define BACKLOG 20
#define APACHE_PORT "8080"

int parseConfig(struct config_t *config, int argc, char **argv)
{
	if (argc < 7) {
                log(DEFAULT_LOG, "not enough arguments.\n");
                return EXIT_FAILURE;
        }

        /* log file */
        config->logFilename = argv[1];

        /* alpha */
        errno = 0;
        config->alpha = strtof(argv[2], NULL);
        if (errno) {
                log(DEFAULT_LOG, "parse alpha failed.\n");
                return EXIT_FAILURE;
        }

        /* listen port */
        errno = 0;
        config->proxyPort = (unsigned) strtol(argv[3], NULL, 10);
        if (errno) {
                log(DEFAULT_LOG, "parse listen port failed.\n");
                return EXIT_FAILURE;
        }
        config->proxyPortChar = argv[3];

        config->dnsPort = argv[6];
        config->fakeIP = argv[4];
        config->dnsIP = argv[5];

        if (argc > 7) {
                config->wwwIP = argv[7];
        } else {
		config->wwwIP = NULL;
	}

        /* predefined */
        config->hostname = VID_DOMAIN;
        config->backlog = BACKLOG;
        config->apachePort = APACHE_PORT;

        return EXIT_SUCCESS;
}
