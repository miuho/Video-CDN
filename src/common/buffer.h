#pragma once

#include <unistd.h>
#include <inttypes.h>

#define BUF_SIZE 4096

struct buffer {
        size_t capacity,
                contentLength;
        uint8_t *buf; /* variable length buffer */
};

/* Returns >0 if there's content in the buffer, 0 if there's no content, -1 if
 * there's any error */
int bufferHaveContent(struct buffer *buffer);

/*
  Appends n bytes from the src buffer to the destination buffer struct's
  internal buffer

  returns EXIT_FAILURE is the append failed, EXIT_SUCCESS otherwise
*/
int bufferAppend(struct buffer *dest, uint8_t *src, size_t n);

/*
  Safely delete the buffer buf
*/
void bufferDelete(struct buffer *buf);

/*
  Clear the buffer buf of internal content
*/
void bufferClear(struct buffer *buf);

/*
  return the amount of free space in the buffer's internal buf
*/
size_t bufferFreeSpace(struct buffer *buf);

/*
  remove n bytes of content from buffer buf
*/
void bufferRemoveContent(struct buffer *buf, size_t n);
/*
  A buffer associates a video data transfer
*/

struct stream_buffer {
        char* recv_buf; /* buffer of receiving request or response */
        char* send_buf; /* buffer of sending request or response */
        int recv_len; /* read_buf length have received so far */
        int send_len; /* total length of write_buf to be written */
};
