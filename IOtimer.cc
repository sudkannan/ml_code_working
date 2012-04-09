#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "IOtimer.h"

//#define PROFILE

#ifdef GTTHREAD
#include "gtthread.h"
#include "gtthread_spinlocks.h"
struct gt_spinlock_t spinlock_r, spinlock_w;
#endif

struct timeval startw, endw;
struct timeval startr, endr;
int lock_created = 0;
long total_size;

#ifdef GTTHREAD
pthread_mutex_t mutex_r = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_w = PTHREAD_MUTEX_INITIALIZER;

void initialize_timer() {
	
	gt_spinlock_init(&spinlock_r);

}
#endif

void IOtimer_clear() {

	total_read_time = 0;
	total_write_time = 0;
}

/* To calculate simulation time */
long simulation_time(struct timeval start, struct timeval end )
{
	long current_time;

	current_time = ((end.tv_sec * 1000000 + end.tv_usec) -
                	(start.tv_sec*1000000 + start.tv_usec));

	return current_time;
}


size_t _fread ( void * ptr, size_t size, size_t count, FILE * stream ){

	size_t  len = 0;

#ifdef PROFILE
	 gettimeofday(&startr, NULL);
#endif

	 len = fread (ptr, size, count, stream );

#ifdef PROFILE
	 gettimeofday(&endr, NULL);

	total_read_time += simulation_time(startr, endr );
#endif

	return len;

}

char * _fgets( char * str, int num, FILE * stream ) {

	char *ptr = NULL;

#ifdef GTTHREAD
	gt_spin_lock(&spinlock_r);
#endif

#ifdef PROFILE
	 gettimeofday(&startr, NULL);

#endif

 	 ptr = fgets(str, num, stream );

#ifdef PROFILE
	 gettimeofday(&endr, NULL);
	
 	 total_read_time += simulation_time(startr, endr );
#endif

#ifdef GTTHREAD
     gt_spin_unlock(&spinlock_r);
#endif
	

	return ptr;
}

int _fputs ( char * str, FILE * stream ) {


#ifdef GTTHREAD
  	 gt_spin_lock(&spinlock_w);
#endif

#ifdef PROFILE
	 gettimeofday(&startw, NULL);
#endif

 	 fputs(str, stream );

#ifdef PROFILE
	 gettimeofday(&endw, NULL);
	
 	 total_write_time += simulation_time(startw, endw );
#endif

#ifdef GTTHREAD
     gt_spin_unlock(&spinlock_w);
#endif

 	 return 0;
}



size_t _fwrite ( void * ptr, size_t size, size_t count, FILE * stream ){

	
#ifdef PROFILE
	 gettimeofday(&startw, NULL);
#endif

    fwrite (ptr, size, count, stream );

#ifdef PROFILE
	 gettimeofday(&endw, NULL);

	 total_write_time += simulation_time(startw, endw );
#endif

}

ssize_t _write(int fd, void *ptr, size_t size){

     ssize_t bytes = 0;

#ifdef PROFILE
	 gettimeofday(&startw, NULL);
#endif

	 bytes = write(fd, ptr, size); 	

#ifdef PROFILE
	 gettimeofday(&endw, NULL);

	 total_write_time += simulation_time(startw, endw );
#endif
 
	  return bytes;

}


void print_total_write_time(){

#ifdef PROFILE
	fprintf(stderr,"Write time %ld  %ld\n",total_write_time, total_read_time);
#endif
}
