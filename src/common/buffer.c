/*
  Functions for manipuating buffers
*/
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

/*
  Create the internal buffer of the buffer b with size n

  return 0 on success, 1 on failure
*/
static int createBuf(struct buffer *b, size_t n);

/*
  Extend the internal capacity of the buffer buf to contain
  n bytes of *free space*

  return 0 on success, 1 on failure
*/
static int extend(struct buffer *buf, size_t n);

int bufferHaveContent(struct buffer *buffer)
{
        return buffer->contentLength;
}

int bufferAppend(struct buffer *dest, uint8_t *src, size_t n)
{
        if (!dest)
                return EXIT_FAILURE;

        if (!src)
                return EXIT_SUCCESS;

        if (!(dest->buf))
                if (createBuf(dest, n))
                        return EXIT_FAILURE;

        if (bufferFreeSpace(dest) < n) /* not enough free space */
                if (extend(dest, n))
                        return EXIT_FAILURE;

        memcpy(dest->buf + dest->contentLength, src, n);
        dest->contentLength += n;

        return EXIT_SUCCESS;
}

size_t bufferFreeSpace(struct buffer *buf)
{
        return buf->capacity - buf->contentLength;
}

void bufferDelete(struct buffer *buf)
{
        if(!buf)
                return;

        free(buf->buf);
        memset(buf, 0, sizeof(*buf));
}

void bufferClear(struct buffer *buf)
{
        memset(buf->buf, 0, buf->capacity);
        buf->contentLength = 0;
}

void bufferRemoveContent(struct buffer *buf, size_t n)
{
        size_t remain = buf->contentLength - n;

        if (buf->contentLength < n)
                return;

        if (buf->contentLength == n) {
                bufferClear(buf);
                return;
        }

        memmove(buf->buf, buf->buf + n, remain);
        buf->contentLength = remain;
}

static int createBuf(struct buffer *buffer, size_t n)
{
        return (buffer->buf = calloc(n, sizeof(uint8_t))) == NULL;
}

static int extend(struct buffer *buf, size_t n)
{
        size_t total = buf->contentLength + n;
        uint8_t *newBuf = calloc(total, sizeof(uint8_t));

        if (!newBuf)
                return EXIT_FAILURE;

        if (buf->contentLength)
                memcpy(newBuf, buf->buf, buf->contentLength);

        free(buf->buf);

        buf->buf = newBuf;
        buf->capacity = total;

        return EXIT_SUCCESS;
}
