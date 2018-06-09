#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mydnsparse.h"
#include "../common/log.h"

struct dns_t *generate_dns_message(uint16_t message_id, enum message_type type,
                                   char *response_ip, int invalid_request)
{
	struct dns_t *dns_message = calloc(1, sizeof(struct dns_t));

        if (dns_message == NULL) {
                log(DEFAULT_LOG,
                    "failed to calloc memory to generate dns message.\n");
                return NULL;
        }

        dns_message->message_id = message_id;

	dns_message->type = type;

        dns_message->invalid_request = invalid_request;

        if (dns_message->invalid_request) {
                dns_message->len = DNS_HEADER_LEN; /* no body section */
                return dns_message;
        } else if (dns_message->type == QUERY) {
                dns_message->query_name = VID_DOMAIN;
                dns_message->len = DNS_REQUEST_LEN;
                return dns_message;
        } else {
                dns_message->query_name = VID_DOMAIN;
                dns_message->response_name = VID_DOMAIN;
                dns_message->response_ip = response_ip;
                dns_message->len = DNS_RESPONSE_LEN;
                return dns_message;
        }
}

void free_dns_message(struct dns_t *dns_message)
{
        if (dns_message->type == RESPONSE &&
            !dns_message->invalid_request) {
                free(dns_message->response_ip);
                dns_message->response_ip = NULL;
        }

        free(dns_message);
        dns_message = NULL;
}

/**
   Turns string format bytes to labels format bytes.

   @param buffer     buffer location to store the labels format bytes
*/
static void string_to_labels_format(uint8_t *buffer)
{
        char *segment;
        uint8_t seg_len;
        uint8_t *buf_loc;

        /* "video.cs.cmu.edu" needs to be declared as char str[] here */
        char str[] = "video.cs.cmu.edu";
        buf_loc = buffer;
        segment = strtok (str,".");
        while (segment != NULL) {
                /* places the segment length */
                seg_len = strlen(segment);
                *buf_loc = seg_len;
                buf_loc += 1;

                /* places the segment characters */
                memcpy(buf_loc, segment, seg_len);
                buf_loc += seg_len;

                segment = strtok(NULL, ".");
        }
        *buf_loc = 0;
        buf_loc += 1;

        return;
}

/**
   Turns string format bytes to labels format bytes.

   @param buf_loc     buffer location of the labels format bytes
   @param char_buf    buffer location to store the string format bytes

   @return The length of the labels format bytes
*/
static int labels_format_to_string(uint8_t *buf_loc, char *char_buf)
{
        int char_buf_i, i, len, seg_len;

        len = strlen((char *)buf_loc);
        seg_len = 0;
        char_buf_i = 0;
        for (i = 0; i < len; i++) {
                if (seg_len == 0) {
                        seg_len = buf_loc[i];
                        if (i != 0) {
                                char_buf[char_buf_i] = '.';
                                char_buf_i++;
                        }
                } else {
                        seg_len--;
                        char_buf[char_buf_i] = (char)buf_loc[i];
                        char_buf_i++;
                }
        }

        return len + 1;
}

uint8_t *serialize_dns(struct dns_t *dns_message)
{
        uint8_t *buf_loc;
        uint8_t *buf;
        uint8_t domain_name[MAX_DOMAIN_NAME_LEN];
        uint16_t tmp;
        uint16_t id, qdcount, ancount, nscount, arcount, qtype, qclass;
        uint16_t r_type, r_class, rdlength;
        uint32_t r_ttl;
        uint16_t qr, opcode, aa, tc, rd, ra, z, rcode;
        unsigned long ip_address;

        buf = calloc(dns_message->len, sizeof(uint8_t));

        if (buf == NULL) {
            log(DEFAULT_LOG, "failed to calloc buffer to serialize.\n");
            return NULL;
        }

        buf_loc = buf;
        /******************* below is the header section ********************/
        /* put in the id number in network byte order */
        id = htons(dns_message->message_id);
        memcpy(buf_loc, &id, 2);
        buf_loc += 2;

        /* put in QR, OPCODE, AA, TC, RD, RA, Z and RCODE */
        qr = dns_message->type;
        opcode = 0, tc = 0, rd = 0;
        aa = (qr == QUERY) ? 0 : 1;
        ra = 0, z = 0;
        rcode = ((dns_message->invalid_request) ? 3 : 0);
        tmp = htons((qr << 15) | (opcode << 11) | (aa << 10) | (tc << 9) |
                    (rd << 8) | (ra << 7) | (z << 4) | rcode);
        memcpy(buf_loc, &tmp, 2);
        buf_loc += 2;

        /* put in QDCOUNT */
        qdcount = htons(((dns_message->type == QUERY) ||
                         (dns_message->type == RESPONSE &&
                          !(dns_message->invalid_request))) ? 1 : 0);
        memcpy(buf_loc, &qdcount, 2);
        buf_loc += 2;

        /* put in ANCOUNT */
        ancount = htons((dns_message->type == RESPONSE &&
                         !(dns_message->invalid_request)) ? 1 : 0);
        memcpy(buf_loc, &ancount, 2);
        buf_loc += 2;

        /* put in NSCOUNT */
        nscount = 0;
        memcpy(buf_loc, &nscount, 2);
        buf_loc += 2;

        /* put in ARCOUNT */
        arcount = 0;
        memcpy(buf_loc, &arcount, 2);
        buf_loc += 2;

        /******************* below is the body section ********************/
        string_to_labels_format(domain_name);

        if (qdcount) {
                /* put in QNAME */
                memcpy(buf_loc, domain_name, DOMAIN_NAME_LEN);
                buf_loc += DOMAIN_NAME_LEN;

                /* put in QTYPE */
                qtype = htons(1);
                memcpy(buf_loc, &qtype, 2);
                buf_loc += 2;

                /* put in QCLASS */
                qclass = htons(1);
                memcpy(buf_loc, &qclass, 2);
                buf_loc += 2;
        }

        if (ancount) {
                /* put in NAME */
                memcpy(buf_loc, domain_name, DOMAIN_NAME_LEN);
                buf_loc += DOMAIN_NAME_LEN;

                /* put in TYPE */
                r_type = htons(1);
                memcpy(buf_loc, &r_type, 2);
                buf_loc += 2;

                /* put in CLASS */
                r_class = htons(1);
                memcpy(buf_loc, &r_class, 2);
                buf_loc += 2;

                /* put in TTL */
                r_ttl = 0;
                memcpy(buf_loc, &r_ttl, 4);
                buf_loc += 4;

                /* put in RDLENGTH */
                rdlength = htons(4);
                memcpy(buf_loc, &rdlength, 2);
                buf_loc += 2;

                /* put in RDATA */
                inet_pton(AF_INET, dns_message->response_ip, &ip_address);
                memcpy(buf_loc, &ip_address, 4);
                buf_loc += 4;
        }

        if ((buf_loc - buf) != dns_message->len) {
                log(DEFAULT_LOG, "failed to serialize the dns message.\n");
                return NULL;
        }

        return buf;
}

struct dns_t *deserialize_dns(uint8_t *buf)
{
        uint16_t id;
        uint16_t rcode;
        uint8_t* start_buf;
        uint16_t tmp1;
        char *domain_name;
        struct dns_t *dns_message;
        unsigned long ip_address;
        char *tmp;

        start_buf = buf;

        /******************* below is the header section ********************/
        dns_message = calloc(1, sizeof(struct dns_t));

        if (dns_message == NULL) {
            log(DEFAULT_LOG, "failed to calloc struct dns_t for deserialize\n");
            return NULL;
        }

        /* the first two bytes must be the message ID field */
        memcpy(&id, buf, 2);
        buf += 2;
        dns_message->message_id = ntohs(id);

        /* type of packet: query or response */
        memcpy(&tmp1, buf, 2);
        buf += 2;

        dns_message->type = (ntohs(tmp1) >> 15) & 0b1;

        /* get the RCODE */
        rcode = ntohs(tmp1) & 0b1111;
        dns_message->invalid_request = (rcode) ? 1 : 0;
        /* skips OPCODE, AA, TC, RD, RA and Z, not useful info */

        /* skips QDCOUNT, ANCOUNT, NSCOUNT and ARCOUNT, not useful info */
        buf += 8;

        /******************* below is the body section ********************/
        if ((dns_message->type == QUERY) ||
            (dns_message->type == RESPONSE && !(dns_message->invalid_request))) {
                domain_name = calloc(MAX_DOMAIN_NAME_LEN, sizeof(char));
                if (domain_name == NULL) {
                    log(DEFAULT_LOG, "failed to calloc buf for domain name\n");
                    return NULL;
                }

                /* stores QNAME */
                buf += labels_format_to_string(buf, domain_name);
                if (strcmp(VID_DOMAIN, domain_name)) {
                        log(DEFAULT_LOG, "invalid query request\n");
                        dns_message->query_name = domain_name;
                } else {
                        dns_message->query_name = VID_DOMAIN;
                        free(domain_name);
                        domain_name = NULL;
                }

                /* skips QTYPE and QCLASS */
                buf += 4;
        }

        if (dns_message->type == RESPONSE &&
            !(dns_message->invalid_request)) {
                domain_name = calloc(MAX_DOMAIN_NAME_LEN, sizeof(char));
                if (domain_name == NULL) {
                    log(DEFAULT_LOG, "failed to calloc buf for domain name\n");
                    return NULL;
                }

                /* stores NAME */
                buf += labels_format_to_string(buf, domain_name);
                if (strcmp(VID_DOMAIN, domain_name)) {
                        log(DEFAULT_LOG, "invalid query response\n");
                        dns_message->response_name = domain_name;
                } else {
                        dns_message->response_name = VID_DOMAIN;
                        free(domain_name);
                        domain_name = NULL;
                }

                /* skips QTYPE and QCLASS */
                buf += 10;

                /* stores RDATA */
                tmp = calloc(IP_STR_LEN, sizeof(char));
                if (tmp == NULL) {
                        log(DEFAULT_LOG,
                            "failed to calloc buffer for ip address\n");
                        return NULL;
                }
                memcpy(&ip_address, buf, 4);
                inet_ntop(AF_INET, &ip_address, tmp, INET_ADDRSTRLEN);
                dns_message->response_ip = tmp;
                buf += 4;
        }

        dns_message->len = buf - start_buf;

        return dns_message;
}
