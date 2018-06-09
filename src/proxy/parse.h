#pragma once
/*
  Parsing functions associates with input and output of stream
*/

#include "config.h"
#include "connection.h"

#define LINE_SIZE 128

int current_seg_num;
int current_frag_num;
int modified_bitrate;

/*
  Parse the received data.
*/
void parse_data(int recv_socket, struct connection_t *conn,
                struct config_t *config);
