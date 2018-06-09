#pragma once
/*
  Header for handling the configuration for the proxy
*/

/* log format string (all on one line):

   <time> current time in seconds since epoch
   <duration> number of seconds it took to download from svr to proxy
   <tput> throughput for current chunk in Kbps
   <avg-tput> current EWMA throughput estimate in Kbps
   <bitrate> bitrate proxy requested for this chunk in Kbps
   <server-ip> the IP address of the server
   <chunkname> the name of the file proxy requested from the server
   (modified file name)
*/
#define LOG_FMT "%lu %f %d %d %d %s %s\n"

#include <stdio.h>

#define VID_DOMAIN "video.cs.cmu.edu"

struct config_t {
	FILE *logFile;

        char *logFilename,
                *hostname,
                *proxyPortChar; /* String representation of the proxy port */

        /* alpha for bitrate calculation */
        double alpha;

        /* ports and IPs */
        unsigned int proxyPort,
                backlog;

        const char *fakeIP,
                *dnsIP,
                *wwwIP,
                *dnsPort,
                *apachePort;

        int maxFd, /* the largest file descriptor for the connections */
		listener; /* listening file descriptor */
};

/*
  Parse the arguments into a global config struct.

  Command line arguments are:
  /proxy <log> <alpha> <listen-port> <fake-ip> <dns-ip> <dns-port> [<www-ip>]

  Returns EXIT_SUCESS if successful, EXIT_FAILURE otherwise
*/
int parseConfig(struct config_t *config, int argc, char **argv);
