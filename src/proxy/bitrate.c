#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "bitrate.h"
#include "../common/log.h"
#include "../common/buffer.h"
#include "../common/mytime.h"

int calculate_throughput(struct connection_t *conn, int frag_size)
{
        int time_spent;
        double new_throughput;

        time_spent = (conn->stream).t_final - (conn->stream).t_start;

        if (time_spent == 0) {
                time_spent = 1;
        }

        /* multiply 8 to change from bytes to bits */
        /* multiply 1000 to change from per microseconds to per seconds */
        new_throughput = (double)((((double)(frag_size * 8)) /
                                   ((double)time_spent)) * ((double) 1000000));

        return (int)floor(new_throughput);
}

int calculate_moving_average(struct config_t *config, int new_throughput) {
        double cur_throughput;

        cur_throughput = (config->alpha)*((double)new_throughput) +
                (1 - (config->alpha))*(throughput);

        return (int)floor(cur_throughput);
}

int lowest_bitrate()
{
        int lowest_bitrate;
        int i;

        lowest_bitrate = bitrates[0];
        for (i = 0; i < bitrates_count; i++) {
                if (bitrates[i] < lowest_bitrate) {
                        lowest_bitrate = bitrates[i];
                }
        }

        return lowest_bitrate;
}

/*
  Returns the highest bitrate available under the calculated bitrate.
*/
static int highest_bitrate_under(int bitrate)
{
        int highest_bitrate;
        int i;

        highest_bitrate = lowest_bitrate();
        for (i = 0; i < bitrates_count; i++) {
                if (bitrates[i] <= bitrate &&
                    bitrates[i] > highest_bitrate) {
                        highest_bitrate = bitrates[i];
                }
        }

        return highest_bitrate;
}

/*
  Choose the bitrate corresponding to the current throughput calculation.
*/
static char *choose_bitrate()
{
        int bitrate;
        int highest_matched_bitrate;
        char *tmp;

        /* the average throughout should be at least 1.5 times of the bitrate */
        /* so bitrate should be 2/3 of the throughput */
        bitrate = 2 * (throughput / 3);

        /* bitrate is read from manifest file in the unit of Kbps, and it is */
        /* now in bps, so it should be divided by 1000 */
        highest_matched_bitrate = highest_bitrate_under(bitrate / 1000);

        tmp = calloc(BIT_LINE_SIZE, sizeof(char));
        if (tmp == NULL) {
                log(DEFAULT_LOG, "not enough memory to allocate.\n");
                return NULL;
        }

        sprintf(tmp, "%d", highest_matched_bitrate);

        return tmp;
}

int modfiy_bitrate(struct stream_buffer *buffer)
{
        int bitrate_int;
        char *bitrate;
        char *end_loc;
        char *begin_loc;
        char *new_send_buf;
        int first_part_len;
        int last_part_len;

        /* find the end location (exclusive) of the bitrate field in uri */
        end_loc = strstr(buffer->send_buf, "Seg");

        /* find the begin location (inclusive) of the bitrate field in uri */
        begin_loc = strstr(buffer->send_buf, "vod/") + strlen("vod/");

        log(DEFAULT_LOG, "send_buf: (%d) begin_loc:(%d) end_loc:(%d)\n",
            buffer->send_buf, begin_loc, end_loc);

        /* this is the bitrate selected to replace the bitrate in the uri */
        bitrate = choose_bitrate();
        if (bitrate == NULL) {
                log(DEFAULT_LOG, "not enough memory to allocate.\n");
                return 0;
        }
        log(DEFAULT_LOG, "bitrate: (%s)\n", bitrate);

        /* calculate the new send_len */
        first_part_len = begin_loc - (buffer->send_buf);
        last_part_len = buffer->send_len - (end_loc - buffer->send_buf);
        buffer->send_len = first_part_len + strlen(bitrate) + last_part_len;
        log(DEFAULT_LOG, "first_part_len: (%d) last_part_len: (%d)\n", first_part_len,
            last_part_len);

        /* creates the new send_buf */
        new_send_buf = calloc(buffer->send_len, sizeof(char));
        if (new_send_buf == NULL) {
                log(DEFAULT_LOG, "not enough memory to allocate.\n");
                return 0;
        }
        /* copies over first part of the http request */
        memcpy(new_send_buf, buffer->send_buf, first_part_len);
        /* replace the part of the bitrate */
        memcpy(new_send_buf + first_part_len, bitrate, strlen(bitrate));
        /* copes over the last part of the http request */
        memcpy(new_send_buf + first_part_len + strlen(bitrate),
               end_loc, last_part_len);
        free(buffer->send_buf);
        buffer->send_buf = NULL;
        buffer->send_buf = new_send_buf;

        bitrate_int = atoi(bitrate);
        log(DEFAULT_LOG, "modified the bitrate of request to (%s).\n",
            bitrate);
        free(bitrate);
        bitrate = NULL;
        return bitrate_int;
}

void connection_alive_to_close(struct stream_buffer *buffer)
{
        char *close;
        char *end_loc;
        char *begin_loc;
        char *new_send_buf;
        int first_part_len;
        int last_part_len;

        /* find the end location (exclusive) of the connection field */
        end_loc = strstr(buffer->send_buf, "keep-alive") + strlen("keep-alive");

        /* find the begin location (inclusive) of the connection field */
        begin_loc = strstr(buffer->send_buf, "keep-alive");

        close = "close";
        if (end_loc == NULL || begin_loc == NULL) {
                begin_loc = strstr(buffer->send_buf, "Accept:");
                end_loc = begin_loc;
                close = "Connection: close\r\n";
                if (end_loc == NULL || begin_loc == NULL) {
                        return;
                }
        }

        /* calculate the new send_len */
        first_part_len = begin_loc - (buffer->send_buf);
        last_part_len = buffer->send_len - (end_loc - buffer->send_buf);
        buffer->send_len = first_part_len + strlen(close) + last_part_len;

        /* creates the new send_buf */
        new_send_buf = calloc(buffer->send_len, sizeof(char));
        if (new_send_buf == NULL) {
                //fprintf(stderr,"not enough memory to allocate.\n");
                return;
        }
        /* copies over first part of the http request */
        memcpy(new_send_buf, buffer->send_buf, first_part_len);
        /* replace the part of the bitrate */
        memcpy(new_send_buf + first_part_len, close, strlen(close));
        /* copes over the last part of the http request */
        memcpy(new_send_buf + first_part_len + strlen(close),
               end_loc, last_part_len);
        free(buffer->send_buf);
        buffer->send_buf = NULL;
        buffer->send_buf = new_send_buf;
}

void normal_plus_nolist_manifest(struct stream_buffer *buffer)
{
        char *insert_loc;
        char *new_send_buf;
        int first_part_len;
        int last_part_len;
        int old_send_len;

        old_send_len = (buffer->send_len);
        /* find the insert location of "_nolist" in uri */
        insert_loc = strstr(buffer->send_buf, ".f4m");

        /* creates the new send_buf */
        new_send_buf = calloc(old_send_len*2 + strlen("_nolist") +
                              strlen("\r\n\r\n"), sizeof(char));
        if (new_send_buf == NULL) {
                log(DEFAULT_LOG, "not enough memory to allocate.\n");
                return;
        }

        /* calculate the new send_len */
        buffer->send_len = old_send_len*2 + strlen("_nolist") +
		strlen("\r\n\r\n");

        /* copies over first part of the http request */
        first_part_len = insert_loc - (buffer->send_buf);
        memcpy(new_send_buf, buffer->send_buf, first_part_len);
        /* insert the part of "_nolist" */
        memcpy(new_send_buf + first_part_len, "_nolist", strlen("_nolist"));
        /* copes over the last part of the http request */
        last_part_len = old_send_len - first_part_len;
        memcpy(new_send_buf + first_part_len + strlen("_nolist"),
               insert_loc, last_part_len);
        /* copes over the CRLF */
        memcpy(new_send_buf + first_part_len + strlen("_nolist") +
               last_part_len, "\r\n\r\n", strlen("\r\n\r\n"));
        /* copes over the original request */
        memcpy(new_send_buf + first_part_len + strlen("_nolist") +
               last_part_len + strlen("\r\n\r\n"),
               buffer->send_buf, old_send_len);

        assert((unsigned)buffer->send_len == first_part_len + strlen("_nolist")
               + last_part_len + strlen("\r\n\r\n") + old_send_len);

        free(buffer->send_buf);
        buffer->send_buf = NULL;
        buffer->send_buf = new_send_buf;

        log(DEFAULT_LOG, "append nolist manifest request to normal manifest request.\n");
}
