#pragma once

/*
  Bitrate associates with the modifying the uri request for video fragments
*/

#include "connection.h"
#include "config.h"
#include "stream.h"

#define MAX_BITRATES_NUM 32 /* assume number of bitrates available is <= 32 */
#define BIT_LINE_SIZE 128

/* global variables */
int bitrates[MAX_BITRATES_NUM]; /* bitrates available for this video */
int bitrates_count; /* the number of bitrates read from manifest */
int throughput; /* the current throughput for the chunk */

/*
  Returns the current throughput for the video fragment.
*/
int calculate_throughput(struct connection_t *conn, int frag_size);

/*
  Returns the moving average throughput for the video fragment.
*/
int calculate_moving_average(struct config_t *config, int new_throughput);

/*
  Returns the lowest bitrate available of all bitrates.
*/
int lowest_bitrate();

/*
  Returns the modified bitrate of the http request in the send_buf to fittest
  bitrate, 0 if unsuccesful.
*/
int modfiy_bitrate(struct stream_buffer *buffer);

/*
  Modify the http request for normal version of manifest file to nolist version
  of manifest file for the proxy to parse the available bitrates.
*/
void normal_plus_nolist_manifest(struct stream_buffer *buffer);

/*
  Modify the http request for connection header from Keep-Alive to Close to
  avoid pipelining.
*/
void connection_alive_to_close(struct stream_buffer *buffer);
