#pragma once
/*
  Header for logging utility
*/
#define DEFAULT_LOG stderr

#ifdef DEBUG
#define log(stream, ...) \
        do { log_printf(stream, __func__, __LINE__, __VA_ARGS__); } while(0)
#else
#define log(...)
#endif

#define log_activity(stream, ...) \
        do { log_printf(stream, NULL, 0, __VA_ARGS__); } while(0)

#include <stdio.h>

/*
  Open the file for reading or writing

*/
FILE *logSetup(const char *file);

/*
  Output to log file similar to printf family of functions
*/
void log_printf(FILE *stream, const char *func, const int line,
                const char *format, ...);

/*
  Close log file elegantly.
*/
void logClose(FILE **log);
