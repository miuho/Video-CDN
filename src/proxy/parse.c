#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "parse.h"
#include "stream.h"
#include "bitrate.h"
#include "../common/log.h"
#include "proxy.h"
#include "../common/mytime.h"

/*
  Returns the send_socket to let the proxy know who to send to.
*/
static int get_send_socket(int recv_socket, struct connection_t *conn)
{
        if (recv_socket == (conn->browser).socket) {
                return (conn->server).socket;
        } else {
                return (conn->browser).socket;
        }
}

/*
  Erases the CRLF(s) in the beginning of the received data, and returns 1 if
  a complete header is received, 0 otherwise.
*/
static int complete_header_received(struct stream_buffer *buffer)
{
        //char* first_CRLF;
        char* header_end;

        /* In the interest of robustness, servers SHOULD ignore any empty */
        /* line(s) received where a header is expected, so erase the */
        /* CRLF before the header */
        /*first_CRLF = strstr(buffer->recv_buf, "\r\n");
	  while (buffer->recv_buf == first_CRLF && buffer->recv_len >= 2) {
	  buffer->recv_len -= 2;
	  memmove(buffer->recv_buf, buffer->recv_buf + 2,
	  buffer->recv_len);
	  first_CRLF = strstr(buffer->recv_buf, "\r\n");
	  }*/

        /* the data is full of "\r\n", ignore them, receive more bytes */
        /*if (buffer->recv_len < 2) {
	  return 0;
	  }*/

        /* now its valid to assume that the first 2 bytes is not "\r\n" */
        header_end = strstr(buffer->recv_buf, "\r\n\r\n");
        if (header_end == NULL) {
                /* the header hasnt been fully received yet, wait till next */
                /* recv call and check again. */
                return 0;
        }

        return 1;
}

/*
  Extract the string from begin_loc (inclusive) to end_loc (exclusive)
*/
static char *extract_string(char *begin_loc, char *end_loc)
{
        char *data;
        int h;
        int data_len;

        data = calloc(LINE_SIZE, sizeof(char));
        if (data == NULL) {
                log(DEFAULT_LOG, "not enough memory to allocate.\n");
                return NULL;
        }

        data_len = end_loc - begin_loc;
        for (h = 0; h < data_len; h++) {
                data[h] = begin_loc[h];
        }
        data[h] = '\0';

        return data;
}

/*
  Returns the character between field_name and data_end in the first header, or
  NULL if field_name does not exists in the first header of the buffer.
*/
static char *extract_data_from_header(char *buffer, char *field_name,
                                      char *data_end)
{
        char* field_name_begin;
        char* first_header_end;
        int field_name_len;
        char *field_data;

        /* with the assumption that the complete header has been receieved */
        field_name_begin = strstr(buffer, field_name);
        first_header_end = strstr(buffer, "\r\n\r\n");
        if (field_name_begin == NULL ||
            field_name_begin > first_header_end) {
                /* if content length field does not present, or the first */
                /* appearance of content length field is not in the first */
                /* header, both mean that no body is present in this first */
                /* request or response, so return NULL */
                return NULL;
        } else {
                /* the content length field presents, and it is in the first */
                /* header. Then check if the complete body is received */
                field_name_len = strlen(field_name);
                field_data = extract_string(field_name_begin + field_name_len,
                                            strstr(field_name_begin, data_end));
                if (field_data == NULL) {
                        log(DEFAULT_LOG, "not enough memory to allocate.\n");
                        return NULL;
                }
                //log(DEFAULT_LOG, "field_data: (%s)\n", field_data);

                return field_data;
        }
}

/*
  Returns the length of the first http request or response's body.
*/
static int first_body_length(char *buffer)
{
        char *content_len_data;
        int content_len;

        content_len_data = extract_data_from_header(buffer, "Content-Length: ",
                                                    "\r\n");

        /* if body doesnt present, body length is 0 */
        if (content_len_data == NULL) {
                return 0;
        } else {
                //log(DEFAULT_LOG, "content_len_data: (%s)\n", content_len_data);
                content_len = atoi(content_len_data);
                free(content_len_data);
                content_len_data = NULL;
                return content_len;
        }
}

/*
  Returns the length of the first complete http request or response.
*/
static int first_message_length(char *buffer)
{
        int first_header_len;
        int first_body_len;
        char *first_header_end;

        first_header_end = strstr(buffer, "\r\n\r\n");
        first_header_len = first_header_end - buffer + strlen("\r\n\r\n");
        assert(first_header_len > 0);
        first_body_len = first_body_length(buffer);
        //log(DEFAULT_LOG, "first_header_len: (%d) first_body_len: (%d)\n",
        //    first_header_len, first_body_len);

        return first_header_len + first_body_len;
}

/*
  Returns 0 if message body is incomplete, and returns 1 if a complete body is
  received or if no body is embedded in the message.
*/
static int complete_body_received(struct stream_buffer *buffer)
{
        int complete_message_len;

        complete_message_len = first_message_length(buffer->recv_buf);

        return (buffer->recv_len >= complete_message_len);
}

/*
  Returns 1 if moveing the first complete request or response from recv_buf to
  send_buf is successful, 0 otherwise.
*/
static int move_data_from_recv_to_send(int message_len,
                                       struct stream_buffer *buffer)
{
        /* copies the first message from recv_buf to send_buf */
        memcpy(buffer->send_buf, buffer->recv_buf, message_len);

        buffer->recv_len -= message_len;
        assert(buffer->recv_len >= 0);
        buffer->send_len = message_len;

        /* shift the remaining data in recv_buf to the front */
        memmove(buffer->recv_buf, (buffer->recv_buf) + message_len,
                buffer->recv_len);
        return 1;
}

/*
  Extract the bitrates info from the http response of manifest file.
*/
static void extract_bitrates_from_response(struct stream_buffer *buffer)
{
        char buf_copy[buffer->send_len];
        char *first_bitrate_loc;
        char *field_end_loc;
        char *bitrate_char;

        bitrates_count = 0;
        /* creates a local copy of the response to free to play with it */
        memcpy(buf_copy, buffer->send_buf, buffer->send_len);

        first_bitrate_loc = strstr(buf_copy, "bitrate=\"");
        while (first_bitrate_loc != NULL) {
                field_end_loc = strstr(first_bitrate_loc +
                                       strlen("bitrate=\""), "\"");
                bitrate_char = extract_string((char *)(first_bitrate_loc +
                                                       strlen("bitrate=\"")),
                                              (char *)field_end_loc);
                if (bitrate_char == NULL) {
                        //fprintf(stderr, "not enough memory to allocate.\n");
                        return;
                }
                bitrates[bitrates_count] = atoi(bitrate_char);
                //fprintf(stderr, "read bitrate : (%d).\n", bitrates[bitrates_count]);
                free(bitrate_char);
                bitrate_char = NULL;
                /* change "bitrate=" to "xitrate=" so that next call of */
                /* strstr for "bitrate=" will find the next bitrate */
                first_bitrate_loc[0] = 'x';
                first_bitrate_loc = strstr(buf_copy, "bitrate=\"");
                bitrates_count++;
                assert(bitrates_count <= MAX_BITRATES_NUM);
        }

        /* there should be at least one bitrate available for the video */
        assert(bitrates_count > 0);

}

/*
  Parse the http request.
*/
static void parse_request(struct connection_t *conn,
                          struct stream_buffer *buffer)
{
        char *seg_num;
        char *frag_num;
        char *manifest_request;

        //fprintf(stderr, "1 %s\n",buffer->send_buf);

        connection_alive_to_close(buffer);

        //fprintf(stderr, "1.5 %s\n",buffer->send_buf);

        //fprintf(stderr, "2 %s\n",buffer->send_buf);

        seg_num = extract_data_from_header(buffer->send_buf, "Seg", "-");
        frag_num = extract_data_from_header(buffer->send_buf, "Frag", " ");

        //fprintf(stderr, "3 %s\n",buffer->send_buf);

        /* seg_num and frag_num should present together, not individually */
        assert((seg_num == NULL && frag_num == NULL) ||
               (seg_num != NULL && frag_num != NULL));

        //fprintf(stderr, "4 %s\n",buffer->send_buf);

        manifest_request = strstr(buffer->send_buf, "f4m");
        if (manifest_request != NULL) {
                //fprintf(stderr, "received a manifest request from browser.\n");
                /* if this a http GET request from browser for manifest file, */
                /* it should append the normal version with the nolist */
                /* version, so server always get the both the normal and */
                /* nolist manifest request */
                normal_plus_nolist_manifest(buffer);
        } else if (seg_num != NULL && frag_num != NULL) {
                //fprintf(stderr, "received a fragment request Seg:%s Frag:%s.\n",
                //    seg_num, frag_num);
                current_seg_num = atoi(seg_num);
                current_frag_num = atoi(frag_num);
                free(seg_num);
                seg_num = NULL;
                free(frag_num);
                frag_num = NULL;
                conn->video_next_response = 1;
                /* this is a http GET request for fragments of video chunk */
                /* modify the bitrate of the uri in the request */
                modified_bitrate = modfiy_bitrate(buffer);
        }
        /* if this is http GET request for HTML, SWF or f4m files */
        /* do nothing and simply forwards it to server */
}

/*
  Parse the http response.
*/
static int parse_response(struct connection_t *conn,
			  struct stream_buffer *buffer, struct config_t *config)
{
        int frag_size;
        float duration;
        int new_throughput;
        char chunk_name[LINE_SIZE];
        char *normal_manifest_response;

        normal_manifest_response = strstr(buffer->send_buf, "bitrate=\"500\"");
        if (normal_manifest_response != NULL) {
                if (bitrates_count > 0) {
                        /* parsed normal manifest response before */
                        return 0;
                }
                //fprintf(stderr, "got response for normal manifest request.\n");
                /* set throughput to lowest bitrate in the beginning, and */
                /* extract bitrate from the normal manifest version */
                extract_bitrates_from_response(buffer);
                /* discard the normal manifest response */
                return 0;
        } else if (throughput == 0) {
                /* initialize the throughput to the lowest bitrate */
                throughput = lowest_bitrate();
                //fprintf(stderr, "set first throughput to (%d)\n", lowest_bitrate());
        } else if (conn->video_next_response) {
                /* stop the timestamp for the video fragment */
                frag_size = first_body_length(buffer->send_buf);
                assert(frag_size > 0);

                /* calcualte the moveing average of the throughput */
                new_throughput = calculate_throughput(conn, frag_size);
                throughput = calculate_moving_average(config, new_throughput);
                //fprintf(stderr, "set throughput to (%d)\n", throughput);

                duration = ((conn->stream).t_final - (conn->stream).t_start)/1000000.0;

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

                sprintf(chunk_name, "%dSeg%d-Frag%d", modified_bitrate,
                        current_seg_num, current_frag_num);
                log_activity(config->logFile, LOG_FMT,
                             microtime(NULL) / 1000000,
                             duration,
                             new_throughput/1000, throughput/1000, modified_bitrate,
                             conn->serverIP,
                             chunk_name);

        }
        return 1;
}

void parse_data(int recv_socket, struct connection_t *conn,
                struct config_t *config)
{
        struct stream_buffer *buffer;
        int first_message_len;

        /* parse the request or response buffer */
        if (recv_socket == (conn->browser).socket) {
                buffer = ((conn->stream).request_buffer);
        } else {
                buffer = ((conn->stream).response_buffer);
        }

        if (!complete_header_received(buffer)) {
                //log(DEFAULT_LOG, "incomplete header.\n");
                /* the received data is not ready to be parsed yet */
                return;
        }
        else if (!complete_body_received(buffer)) {
                //log(DEFAULT_LOG, "incomplete body.\n");
                /* the received data is not ready to be parsed yet */
                return;
        }

        first_message_len = first_message_length(buffer->recv_buf);
        buffer->send_buf = calloc(first_message_len, sizeof(char));
        if (buffer->send_buf == NULL) {
                //fprintf(stderr, "not enough memory to allocate.\n");
                return;
        }

        //fprintf(stderr, "%s\n",buffer->recv_buf);

        /* moves the first complete request or response from recv_buf to */
        /* send_buf */
        if (!move_data_from_recv_to_send(first_message_len, buffer)) {
                //fprintf(stderr, "not enough memory to allocate.\n");
                return;
        }
        //fprintf(stderr, "moved complete message from recv_buf to send_buf.\n");

        /* parse the received completed http request or response */
        if (recv_socket == (conn->browser).socket) {
                //fprintf(stderr, "received a complete request from socket %d.\n",
                //    recv_socket);
                /* received a http request */
                parse_request(conn, buffer);

        } else {
                microtime(&((conn->stream).t_final));
                //fprintf(stderr, "received a complete response from socket %d.\n",
                //    recv_socket);
                /* received a http response */
                if (parse_response(conn, buffer, config) == 0) {
                        free(buffer->send_buf);
                        buffer->send_buf = NULL;
                        buffer->send_len = 0;
                        //fprintf(stderr, "cleared stream's send_buf.\n");
                        return;
                }
        }

        //fprintf(stderr, "Proxy: %s\n", buffer->send_buf);
        dump_to_proxy(get_send_socket(recv_socket, conn),
                      (uint8_t *) buffer->send_buf, buffer->send_len);

        //fprintf(stderr, "__________dump_to_proxy called_____\n");

        //fprintf(stderr, "proxy sent: (%s) bytes: (%d)\n", buffer->send_buf, buffer->send_len);

        free(buffer->send_buf);
        buffer->send_buf = NULL;
        buffer->send_len = 0;
        //fprintf(stderr, "cleared stream's send_buf.\n");
        return;
}
