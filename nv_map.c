//#define __USE_GNU
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "nv_map.h"
#include "list.h"
#include <sys/mman.h>
#include <strings.h>
#include <time.h>
#include <assert.h>
#include "nv_def.h"
#include <inttypes.h>


//#define NV_DEBUG
int dummy_var = 0;
static int vma_id;
/*List containing all project id's */
static struct list_head proc_objlist;
/*intial process obj list var*/
int proc_list_init = 0;
/*fd for file which contains process obj map*/
static int proc_map;
unsigned long proc_map_start;
int g_file_desc = -1;
//void *map = NULL;
unsigned long tot_bytes =0 ;
struct nvmap_arg_struct nvarg;


#ifdef NVRAM_OPTIMIZE

//optimization variables
struct proc_obj* prev_proc_obj = NULL;
unsigned prev_proc_id;
//optimization variables

#endif


//RBTREE CODE
void IntDest(void* a) {
  free((int*)a);
}



int IntComp(const void* a,const void* b) {
  if( *(int*)a > *(int*)b) return(1);
  if( *(int*)a < *(int*)b) return(-1);
  return(0);
}

void IntPrint(const void* a) {
  printf("%i",*(int*)a);
}

void InfoPrint(void* a) {
  ;
}

void InfoDest(void *a){
  ;
}

//RBtree code ends

int initialize_chunk_tree( struct proc_obj *proc_obj){


	 assert(proc_obj);
	 INIT_LIST_HEAD(&(proc_obj->chunk_list));
	 proc_obj->chunk_initialized = 1;

	 //RB tree code
	 if(!proc_obj->chunk_tree) {
		 proc_obj->chunk_tree =RBTreeCreate(IntComp,IntDest,InfoDest,IntPrint,InfoPrint);
		 assert(proc_obj->chunk_tree);
	 }

}

static struct proc_obj * read_map_from_pmem(int pid);

static char* generate_file_name(char *base_name, int pid, char *dest) {

 int len = strlen(base_name);
 char c_pid[16];

 sprintf(c_pid, "%d", pid);
 memcpy(dest,base_name, len);
 len++;
 strcat(dest, c_pid);
 //fprintf(stderr, "generated file name %s \n", dest);
 return dest;
}

/*creates chunk object.sets object variables to app. value*/
static struct chunk* create_chunk_obj(struct rqst_struct *rqst, unsigned long curr_offset, struct proc_obj* proc_obj) {

	struct chunk *chunk = NULL;
	unsigned long addr = 0;

	addr = proc_map_start;
	//Meta offset indicates offset with respect to metadata
	addr = addr + proc_obj->meta_offset;
#ifdef NV_DEBUG
	fprintf(stderr, "addr %lu  sizeof(chunk) %lu proc_obj->meta_offset %lu \n", addr, sizeof(struct chunk), proc_obj->meta_offset);
#endif
	chunk = (struct chunk*) addr;
	proc_obj->meta_offset += sizeof(struct chunk);

	if(chunk == NULL) {
		fprintf(stderr,"chunk creation failed\n");
		return NULL;
	}

	if(rqst == NULL) {
        fprintf(stderr,"chunk creation failed\n");
        return NULL;
    }

	chunk->vma_id =  rqst->id;
	chunk->length = rqst->bytes;
    chunk->proc_id = rqst->pid;
	//Indicates where in the memory mapped region does the region begin
	chunk->offset = curr_offset;

#ifdef CHCKPT_HPC
    chunk->order_id = rqst->order_id;
#endif

#ifdef NV_DEBUG
	fprintf(stderr, "Setting offset chunk->vma_id %u to %lu  %u\n",chunk->vma_id, chunk->offset, proc_obj->meta_offset);
#endif
	//current offset of process
	return chunk;
}


 int  setup_map_file(char *filepath, unsigned long bytes)
 {
	int result;
        int fd; 

	fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
	if (fd == -1) {
		perror("Error opening file for writing");
		exit(EXIT_FAILURE);
	}

	result = lseek(fd,bytes,  SEEK_SET);
	if (result == -1) {
		close(fd);
		perror("Error calling lseek() to 'stretch' the file");
		exit(EXIT_FAILURE);
	}

	result = write(fd, "", 1);
	if (result != 1) {
		close(fd);
		perror("Error writing last byte of the file");
		exit(EXIT_FAILURE);
	}
	return fd;
}


int gen_rand(void)
{
  int n=0;
  unsigned int iseed = (unsigned int)time(NULL);
  srand (iseed);
 

   n=rand();  
   return(n);
}


// Slight variation on the ETH hashing algorithm
static int MAGIC1 = 1453;
unsigned int generate_vmaid(const char *key) {

	long hash = 0;

	while (*key) {
		hash += ((hash % MAGIC1) + 1) * (unsigned char) *key++;

	}
	return hash % 1699;
}




/*Function to return the process object to which chunk belongs
@ chunk: process to which the chunk belongs
@ return: process object 
 */
struct proc_obj* get_process_obj(struct chunk *chunk) {

	if(!chunk) {
		fprintf(stderr, "get_process_obj: chunk null \n");
		return NULL;
	}
	return chunk->proc_obj;
}

/*Function to find the chunk.
@ process_id: process identifier
@ var:  variable which we are looking for
 */
struct chunk* find_chunk(unsigned int vma_id, struct proc_obj *proc_obj ) {

	struct chunk *t_chunk = NULL;
	struct list_head *pos = NULL;
	//RBtree
	rb_red_blk_node* newNode;

#ifdef NV_DEBUG 
	fprintf(stdout, "find_chunk vma_id:%u \n",vma_id);
	if(proc_obj)
		fprintf(stdout,"total chunks proc_obj->num_chunks:%u \n",proc_obj->num_chunks);
#endif

	if(!proc_obj) {
		fprintf(stdout, "could not identify project id \n");
		return NULL;
	}

	/*if chunk is not yet initialized do so*/
	if (!proc_obj->chunk_initialized){

			initialize_chunk_tree(proc_obj);
	        return NULL;
	 }

	if(proc_obj->chunk_tree) {

		newNode=RBExactQuery(proc_obj->chunk_tree ,&vma_id );

		if(newNode){
			t_chunk = (struct chunk*)newNode->info;
		}else {
			return NULL;
		}
	}

	/*iterate through the process object list to locate the object*/
	/*list_for_each(pos, &proc_obj->chunk_list) {

		if (pos != NULL) {
			t_chunk= list_entry(pos, struct chunk, next_chunk);
			if (t_chunk && t_chunk->vma_id == vma_id) {
				return t_chunk;
			}else{
#ifdef NV_DEBUG
				if(t_chunk){
					fprintf(stderr, "iterating .... t_chunk %u \n", t_chunk->vma_id);
				}else{
					fprintf(stderr, "iterating .... t_chunk is null \n");
				}
#endif
                //FIXME: BUG, list keeps iterating
                if(!t_chunk || t_chunk->vma_id == 0)
					break;	
                 t_chunk = NULL;
			}
		}
	}*/

 #ifdef NV_DEBUG
        if(t_chunk)
       fprintf(stderr, "find_chunk found t_chunk %u \n", t_chunk->vma_id);
#endif

	return t_chunk;
}

/*add the chunk to process object*/
static int add_chunk(struct chunk *chunk, struct proc_obj *proc_obj) {

    if (!chunk)
       return 1;

    if (!proc_obj)
    		return -1;

    if (!proc_obj->chunk_initialized){
    	initialize_chunk_tree(proc_obj);
    }

    //list_add(&(chunk->next_chunk), &(proc_obj->chunk_list));
    //RB tree code
    assert(proc_obj->chunk_tree);
    RBTreeInsert( proc_obj->chunk_tree,&chunk->vma_id,chunk);

    //set the process obj to which chunk belongs 
    chunk->proc_obj = proc_obj;
    return 0;
}

/*Idea is to have a seperate process map file for each process
 But fine for now as using browser */
static struct proc_obj * create_proc_obj(int pid) {

      struct proc_obj *proc_obj = NULL;
      size_t bytes = sizeof(struct proc_obj);
      char file_name[256];
       
     
      bzero(file_name, 256);
      generate_file_name(MAPMETADATA_PATH, pid, file_name);
#ifdef NV_DEBUG
	  fprintf(stderr, "%s metadata file name   \n",file_name);	
#endif
      proc_map = setup_map_file(file_name, METADATA_MAP_SIZE);
      if (proc_map < 1) {
          printf("failed to create a map\n");
          return NULL;
      }

#ifdef NV_DEBUG
        fprintf(stderr, "create_proc_obj:Before mmap \n");
#endif
        proc_obj = (struct proc_obj *) mmap(0, METADATA_MAP_SIZE, PROT_READ | PROT_WRITE,
                        MAP_SHARED, proc_map, 0);

		//proc_obj = malloc(METADATA_MAP_SIZE);
        if (proc_obj == MAP_FAILED) {
                close(proc_map);
                perror("Error mmapping the file");
                exit(EXIT_FAILURE);
        }

         memset ((void *)proc_obj,0,bytes);

        proc_map_start = (unsigned long) proc_obj;

        if (!proc_obj) {
#ifdef NV_DEBUG
          fprintf(stderr, "create_proc_obj:create_proc_obj failed\n");
#endif
          return NULL;
        }
#ifdef NV_DEBUG
        fprintf(stderr, "create_proc_obj:create_proc_obj succeeded\n");
#endif
        return proc_obj;
}



/*Func resposible for locating a process object given
 process id. The idea is that once we have process object
 we can get the remaining chunks allocated process object
 YET TO COMPLETE */
static struct proc_obj *find_proc_obj(int proc_id) {

    struct proc_obj *proc_obj = NULL;
    struct list_head *pos = NULL;

#ifdef NVRAM_OPTIMIZE

	if(proc_id && (proc_id == prev_proc_id)  ) {	
		
		if(prev_proc_obj) {
#ifdef NV_DEBUG
			fprintf(stdout, "returning from cache \n");
#endif
			return prev_proc_obj;
		}
	}
#endif


    if (!proc_list_init) {
#ifdef NV_DEBUG
        fprintf(stderr, "find_proc_obj:proc object tree not yet initialized\n");
#endif
        INIT_LIST_HEAD(&proc_objlist);
        proc_list_init = 1;
        return NULL;
    }

    /*iterate through the process object list to locate the object*/
    list_for_each(pos, &proc_objlist) {

        //printf("Searching procid %d \n", proc_id);
        if (pos != NULL) {
            proc_obj = list_entry(pos, struct proc_obj, next_proc);
            if (proc_obj && proc_obj->pid == proc_id) {
                //printf("\nfind_proc_obj: found proc_obj %d\n", proc_obj->pid);
#ifdef NVRAM_OPTIMIZE		
				prev_proc_id = proc_id;
				prev_proc_obj = proc_obj;
#endif
                return proc_obj;
            }
        }
    }
    return NULL;
}



unsigned long initialized =0;


/*Every NValloc call creates a chunk and each chunk
 is added to process object list*/
static int add_to_process_chunk(struct proc_obj *proc_obj, struct rqst_struct *rqst, unsigned long offset) {

        struct chunk *chunk = NULL;

        chunk = create_chunk_obj( rqst, offset, proc_obj);

        /*add chunk to process object*/
        if (chunk){

              add_chunk(chunk, proc_obj);

              proc_obj->num_chunks++;

			chunk->mmap_id = rqst->mmap_id;	
			 //update the process with number of mmap blocks
            if(chunk->mmap_id > proc_obj->num_mmaps) {
                proc_obj->num_mmaps = chunk->mmap_id;
#ifdef NV_DEBUG
				fprintf(stderr,"updating process mmap num\n");
#endif
            }	
#ifdef NV_DEBUG
		print_chunk(chunk);
#endif
        }
        return 0;
}

/*add process to the list of processes*/
static int add_proc_obj(struct proc_obj *proc_obj) {

        if (!proc_obj)
                return 1;
   
       //if proecess list is not initialized,
       //then intialize it
       if (!proc_list_init) {
    	    INIT_LIST_HEAD(&proc_objlist);
       		proc_list_init = 1;
	    }	

        list_add(&proc_obj->next_proc, &proc_objlist);
#ifdef NV_DEBUG
        fprintf(stderr,"add_proc_obj: proc_obj->pid %d \n", proc_obj->pid);
#endif
        return 0;
}

/*Function to find the process.
@ pid: process identifier
 */
struct proc_obj* find_process(int pid) {

    struct list_head *pos = NULL;
    struct proc_obj* t_proc_obj = NULL;

#ifdef NV_DEBUG 
    fprintf(stderr, "find_process:%u %u\n",pid, proc_list_init);
#endif

    if(!proc_list_init){
    	return NULL;
    }


    /*iterate through the process object list to locate the object*/
    list_for_each(pos, &proc_objlist) {

        if (pos != NULL) {
            t_proc_obj= list_entry(pos, struct proc_obj, next_proc);
            if (t_proc_obj && t_proc_obj->pid == pid) {
                return t_proc_obj;
            }else{
#ifdef NV_DEBUG
                if(t_proc_obj){
                    fprintf(stderr, "iterating .... t_proc_obj %u \n", t_proc_obj->pid);
                }else{
                    fprintf(stderr, "iterating .... t_chunk is null \n");
                }
#endif
                //FIXME: BUG, list keeps iterating
                if(!t_proc_obj || t_proc_obj->pid == 0)
                    break;
                 t_proc_obj = NULL;
            }
        }
    }

 #ifdef NV_DEBUG
        if(t_proc_obj)
       fprintf(stderr, "find_chunk found t_chunk %u \n", t_proc_obj->pid);
#endif

    return t_proc_obj;

}


//Return the starting address of process
unsigned long  get_proc_strtaddress(struct rqst_struct *rqst){

	struct proc_obj *proc_obj=NULL;
    int pid = -1;
    uintptr_t uptrmap;

    pid = rqst->pid;
	proc_obj = find_proc_obj(pid);
    if (!proc_obj) {
		fprintf(stderr,"could not find the process. Check the pid %d\n", pid);
		return 0;	
    }else {
		//found the start address
        uptrmap = (uintptr_t)proc_obj->start_addr;
		return proc_obj->start_addr;
    }
	return 0;
}

//Temporary memory allocation
//CAUTION: returns 0 if success
int nv_initialize(struct rqst_struct *rqst) {

	int pid = -1;
	struct proc_obj *proc_obj=NULL;
	unsigned long offset =0;
	ULONG bytes = 0;
	char *var = NULL;
	char file_name[256];

#ifdef NV_DEBUG
    fprintf(stderr,"Entering nv_initialize\n");
#endif

	bzero(file_name,256);

	if( !rqst ) {
		return NULL;
	}

	bytes = rqst->bytes;
	var = (char *)rqst->var;
	pid = rqst->pid;

    proc_obj = find_proc_obj(pid);
	if(proc_obj) {
#ifdef NV_DEBUG
		fprintf(stderr,"process already exists \n");
#endif
		proc_map_start = (unsigned long)proc_obj;
	}
	if (!proc_obj) {
		proc_obj = create_proc_obj(rqst->pid);

        if(proc_obj) {
			proc_obj->pid = rqst->pid;
#ifdef NV_DEBUG
    	    fprintf(stderr,"nv_initialize: writing to process object \n");
#endif
			proc_obj->size = 0;
			proc_obj->curr_heap_addr = 0;
			proc_obj->num_chunks = 0;
			proc_obj->start_addr = 0;
			proc_obj->offset = 0;
            proc_obj->meta_offset = sizeof(struct proc_obj);
   			add_proc_obj(proc_obj);
#ifdef NV_DEBUG
	        fprintf(stderr,"nv_initialize: finished adding to project \n");
#endif
        }else{
#ifdef NV_DEBUG
			fprintf(stderr,"process object creation failed \n");
#endif
			return NULL; 	
        }
#ifdef NV_DEBUG
		fprintf(stderr,"write_map: created proc obj\n");
#endif
		/*FIX ME: Something wrong*/
		generate_file_name((char *) FILEPATH, rqst->pid, file_name);
		g_file_desc = setup_map_file(file_name, MAX_DATA_SIZE);
        proc_obj->file_desc = g_file_desc;
	}
#ifdef NV_DEBUG
	fprintf(stderr,"proc_obj->offset %ld \n", proc_obj->offset);
	fprintf(stderr,"mmaping again %lu \n",bytes);
#endif

	if (!proc_obj){
		fprintf(stderr,"process object does not exist\n");
		return NULL;
    }
	proc_obj->data_map_size += bytes;

	//For now return 0; 
	//else caller will complain
	return 0;
}


//Temporary memory allocation
//CAUTION: returns 0 if success
void* nv_mmap(struct rqst_struct *rqst) {

	//void *map; /* mmapped array of int's */
	int pid = -1;
	struct proc_obj *proc_obj=NULL;
	unsigned long offset =0;
	ULONG bytes = 0;
	char *var = NULL;
	char file_name[256];
#ifdef NV_DEBUG
    uintptr_t uptrmap;
    uint32_t  int32map;

    fprintf(stderr,"Entering nv_mmap \n");
#endif

	bzero(file_name,256);

	if( !rqst ) {
		return NULL;
	}

	bytes = rqst->bytes;
	var = (char *)rqst->var;
	pid = rqst->pid;

    proc_obj = find_proc_obj(pid);
	if(proc_obj) {
#ifdef NV_DEBUG
		fprintf(stderr,"process already exists \n");
#endif
		proc_map_start = (unsigned long)proc_obj;
	}
   /*if(!proc_obj) {
       
        //looks like we are reading persistent structures and the process is not avaialable in 
		//memory 
        //FIXME: this just addressies one process, since map_read field is global
	    //proc_obj = read_map_from_pmem(pid);
    	if(!proc_obj){
	       printf("getting proc object from pmem failed.create new process\n");
	    }
	}*/
	if (!proc_obj) {
		proc_obj = create_proc_obj(rqst->pid);

        if(proc_obj) {
			proc_obj->pid = rqst->pid;
#ifdef NV_DEBUG
    	    fprintf(stderr,"nv_mmap: writing to process object \n");
#endif
			proc_obj->size = 0;
			proc_obj->curr_heap_addr = 0;
			proc_obj->num_chunks = 0;
			proc_obj->start_addr = 0;
			proc_obj->offset = 0;
            proc_obj->meta_offset = sizeof(struct proc_obj);
        	/*if chunk is not yet initialized do so*/
        	//if (!proc_obj->chunk_initialized){
        	//        INIT_LIST_HEAD(&(proc_obj->chunk_list));
        	//        proc_obj->chunk_initialized = 1;
        	//}
			add_proc_obj(proc_obj);
#ifdef NV_DEBUG
	        fprintf(stderr,"nv_map.c: finished adding to project \n");
#endif
        }else{
#ifdef NV_DEBUG
			fprintf(stderr,"process object creation failed \n");
#endif
			return NULL; 	
        }

#ifdef NV_DEBUG
		fprintf(stderr,"write_map: created proc obj\n");
#endif
		/*FIX ME: Something wrong*/
		generate_file_name((char *) FILEPATH, rqst->pid, file_name);
		g_file_desc = setup_map_file(file_name, MAX_DATA_SIZE);
        proc_obj->file_desc = g_file_desc;
	}

#ifdef NV_DEBUG
	fprintf(stderr,"proc_obj->offset %ld \n", proc_obj->offset);
	fprintf(stderr,"mmaping again %lu \n",bytes);
#endif

	if (!proc_obj){
		fprintf(stderr,"process object does not exist\n");
		return NULL;
    }
	proc_obj->data_map_size += bytes;

	//For now return 0; 
	//else caller will complain

	return (void *)0;
}



/*Function to copy chunks */
static int chunk_copy(struct chunk *dest, struct chunk *src){ 

	if(!dest ) {
		printf("chunk_copy:dest is null \n");
		goto null_err;
	}

	if(!src ) {
		printf("chunk_copy:src is null \n");
		goto null_err;
	}

	//TODO: this is not a great way to copy structures
	dest->vma_id = src->vma_id;
	dest->length = src->length;
	dest->proc_id = src->proc_id;
	dest->offset = src->offset;
    //print_chunk(dest);
    
        
#ifdef CHCKPT_HPC
	//FIXME:operations should be structure
	// BUT how to map them
    dest->order_id = src->order_id;  
	dest->ops = src->ops;
#endif

	return 0;

	null_err:
	return -1;	
}



void print_chunk(struct chunk *chunk) {

     fprintf(stderr,"chunk: vma_id %u\n", chunk->vma_id);
     fprintf(stderr,"chunk: length %ld\n",  chunk->length);
     fprintf(stderr,"chunk: proc_id %d\n", chunk->proc_id); 
     fprintf(stderr,"chunk: offset %ld\n", chunk->offset); 
	 fprintf(stderr,"chunk: mmap id %u \n", chunk->mmap_id);
#ifdef CHCKPT_HPC
     fprintf(stderr,"chunk: order_id %ld\n",chunk->order_id);
#endif
}



/*Function to update the queue/log
Called by holding spin lock
avoid calling to function recursively*/
int update_queue(struct queue *l_queue, struct chunk *chunk) {

	unsigned long addr=0;
	struct chunk *l_chunk = NULL;

	//update the log
	if(!l_queue){
		printf("l_queue is null\n");
		goto error;
	}

	l_queue->num_chunks++;

	//Get the offset to write 
	//add it to the starting address
	addr = (unsigned long)l_queue  + l_queue->offset;
	l_chunk = (struct chunk *)addr;

	//intialize a new chunk
	//FIXME: do we need two copies of chunk, one?
	//in process metadata and another here
	if (chunk_copy(	l_chunk, chunk ) == -1){
		printf("chunk copy failed \n");
		goto error;
	}

	//update queue offset
	l_queue->offset += sizeof(struct chunk);

#ifdef NV_DEBUG
    print_chunk(l_chunk);
#endif

	//testing purpose
	/*iterate through the process object list to locate the object*/
	//list_for_each(pos, &l_queue->lchunk_list) {

 /*    addr = (unsigned long)l_queue;
     addr = addr + sizeof(struct queue);      
     for (index =0; index  < l_queue->num_chunks; index++) {
        temp  = (struct chunk *)addr;
        addr +=  sizeof(struct chunk);
			if (temp) {
#ifdef NV_DEBUG
			fprintf(stderr,"update_queue: chunk->vma_id %d\n", temp->vma_id);
#endif	
			}
    }*/
	return 0;

	error:
	return -1;
}

void* create_queue() {

        key_t key;
        int shmid;
        void *shm;

        key = 2078;

        fprintf(stderr,"creating queue of size %d\n", SHMSZ);

        if ((shmid = shmget(key,SHMSZ, 0666 | IPC_CREAT)) < 0) {
                //perror("shmget");
                return NULL;
        }

        if ((shm = shmat(shmid, (void *)0, 0)) == (void *)-1) {
                //perror("shmat");
                return NULL;
        }

        /*queue already exists*/
        return shm;
}



void* check_if_init() {

	key_t key;
	int shmid;
	void *shm;

	key = 2078;

	/*fd = open("test", O_RDWR);

	if (fd == -1) {
		perror("Error opening file for reading");
		return NULL;
	}*/

	if ((shmid = shmget(key, SHMSZ, 0666)) < 0) {
		perror("shmget");
		return NULL;
	}

	if ((shm = shmat(shmid, (void *)0, 0)) == (void *)-1) {
		perror("shmat");
		return NULL;
	}

	/*queue already exists*/
	return shm;
}

int nv_commit(struct rqst_struct *rqst) {

	int process_id = -1;
	unsigned long addr = 0;
	int size = 0;
	int ops = -1;
	char *var = NULL;
	void *src = NULL;
	struct proc_obj *proc_obj= NULL;   
    unsigned int vma_id = 0;
    struct chunk *chunk_ptr= NULL;

	if(!rqst)
		return -1;

	var = (char *)rqst->var;
	size = rqst->bytes;
	process_id = rqst->pid;
	ops = rqst->ops;
	src = rqst->src;
	proc_obj= find_proc_obj(process_id);



#ifdef NV_DEBUG
		fprintf(stderr, "nv_commit: finding chunk \n");
#endif
	//find the chunk
    //if application has supplied request id, neglect
    if(!rqst->id) {
      if(rqst->var) 
	      vma_id = generate_vmaid((const char*)rqst->var);
      else
		printf("nv_commit:error generating vma id \n");
    } 
    else
      vma_id = rqst->id;
 
	chunk_ptr = find_chunk( vma_id, proc_obj );
	if(!chunk_ptr) {
		printf("nv_commit:finding chunk failed %d \n", vma_id);
		goto error;
	}
	chunk_ptr->length = rqst->bytes; 

#ifdef CHCKPT_HPC
	chunk_ptr->ops = ops;
	chunk_ptr->order_id = rqst->order_id;
#endif


	if(!src){
		printf( "nv_commit:dram src pointer is null \n");
		goto error;
	}

	//found the chunk ptr
	if (size <= 0 ) {
		printf( "nv_commit:very few bytes to copy \n");
		goto error;
	}

#ifdef NV_DEBUG
	fprintf(stderr, "nv_commit:getting chunk start address \n");
#endif

	//get the chunk starting address
	if ( !chunk_ptr ||  !(chunk_ptr->proc_obj)) {
		printf("nv_commit: could not locate process obj or chunk\n");
		goto error;
	}

    addr = rqst->mem;

   //if(initialize_sema){
	//    while(gt_decr_sema());
  // }
   memcpy((void *) addr, src, size);

#ifdef NV_DEBUG
    //fprintf(stderr, "addr %ld, src %ld size %ld \n", addr, src, size);
#endif

   /*if(initialize_sema){

        gt_incr_sema();
   }*/

  /* if(!l_queue) {
	l_queue = check_if_init();
	if(!l_queue)
    fprintf(stderr, "nv_commit:l_queue is null \n");
   }*/	
 
  //fprintf(stderr, "nv_commit: before spin lock %d \n",l_queue->lock.locked);
#ifdef ENABLE_LOCK
	//gt_spin_lock(&l_queue->lock);
#endif

	update_queue(l_queue, chunk_ptr);

#ifdef ENABLE_LOCK
	 //gt_spin_unlock(&l_queue->lock);

#endif

#ifdef NV_DEBUG
   printf("nv_commit: after spin lock %d \n",  chunk_ptr->proc_obj->pid); 
#endif

	return 0;

	error:
	return -1;
}

/*gives the offset from start address
 * @params: start_addr start address of process
 * @params: curr_addr  curr_addr address of process
 * @return: offset
 */
ULONG findoffset(UINT proc_id, ULONG curr_addr) {

	struct proc_obj *proc_obj = NULL;
	ULONG diff = 0;

	proc_obj = find_proc_obj(proc_id);
	if (proc_obj) {
		diff = curr_addr - (ULONG)proc_obj->start_addr;
		return diff;
	}
    return 0;
}

/* update offset for a chunk relative to start address
 * @params: proc_id
 * @params: vma_id
 * @params: offset
 * @return: 0 if success, -1 on failure
 */
int update_offset(UINT proc_id, ULONG offset, struct rqst_struct *rqst) {

	struct proc_obj *proc_obj;
	struct chunk* chunk = NULL;
	long vma_id = 0;


	proc_obj = find_proc_obj(proc_id);
	
	if (proc_obj) {

		//find the chunk using vma id
       //if application has supplied request id, neglect
       if(!rqst->id) {     
    	   if(rqst->var){ 
				vma_id = generate_vmaid((const char*)rqst->var);
            }else{
				printf("nv_commit:error generating vma id \n");
                goto error;  
 			}
       }
       else{
		   vma_id = rqst->id;
       }

	
		//chunk = find_chunk(vma_id, proc_obj);
		if (!chunk) {
#ifdef NV_DEBUG
			fprintf(stdout,"rqst->mmap_id %d \n",rqst->mmap_id);
#endif
			//add a new chunk and update to process
			add_to_process_chunk(proc_obj, rqst, offset);
        } 
		else{
			//update the chunk id
		chunk->offset = offset;
#ifdef NV_DEBUG
         fprintf(stderr,"update_offset %ld \n", chunk->offset);
#endif
        
		}
		return SUCCESS;
	} else {
		return FAILURE;
	}
error:
	 return FAILURE;
  
}


/*if not process with such ID is created then
we return 0, else number of mapped blocks */
int get_proc_num_maps(int pid) {

	 struct proc_obj *proc_obj = NULL;

	proc_obj = find_proc_obj(pid);

	if(!proc_obj) {
	
		fprintf(stderr,"process not created \n");
		return 0;
	}
	else {
	    //also update the number of mmap blocks
    	return proc_obj->num_mmaps;
	}
	return 0;
}


static struct proc_obj * read_map_from_pmem(int pid) {

	struct proc_obj *proc_obj = NULL;
	size_t bytes = 0;
	int fd = -1;
	void *map;
	int idx = 0;
	ULONG addr = 0;
	struct chunk *chunk;
	char file_name[256];

	bzero(file_name,256);


	bytes = sizeof(struct proc_obj);

	generate_file_name((char *) MAPMETADATA_PATH, pid, file_name);

	fd = open(file_name, O_RDWR);

#ifdef NV_DEBUG
     fprintf(stderr, "entering nv_map_read %s\n",file_name);
#endif

	if (fd == -1) {
		perror("Error opening file for reading");
		return NULL;
	}


	map = (struct proc_obj *) mmap(0, METADATA_MAP_SIZE,
			PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	//Extract the base process object
	proc_obj = (struct proc_obj *) map;
	if (proc_obj == MAP_FAILED) {
		close(fd);
		perror("Error mmapping the file");
		return NULL;
	}
	 fprintf(stderr, "proc_obj->num_chunks %d \n", proc_obj->num_chunks);

	bytes = sizeof(struct proc_obj);
	//Start reading the chunks
	addr = (unsigned long) proc_obj;
	addr = addr + bytes;
	proc_obj->chunk_initialized = 0;


	 //add the process to proc_obj tree
     add_proc_obj(proc_obj);

	//initialize the chunk list
	 /*if chunk is not yet initialized do so*/
    if (!proc_obj->chunk_initialized){
            initialize_chunk_tree(proc_obj);
     }


#ifdef NV_DEBUG
	fprintf(stderr,"proc_obj->pid %d \n", proc_obj->pid);
	fprintf(stderr,"proc_obj->size %lu \n",proc_obj->size);
	fprintf(stderr,"proc_obj->curr_heap_addr %lu \n",proc_obj->curr_heap_addr);
	fprintf(stderr,"proc_obj->num_chunks %d\n", proc_obj->num_chunks);
	fprintf(stderr,"proc_obj->start_addr %lu\n", proc_obj->start_addr);
#endif


	//Read all the chunksa
	for (idx = 0; idx < proc_obj->num_chunks; idx++) {
		chunk = (struct chunk*) addr;

		 fprintf(stderr,"proc_obj->num_chunks % lu \n", proc_obj->num_chunks);

#ifdef NV_DEBUG
         print_chunk(chunk);
#endif

		//Add chunks to process
		add_chunk(chunk, proc_obj);
		addr = addr + sizeof(struct chunk);
	}

	return proc_obj;
}


void *map_read = NULL;


//This function just maps the address space corresponding
//to process.
void *map_process(struct rqst_struct *rqst) {

  char file_name[256];
  int fd = -1; 
  void *nvmap = NULL;
  char *deleteme;	

  bzero(file_name, 256);
  //TODO: Find a way to convert process id  to  file
  generate_file_name(FILEPATH, rqst->pid , file_name);

  fd = open(file_name, O_RDWR);
  if (fd == -1) {
      fprintf(stderr,"file_name %s\n",file_name);
      perror("Error opening file for reading");
      //exit(EXIT_FAILURE);
     goto error;
  }
  if(fd == -1) {
     fprintf(stderr,"cannot read map file \n");
     goto error;
  }
 
  nvarg.chunk_id = rqst->mmap_id;
  nvarg.fd = fd;
  nvarg.proc_id = rqst->pid;
  nvarg.offset = 0;
  nvarg.pflags = 1;
  nvarg.ref_count = 1;

   //nvmap = mmap(0, NVRAM_DATASZ, PROT_NV_RW, MAP_PRIVATE, fd, 0);
   nvmap  = (char *)syscall(__NR_nv_mmap_pgoff,0 ,NVRAM_DATASZ,  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, &nvarg);
   if (nvmap == MAP_FAILED) {
       close(fd);
       goto error;
   }

   return nvmap;

error:
    return NULL;
}

void* nv_map_read(struct rqst_struct *rqst, void* map ) {

    unsigned long offset = 0;
    int process_id = 1;
    struct proc_obj *proc_obj = NULL;
    unsigned int vma_id;
    struct chunk *chunk_ptr = NULL;

    process_id = rqst->pid;


   //Check if all the process objects are still in memory and we are not reading
   //process for the first time
   proc_obj = find_process(rqst->pid);

   if(!proc_obj) {
       
        //looks like we are reading persistent structures and the process is not avaialable in 
		//memory
	    //FIXME: this just addressies one process, since map_read field is global
	    proc_obj = read_map_from_pmem(process_id);
    	if(!proc_obj){
	       printf("getting proc object from pmem failed\n");
    	   goto error;
	    }
	}
    //find the chunk   
    if(!rqst->id)
      vma_id = generate_vmaid((const char*)rqst->var);
    else
     vma_id = rqst->id;
 
    chunk_ptr = find_chunk( vma_id, proc_obj );
    if(!chunk_ptr) {
        fprintf(stderr, "finding chunk failed \n");
		goto error;        
    }

     if(chunk_ptr->proc_obj->start_addr == 0) {
         chunk_ptr->proc_obj->start_addr = (unsigned long)map_read;
     }

     rqst->mmap_id = chunk_ptr->mmap_id;
     rqst->id = chunk_ptr->vma_id;
     rqst->pid = chunk_ptr->proc_id;

     map_read = map_process(rqst);
     if(!map_read){
    	 fprintf(stderr, "nv_map_read: map_process returned null \n");
    	 goto error;
     }

     //Get the the start address and then end address of chunk
    offset = chunk_ptr->offset; /*Every malloc call will lead to a chunk creation*/
    rqst->mem = (unsigned long)((unsigned long)map_read + offset);
#ifdef NV_DEBUG
     fprintf(stderr, "nv_map_read: chunk offset %lu %lu %lu \n", offset, (unsigned long)map_read, rqst->mem);
#endif

   return (void *)rqst->mem;
error:
    return NULL;

}

int nv_munmap(void *addr){

    int ret_val = 0;

	if(!addr) {
		perror("null address \n");
		return -1;
	}

   ret_val = munmap(addr, MAX_DATA_SIZE);

   return ret_val;
   
}


