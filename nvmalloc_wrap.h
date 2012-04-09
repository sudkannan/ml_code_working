

#include <stdio.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif

#define PROC_ID 6000
#define CHUNK_ID 1
#define MAXSIZE 200 * 1024 * 1024

#define USE_NVMALLOC
//#define ALLOCATE
//#define FASTA_DELETE_ME

#define USE_BENCHMARKING

#define OUTFILE "fasta_output"
#define REV_INFILE "fasta_output"
#define REV_OUTFILE "revcomp_output"

//#define OUTFILE "/tmp/ramsud/fasta_output"
//#define REV_INFILE "/tmp/ramsud/fasta_output"
//#define REV_OUTFILE "/tmp/ramsud/revcomp_output"

/*struct rqst_struct {
    size_t bytes;
    const char* var;
    //unique id on how application wants to identify 
    //this chunk;
    int id;
    int pid;

    int ops;
    void *src;
    unsigned long mem;
    unsigned int order_id;

    //volatile flag
    int isVolatile;
    unsigned int mmap_id;
    unsigned long mmap_straddr;
};

struct nvmap_arg_struct{

    unsigned long fd;
    unsigned long offset;
    int chunk_id;
    int proc_id;
    int pflags;
};*/



void *allocate_mem(size_t size);
void* allocate_nvmem(size_t mapsize);
void nv_free(void *ptr);
size_t print_total_stats();
void *nvrealloc(void *, size_t);
void *pnvmalloc(size_t size, struct rqst_struct *rqst);
void *pnvread(size_t bytes, struct rqst_struct *rqst);
int  pnvcommit(struct rqst_struct *rqst);
#ifdef __cplusplus
}

/* Wrapper to check for errors */
#define CHECK_ERR(a)                                       \
   if ((a))                                                  \
   {                                                         \
      perror("Error at line\n\t" #a "\nSystem Msg");         \
      exit(1);                                               \
   }



#endif


