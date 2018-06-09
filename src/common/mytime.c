#include <stdio.h>
#include "mytime.h"

mytime_t microtime(mytime_t *time) {
	struct timeval tv;
	mytime_t nCount;

	if (gettimeofday(&tv, NULL) == -1) {
		perror("gettimeofday");
		return EXIT_FAILURE;
	}

	nCount = (mytime_t) (tv.tv_usec + tv.tv_sec * 1000000);

	if (time != NULL) {
		*time = nCount;
	}

	return nCount;
}
