/*
  Utility for logging outputs
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

#include "log.h"

FILE *logSetup(const char *filename) {
        FILE *file = NULL;

        if (filename) {
		file = fopen(filename, "w+");
                if (!file) {
                        log(DEFAULT_LOG, "open log file failed.\n");
			return NULL;
                }
        }

	return file;
}

void log_printf(FILE *stream, const char *func, const int line, const char *format, ...) {
        va_list args;
        va_start(args, format);

        stream = stream ? stream : DEFAULT_LOG;

        if (func != NULL) {
                fprintf(stream, "[%s (%d)]\t", func, line);
        }

        vfprintf(stream, format, args);

        fflush(stream);
        va_end(args);
}

void logClose(FILE **log) {
	if (!log)
		return;

        if (*log && !fclose(*log)) {
                log(DEFAULT_LOG, "close log.\n");
        }

        log = NULL;
}
