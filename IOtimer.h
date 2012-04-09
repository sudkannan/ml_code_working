#ifndef IOtimer_H_
#define IOtimer_H_



#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif



size_t _fwrite (void * ptr, size_t size, size_t count, FILE * stream );
size_t _fread ( void * ptr, size_t size, size_t count, FILE * stream );
ssize_t _write(int fd, void *ptr, size_t size);
void IOtimer_clear();

void print_total_write_time();

static long total_write_time;
static long total_read_time;

#ifdef __cplusplus
};
#endif


#endif
