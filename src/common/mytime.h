#pragma once

#include <sys/time.h>
#include <stdlib.h>

typedef unsigned long mytime_t;

/* return the time since epoch in microseconds:
   Code reference: https://code.google.com/p/lz4/issues/attachmentText?id=39&aid=390001000&name=gettimeofday.patch&token=NCIqhNChcHl0ht7Qtbu8reNw0jA%3A1383498967479
*/
mytime_t microtime(mytime_t *);
