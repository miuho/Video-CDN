/*
  Header file for the video streaming proxy
*/

#include <stdlib.h>
#include <inttypes.h>

#include "config.h"

/*
  Start the proxy based on the configuration provided
*/
void proxyStart(struct config_t *config);

/* Manipulating the proxy's internal buffers */

/*
  Change the proxy's internal buffer to contain the content from buffer buf of
  length n

  returns EXIT_SUCCESS if successful, EXIT_FAILURE otherwise.
*/
int dump_to_proxy(int socket, uint8_t *buf, size_t n);

/*
  Set up connection with the server specified by the hostname

  return EXIT_SUCCESS if successful, EXIT_FAILURE otherwise.
 */
int set_server(char *hostname);
