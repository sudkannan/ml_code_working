// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A basic PPAPI-based hello world. Used to measure the size and startup
// time of a really small program.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdarg.h>
#include "ptmalloc.h"
#include "dbacl.h"
#include "IOtimer.h"
#include "nv_map.h"
#include "nvmalloc_wrap.h"

#ifdef NACL
#include <string>
#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#endif

using namespace std;

#define REGION_SIZE 65536
//Number of documen to learn initially
#define NO_OF_INIT_LEARN_DOCS 2
//Max file path len
#define FILE_PATH_LEN 256
#define LEARN_FILE1 "22.txt"

//indicates list of mapped documents 
int *map_list;
extern int maplist[256];
extern int temp_file;
extern int online_fd;


//list of training data file pointers
//contains address of file pointers
extern struct MAPLIST maplist_arr[256];

size_t region_size = REGION_SIZE * 25;
size_t input_size = 1 * region_size;
size_t output_size = 2 * region_size;
size_t temp_size = 100 * region_size;
size_t online_size = 100 * region_size;

//stats
#ifdef STATS
unsigned int nv_alloc_bytes = 0;
unsigned long learner_write_bytes =0;
unsigned long learner_read_bytes = 0;
long glob_read_time = 0;
struct timeval start_learn_time;
struct timeval end_learn_time;

struct timeval start_read_time;
struct timeval end_read_time;

long tot_learn_time = 0;
unsigned int hash_tokens =0;

struct timeval strt_classify, end_classify;
#endif

int main_test(std::string message);
void print_stats();

int create_learning_args(int idx);
int create_classify_args(int num_categories);
#ifdef NACL
int classify_data(std::string message, int num_categories);
#else
int classify_data(int num_categories);
#endif

void LOG( FILE *fp,  const char* format, ... );
#ifdef NACL

namespace {
// The expected string sent by the browser.
const char* const kHelloString = "hello";

// The string sent back to the browser upon receipt of a message
// containing "hello".
const char* const kReplyString = "hello from NaCl";
}  // namespace

namespace hello_world {
class HelloWorldInstance : public pp::Instance {
public:
	explicit HelloWorldInstance(PP_Instance instance) : pp::Instance(instance) {}
	virtual ~HelloWorldInstance() {}
	virtual void HandleMessage(const pp::Var& var_message);
};

void HelloWorldInstance::HandleMessage(const pp::Var& var_message) {
	if (!var_message.is_string()) {
		return;
	}
	std::string message = var_message.AsString();
	pp::Var var_reply;
	int cntr = 0;

	//initialize a map list counter
	if(!map_list) {
		map_list = (int *)malloc(NO_OF_INIT_LEARN_DOCS * sizeof(int));
		if(!map_list) {
			LOG(stderr,"map list allocation failed \n");
			return;
		}
	}

	//All map list are set to -1;
	for(cntr = 0; cntr < NO_OF_INIT_LEARN_DOCS; cntr++) {
		map_list[cntr] = -1;
	}


	for(cntr = 0; cntr < NO_OF_INIT_LEARN_DOCS; cntr++) {

		//we learn from all the doucuments first
		//FIXME: Same message getting passed again and again
		//map_list[cntr] =learn_data((char *)message.c_str(), message.size());

	}

	//start();
	main_test(message);

	//if (message == kHelloString)
	var_reply = pp::Var(kReplyString);
	PostMessage(var_reply);
}

class HelloWorldModule : public pp::Module {
public:
	HelloWorldModule() : pp::Module() {}
	virtual ~HelloWorldModule() {}

	virtual pp::Instance* CreateInstance(PP_Instance instance) {
		return new HelloWorldInstance(instance);
	}
};
}  // namespace hello_world

namespace pp {
/// Factory function called by the browser when the module is first loaded.
/// The browser keeps a singleton of this module.  It calls the
/// CreateInstance() method on the object you return to make instances.  There
/// is one instance per <embed> tag on the page.  This is the main binding
/// point for your NaCl module with the browser.
/// @return new HelloWorldModule.
/// @note The browser is responsible for deleting returned @a Module.
Module* CreateModule() {
	return new hello_world::HelloWorldModule();
}
} 

#endif


#ifdef STANDALONE
/* To calculate simulation time */
long simulation_time(struct timeval start, struct timeval end )
{
    long current_time;

    current_time = ((end.tv_sec * 1000000 + end.tv_usec) -
                    (start.tv_sec*1000000 + start.tv_usec));

    return current_time;
}

#endif

void LOG( FILE *fp,  const char* format, ... ) {
        va_list args;
        va_start( args, format );
//#ifdef DEBUG
        vfprintf( fp, format, args );
//#endif
        va_end( args );
}


int setup_map_file1(char *filepath, unsigned long bytes) {

	int result;
	int fd = -1;

	fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
	if (fd == -1) {
		perror("Error opening file for writing");
		exit(EXIT_FAILURE);
	}

	result = lseek(fd,bytes, SEEK_SET);
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



//Method to generate random data
int generate_random_text( char *addr, unsigned long len, unsigned long  num_words  ) {

	unsigned long idxa =0, idxb =0;
	unsigned long cntr =0;
	int wordsize;
	int maxwordsize = 0 ;
	int idxc =0;

	maxwordsize = len / num_words;

	static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	for (idxa = 0; idxa < num_words; idxa++) {

		wordsize = rand()%  maxwordsize;

		for (idxb = 0; idxb < wordsize; idxb++) {

			if( cntr >= len) break;

			idxc = rand()% (sizeof(alphanum)/sizeof(alphanum[0]) - 1); 
			addr[cntr] = alphanum[idxc];
			cntr++;
		}
		/*if(idxa%10 ==0)
		addr[cntr] = '\n';
		else*/
		addr[cntr] = ' ';
		cntr++;
	}
	return cntr;

}

//Method to generate random filename
void gen_random(char *s, const int len) {
	static const char alphanum[] =
			"abcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < len; ++i) {
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	s[len] = 0;
}


int get_base_out_rqstid(){
	return 100;
}

int get_tid(){
	return 1;
}

int get_outsize(){

	return output_size;
}


#ifndef NACL

int imc_mem_obj_create(size_t mapsize){

	char s[10];
	int len = 9;
	int fd = -1;
	char fullpath[FILE_PATH_LEN ];

	gen_random(s,  len);
	bzero(fullpath, FILE_PATH_LEN );
	strcpy(fullpath,"/tmp/");
	strcat(fullpath, s);
	fd=  setup_map_file1(fullpath, mapsize);
	return fd;
}
#endif

int chunk_no = 1;
int proc_no = PROC_ID;

void* allocate_nvmem(size_t mapsize){

   void *nv_ptr = NULL;

#ifdef USE_NVMALLOC
    int offset = 0;
    struct rqst_struct rqst;
    
    rqst.bytes = mapsize;
    rqst.pid = proc_no;
    rqst.id = chunk_no;

    CHECK_ERR((nv_ptr = (void *)pnvmalloc(rqst.bytes + 1, &rqst)) == NULL);
    //CHECK_ERR((nv_ptr = (void *)malloc(rqst.bytes + 1)) == NULL);
	chunk_no++;
#endif
	return nv_ptr;

}

#ifdef STATS
void print_stats() {

	fprintf(stdout, "nvmalloc total : %u\n", nv_alloc_bytes);
	fprintf(stdout, "learner bytes written : %u\n", learner_write_bytes);
	fprintf(stdout, "learner bytes read : %u\n", learner_read_bytes);
	fprintf(stdout," hash_tokens : %u \n",hash_tokens);
	//fprintf(stdout, "time %ld \n", simulation_time(strt_classify, end_classify));
	fprintf(stdout, "global read time: %ld \n", glob_read_time);
	fprintf(stdout,"total learn time : %ld \n", tot_learn_time);
}
#endif

int argc =0;
char **argv;

int create_learning_args(int idx) {

#ifdef NVRAM
	argv = (char **)nvmalloc(sizeof(char *) * 7);
#else
	argv = (char **)malloc(sizeof(char *) * 4);
#endif	
	argv[0] = (char *)nvmalloc(sizeof(char) * 56);
	argv[1] = (char *)nvmalloc(sizeof(char) * 56);
	argv[2] = (char *)nvmalloc(sizeof(char) * 56);
	argv[3] = (char *)nvmalloc(sizeof(char) * 56);
#ifdef NVRAM
	argv[4] = (char *)nvmalloc(sizeof(char) * 56);
	argv[5] = (char *)nvmalloc(sizeof(char) * 56);
	argv[6] = (char *)nvmalloc(sizeof(char) * 56);
	argv[7] = NULL;
#else
	argv[4] = NULL;
#endif

	strcpy(argv[0],(char*)"./dbacl");
	strcpy(argv[1],( char*)"-l");
	sprintf(argv[2],"%d",idx);
	strcat(argv[2],"_out");

	sprintf(argv[3],"%d",idx);
	strcat( argv[3],".txt");

#ifdef NVRAM
	strcpy(argv[4],(char*) "-m");
	strcpy(argv[5],( char*) "-o");
	sprintf(argv[6],"%d",idx);
	strcat( argv[6],"_online");
	argc   = 7;
#else
	argc   = 4;
#endif
}





int create_classify_args(int num_categories) {

	int cntr =0;
	int idx =0;
	void *mmap_ptr = NULL;
	int fd = 0;
	struct stat buf;
	int data_sz = 2 * 25 * 65536;
	struct rqst_struct rqst;

	chunk_no = 1;

	rqst.bytes = data_sz;
	rqst.pid = proc_no;
	rqst.id = chunk_no;


	argv = (char **)malloc(sizeof(char *) * (num_categories * 2 + 6) );
	for ( idx =0; idx < (num_categories * 2 + 6); idx++)
		argv[idx] = (char *)malloc(sizeof(char) * 256);

	strcpy(argv[0],(char*)"./dbacl");
	cntr = 1;

	for ( idx =1; idx < (num_categories * 2) ; idx++){

		strcpy(argv[idx],"-c");
		idx++;
		sprintf(argv[idx],"%d",cntr);
		strcat(argv[idx],"_out.tmp.0");

		char filename[FILE_PATH_LEN ];
		bzero(filename,FILE_PATH_LEN );

		sprintf(filename,"%d",cntr);
		strcat(filename, "_out");


#ifdef USE_NVMALLOC
		CHECK_ERR((mmap_ptr = (char *)pnvread(data_sz + 1, &rqst)) == NULL);
		rqst.id++;
		maplist_arr[cntr-1].dataptr = (unsigned long)mmap_ptr;
		maplist_arr[cntr-1].datasize = rqst.bytes;
#ifdef DEBUG
		LOG(stderr,"nvread mmap_ptr %s \n", mmap_ptr);
#endif

#else	

#ifdef NVRAM
		FILE *tmp_fp;
		tmp_fp = fopen(filename,"r");
		if(!tmp_fp) {
			LOG(stderr,"failed to open file pointer \n");
			exit(-1);
		}

		fd = fileno(tmp_fp);
		fstat(fd, &buf);

		//LOG(stderr,"input size %d \n", buf.st_size);
		mmap_ptr = mmap(0,  buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		maplist_arr[cntr-1].dataptr = (unsigned long)tmp_fp;
#endif
				
   		#endif
		cntr++;
	}
	strcpy(argv[idx++],(char*) LEARN_FILE1);
	strcpy(argv[idx++],(char*) "-m");
	strcpy(argv[idx++],( char*)"-v");
	strcpy(argv[idx++],( char*)"-o");
	strcpy(argv[idx++],( char*)"three_online");
	argv[idx]= NULL;
	argc   = idx;  //(int)(sizeof(argv) / sizeof(argv[0])) - 1;
#ifdef DEBUG
	LOG(stderr," total arguments %d \n", argc);
#endif	
	return 0;	

}


int learn_data(int num_categories)
{
		int output_fd = -1;
		int data_len = 0;		
		int cntr =0, idx = 0;
		char filepath[FILE_PATH_LEN ];
		FILE *input = NULL;
		int fd = -1;
		int input_fd = -1;
		struct stat buf;

		//process id
		unsigned int tid =1;
		//ids of data input nvmallocs
		unsigned int input_id = 1;
		//ids of learning data output nvmallocs
		//set to default starting value of 100
		unsigned int output_id = 100;
		//output file pointer
		FILE *output_fp = NULL;
		//input addr
		char *addr = NULL;
		//output addr
		char *outaddr = NULL;
		size_t size = 0;
		//If data needs to be persisted in NVRAM
		int flgPersist =0;
		//Message from browser
	
		char *out_addr_arr[num_categories];


#ifdef USE_NVMALLOC	
		  for (  idx = 1; idx <= num_categories; idx ++) {
                out_addr_arr[idx] = (char *)allocate_nvmem(output_size);
				memset(out_addr_arr[idx], 0, output_size);
           }
#endif

#ifdef STATS
			gettimeofday(&start_learn_time, NULL);
#endif

			for (  idx = 1; idx <= num_categories; idx ++) {

				init();

				create_learning_args(idx);
#ifdef DEBUG
				fprintf(stderr,"before opening file \n");
#endif

#ifdef NVRAM
				input_fd =  open( argv[3] ,0666);
#else
				input_fd =  open(LEARN_FILE1,0666);
#endif //NVRAM

				if(input_fd < 0){
					LOG(stderr,"creating memory object failed %s\n",(char *)LEARN_FILE1);
					return -1;
				}
#ifdef NVRAM
				fstat(input_fd, &buf);
				data_len = buf.st_size;
				input_size =  data_len;
				LOG(stdout,"data len %d \n", data_len);
				addr = (char *)mmap(0,  input_size, PROT_READ | PROT_WRITE, MAP_SHARED, input_fd, 0);
				if (addr == MAP_FAILED) {
					LOG(stderr, "mmap failed \n");
					close(input_fd);
					exit(-1);
				}
				LOG(stdout,"after mmap\n");
#ifdef STATS
				nv_alloc_bytes +=  input_size;
#endif
				LOG(stderr," After generate_random_text \n");
				input =  fmemopen(addr, data_len, "r");
				if(!input) {
					LOG(stderr,"creating file input from memory stream failed \n");
					return -1;
				}

				online_fd = imc_mem_obj_create(online_size);
				if(online_fd < 0){
					LOG(stderr,"creating memory object failed \n");
					return -1;
				}

				temp_file = imc_mem_obj_create(temp_size);
				if(temp_file < 0){
					LOG(stderr,"creating memory object failed \n");
					return -1;
				}

				LOG(stdout," After temp file creation %s \n", argv[2]);
				output_fd = -1;
				output_fd = setup_map_file1(argv[2], output_size *10);
				output_fp = fdopen(output_fd,"w");
				if(!output_fp){
					LOG(stderr,"opening output file failed \n");
					return -1;
				}

#ifdef USE_NVMALLOC

				outaddr = NULL;
				//outaddr = (char *)allocate_nvmem(output_size);
				outaddr = out_addr_arr[idx];
				LOG(stdout,"outaddr %s \n", outaddr);
				//continue;
#else
				outaddr = (char *)mmap(0, output_size *10, PROT_READ | PROT_WRITE, MAP_SHARED, output_fd, 0);
#endif
				if (outaddr == MAP_FAILED) {
					LOG(stderr, "mmap failed \n");
					close(output_fd);
					exit(-1);
				}
#ifdef STATS
				nv_alloc_bytes += output_size;
#endif
				output_id++;
#else	
				//also update the input file pointer
				input = fdopen(input_fd, "r");
				if(!input) {
					LOG(stderr, "unable to open file %s \n", argv[3]);
					goto error;
				}
#endif

#ifdef DEBUG
				LOG(stdout,"before learn_or_classify_data \n");
#endif
				learn_or_classify_data(argc, argv, input, output_fp, data_len, NULL, addr, outaddr );

#ifdef NVRAM
				LOG(stdout,"successfuly learnt data  %s\n\n\n\n\n\n", argv[6]);
#endif

#ifndef NVRAM
				close(output_fd);
#endif
			}

#ifdef STATS
			gettimeofday(&end_learn_time, NULL);
			//tot_learn_time = simulation_time(start_learn_time, end_learn_time);
#endif //STATS

ret:
		return 0;
		error:
		return -1;
	}

#ifdef NACL
	int classify_data(std::string message, int num_categories) {
#else
	int classify_data(int num_categories) {
#endif

		int output_fd = -1;
		int data_len = 0;
		int cntr =0, idx = 0;
		FILE *input = NULL;
		int fd = -1;
		int input_fd = -1;
		struct stat buf;

		//process id
		unsigned int tid =1;
		//ids of data input nvmallocs
		unsigned int input_id = 1;
		//ids of learning data output nvmallocs
		//set to default starting value of 100
		unsigned int output_id = 100;
		//output file pointer
		FILE *output_fp = NULL;
		//input addr
		char *addr = NULL;
		//output addr
		char *outaddr = NULL;
		size_t size = 0;
		//If data needs to be persisted in NVRAM
		int flgPersist =0;
#ifdef NACL
		//Message from browser
		char *msg = (char *)message.c_str();
#endif

		init();
		create_classify_args(num_categories);

		input = NULL;
		input = fopen(LEARN_FILE1, "r");
		if(!input){
			LOG(stderr,"creating input object failed \n");
			return -1;
		}
		fstat(fileno(input), &buf);
		data_len = buf.st_size;

		if(input)
			fd = fileno(input);


#ifdef STATS
		gettimeofday(&start_read_time, NULL);
#endif


#ifdef NVRAM
		addr = NULL;
		
		addr = (char *)mmap(0, data_len,PROT_READ , MAP_SHARED, fd, 0);
		if (addr == MAP_FAILED) {
			close(fd);
			exit(-1);
		}
#ifdef STATS
		gettimeofday(&end_read_time, NULL);
		//glob_read_time += simulation_time(start_read_time,end_read_time);
#endif


#ifndef USE_NVMALLOC
		online_fd = setup_map_file1("online_file",temp_size);
		if(online_fd < 0){
			LOG(stderr,"creating memory object failed \n");
			return -1;
		}

		temp_file = setup_map_file1("temp_file",temp_size);
		if(temp_file < 0){
			LOG(stderr,"creating memory object failed \n");
			return -1;
		}

#endif //USE_NVMALLOC

#endif

#ifdef STATS
		gettimeofday(&strt_classify, NULL);
#endif //STATS

#ifdef DEBUG
		LOG( stderr,  "before classify \n");
#endif

		learn_or_classify_data(argc, argv, input, output_fp, data_len, maplist_arr, addr, NULL);

#ifdef STATS
		gettimeofday(&end_classify, NULL);
#endif//STATS

		LOG( stdout,  "CLASSIFY DONE \n");
		
#ifdef STATS
		print_stats();
#endif

#ifdef NVRAM
		close(output_fd);
		close(online_fd);
		close(temp_file);				
#endif
	       	
}


#ifdef NACL
int main_test(std::string message) {
#else
	int main() {
#endif

	int num_categories = NUM_CATEGORIES;

#ifdef LEARN_DATA
		learn_data( num_categories);
#else // NOT LEARN_DATA 

#ifdef NACL
		classify_data(message, num_categories);

#else //NOT LEARN DATA
		classify_data(num_categories);
#endif

#endif	

ret:
		return 0;
		error:
		return -1;
}

// namespace pp*/

