#pragma once

/*
  Functions for parsing DNS update packets
*/

#include <stdlib.h>

#include "../nameserver/nameserver.h"

#define MESSAGE_ID 15441
#define DNS_BUF_SIZE 4096
#define IP_STR_LEN 128
#define DNS_HEADER_LEN (6 * 2)
#define MAX_DOMAIN_NAME_LEN 64
#define DOMAIN_NAME_LEN 18
#define DNS_REQUEST_LEN (DNS_HEADER_LEN + DOMAIN_NAME_LEN + 4)
#define DNS_RESPONSE_LEN (DNS_HEADER_LEN + DOMAIN_NAME_LEN*2 + 4 + 14)

/**
   Generate a dns message.

   @param type 	Type of message : QUERY or RESPONSE
   @param response_ip The ip address for response.

   @return struct pointer to contain the message data, NULL if unsuccessful
*/
struct dns_t *generate_dns_message(uint16_t message_id, enum message_type type,
                                   char *response_ip, int invalid_request);

/**
   Free a dns message struct.

   @param dns_message 	Struct pointer to contain the message data
*/
void free_dns_message(struct dns_t *dns_message);

/**
   Take a dns struct and turn it into a string representation that can
   be sent via UDP (sendto)

   @param dns 	struct to be serialized

   @return buffer pointer to contain serialized struct, NULL if unsuccessful
*/
uint8_t *serialize_dns(struct dns_t *dns_message);

/**
   Turn a string representation of dns struct into an actual struct

   @param buf 	buffer containing the string representation of struct

   @return struct pointer to be filled with deserialized information, NULL
   if unsuccessful
*/
struct dns_t *deserialize_dns(uint8_t *buf);
