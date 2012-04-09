#include <stdio.h>
#include <stdlib.h>
#include "hash_map.h"
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "nvmalloc_wrap.h"
#include "IOtimer.h"
#include <sys/time.h>
#include "oswego_malloc.h"

//#define USE_STATS


//extern void* pnv_malloc(size_t, struct rqst_struct *);
//extern void *nv_realloc(void *, size_t);
//extern void *nv_calloc(size_t, size_t);

extern void hash_insert( unsigned long key, size_t val);
extern size_t hash_find( unsigned long key);
extern void hash_delete( unsigned long key);
extern size_t find_hash_total();

size_t print_total_stats() {

	size_t total_alloc =0;

	//total_alloc= find_hash_total();

	fprintf(stderr,"NVmalloc: total %zu\n",total_alloc);
	 return total_alloc;	

}

char *addr = NULL;
size_t val = 0;
size_t total_size = 0;


void *pnvmalloc(size_t size, struct rqst_struct *rqst) {

    int flgPersist = 0;
	struct timeval start, end;
	long total_time; 	

	if(!rqst) {
		perror("failed pnvmalloc \n");
		return NULL;
	}

#ifdef USE_NVMALLOC

//   gettimeofday(&start, NULL);
	fprintf(stderr, "pnv_malloc time %ld \n", total_time);
	addr = (char *)pnv_malloc(size, rqst);

    //  gettimeofday(&end, NULL);
   //total_time =  simulation_time( start, end );
   fprintf(stderr, "pnv_malloc time %ld \n", total_time);
//#endif

#else

#ifdef USE_STATS
	
	gettimeofday(&start, NULL);	

	addr = (char *)malloc(size);

	 gettimeofday(&end, NULL);

	total_time =  simulation_time( start, end );

	fprintf(stderr, "malloc time %ld \n", total_time);

#endif

#endif


#ifdef USE_STATS
    if(!addr){
        fprintf(stderr,"NVmalloc allocation failed \n");
        return NULL;
    }else{
		hash_insert((unsigned long)addr, size);
    }
#endif
    return addr;
}

void *pnvread(size_t size, struct rqst_struct *rqst) {

    int flgPersist = 0;

	if(!rqst) {
		perror("failed pnvmalloc \n");
		return NULL;
	}

#ifdef USE_NVMALLOC

	addr = (char *)pnv_read(size, rqst);
#else
	addr = (char *)malloc(size);
#endif

#ifdef USE_STATS
    if(!addr){
        fprintf(stderr,"NVmalloc allocation failed \n");
        return NULL;
    }else{
		hash_insert((unsigned long)addr, size);
    }
#endif
    return addr;
}




void *nvcalloc(size_t nelemnts, size_t elemnt_sz) {

    int flgPersist = 0;

	total_size += nelemnts* elemnt_sz;
    //++input_id;

	//fprintf(stderr,"%zu\n",total_size );
	//addr = nv_malloc(size);

#ifdef USE_NVMALLOC    
	//addr = (char *)nv_calloc(nelemnts, elemnt_sz);
#else
	addr = (char *)calloc(nelemnts, elemnt_sz);
#endif

    if(!addr){
        fprintf(stderr,"nvcalloc allocation failed \n");
        return NULL;
    }/*else{
		hash_insert((unsigned long)addr, nelemnts* elemnt_sz);
    }*/
	//fprintf(stderr,"returning address %lu \n",(unsigned long)addr);
    return (void *)addr;
}



void *nvrealloc(void *orig_ptr,  size_t size) {

    unsigned long addr;
	void *new_ptr = NULL;

#ifdef USE_NVMALLOC
     //new_ptr = (void *)nv_realloc(orig_ptr, size);
#else
	new_ptr = realloc(orig_ptr, size);	
#endif

     if(new_ptr == NULL){
		fprintf(stderr,"nv_realloc failed \n");
	 }
#ifdef USE_STATS
	addr = (unsigned long)orig_ptr;
    if(!addr){
        fprintf(stderr,"NVmalloc reallocation failed \n");
        return NULL;
    }else{
		if(hash_find(addr)) {

			if( addr == (unsigned long)new_ptr) {

				hash_insert(addr, size);
			}else {
	
				hash_delete(addr);
				hash_insert((unsigned long)new_ptr, size);
			}
		}else{
			hash_insert((unsigned long)new_ptr, size);
		}
	fprintf(stderr,"nvrealloc return %zu \n",size);
	}
#endif

    return new_ptr;
}

void nv_free(void *addr) {

#ifdef USE_NVMALLOC
	//nvfree(addr);
#else
//	free(addr);
#endif
}

int pnvcommit(struct rqst_struct *rqst) {


	//return nv_commit(rqst);
	return 0;


}
