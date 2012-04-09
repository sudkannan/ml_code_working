/// @file mapreduce.cc
/// This example demonstrates loading, running and scripting a very simple NaCl
/// module.  To load the NaCl module, the browser first looks for the
/// CreateModule() factory method (at the end of this file).  It calls
/// CreateModule() once to load the module code from your .nexe.  After the
/// .nexe code is loaded, CreateModule() is not called again.
///
/// Once the .nexe code is loaded, the browser than calls the CreateInstance()
/// method on the object returned by CreateModule().  It calls CreateInstance()
/// each time it encounters an <embed> tag that references your NaCl module.
///
/// The browser can talk to your NaCl module via the postMessage() Javascript
/// function.  When you call postMessage() on your NaCl module from the browser,
/// this becomes a call to the HandleMessage() method of your pp::Instance
/// subclass.  You can send messages back to the browser by calling the
/// PostMessage() method on your pp::Instance.  Note that these two methods
/// (postMessage() in Javascript and PostMessage() in C++) are asynchronous.
/// This means they return immediately - there is no waiting for the message
/// to be handled.  This has implications in your program design, particularly
/// when mutating property values that are exposed to both the browser and the
/// NaCl module.

#include <cstdio>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <string.h>
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "task_queue.h"
#include "synch.h"
#include <assert.h>
#include <pthread.h>
#include "thread_pool.h"
#include "atomic.h"
#include "scheduler.h"
#include "map_reduce.h"

#include <sys/unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits>



using namespace std;

int start_map_reduce( );


namespace {
// The expected string sent by the browser.
const char* const kHelloString = "hello";
// The string sent back to the browser upon receipt of a message
// containing "hello".
const char* const kReplyString = "hello from NaCl";
} // namespace



/// The Instance class.  One of these exists for each instance of your NaCl
/// module on the web page.  The browser will ask the Module object to create
/// a new Instance for each occurence of the <embed> tag that has these
/// attributes:
///     type="application/x-nacl"
///     src="mapreduce.nmf"
/// To communicate with the browser, you must override HandleMessage() for
/// receiving messages from the borwser, and use PostMessage() to send messages
/// back to the browser.  Note that this interface is entirely asynchronous.
class MapreduceInstance : public pp::Instance {
 public:
  /// The constructor creates the plugin-side instance.
  /// @param[in] instance the handle to the browser-side plugin instance.
  explicit MapreduceInstance(PP_Instance instance) : pp::Instance(instance)
  {}
  virtual ~MapreduceInstance() {}

  /// Handler for messages coming in from the browser via postMessage().  The
  /// @a var_message can contain anything: a JSON string; a string that encodes
  /// method names and arguments; etc.  For example, you could use
  /// JSON.stringify in the browser to create a message that contains a method
  /// name and some parameters, something like this:
  ///   var json_message = JSON.stringify({ "myMethod" : "3.14159" });
  ///   nacl_module.postMessage(json_message);
  /// On receipt of this message in @a var_message, you could parse the JSON to
  /// retrieve the method name, match it to a function call, and then call it
  /// with the parameter.
  /// @param[in] var_message The message posted by the browser.
  virtual void HandleMessage(const pp::Var& var_message) {
    // TODO(sdk_user): 1. Make this function handle the incoming message.

   if (!var_message.is_string())
    return;
  std::string message = var_message.AsString();
  pp::Var var_reply;
  if (message == kHelloString) {
    var_reply = pp::Var(kReplyString);
   printf("STARTING MAP REDUCE \n");
    PostMessage(var_reply);
  }
  start_map_reduce();	
  }
};

/// The Module class.  The browser calls the CreateInstance() method to create
/// an instance of your NaCl module on the web page.  The browser creates a new
/// instance for each <embed> tag with type="application/x-nacl".
class MapreduceModule : public pp::Module {
 public:
  MapreduceModule() : pp::Module() {}
  virtual ~MapreduceModule() {}

  /// Create and return a MapreduceInstance object.
  /// @param[in] instance The browser-side instance.
  /// @return the plugin-side instance.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MapreduceInstance(instance);
  }
};

namespace pp {
/// Factory function called by the browser when the module is first loaded.
/// The browser keeps a singleton of this module.  It calls the
/// CreateInstance() method on the object you return to make instances.  There
/// is one instance per <embed> tag on the page.  This is the main binding
/// point for your NaCl module with the browser.
Module* CreateModule() {
  return new MapreduceModule();
}
}  // namespace pp



task_queue::task_queue(int sub_queues, int num_threads)
{
    this->num_queues = sub_queues;
    this->num_threads = num_threads;

    this->queues = new std::deque<task_t>[this->num_queues];
    this->locks = new lock*[this->num_queues];
    for (int i = 0; i < this->num_queues; ++i)
        this->locks[i] = new lock(this->num_threads);
}

task_queue::~task_queue()
{
    for (int i = 0; i < this->num_queues; ++i) {
        delete this->locks[i];
    }

    delete [] this->locks;
    delete [] this->queues;
}


/* Queue TASK at LGRP task queue with locking.
   LGRP is a locality hint denoting to which locality group this task 
   should be queued at. If LGRP is less than 0, the locality group is 
   randomly selected. TID is required for MCS locking. */
void task_queue::enqueue (const task_t& task, thread_loc const& loc, int total_tasks, int lgrp)
{
    int index = (lgrp < 0) ?
        (total_tasks > 0 ? task.id * this->num_queues / total_tasks : rand_r(&loc.seed)) :
        lgrp;
    index %= this->num_queues;

    locks[index]->acquire(loc.thread);
    queues[index].push_back(task);
    locks[index]->release(loc.thread);
}

/* Queue TASK at LGRP task queue without locking.
   LGRP is a locality hint denoting to which locality group this task
   should be queued at. If LGRP is less than 0, the locality group is
   randomly selected. */
void task_queue::enqueue_seq (const task_t& task, int total_tasks, int lgrp)
{
    int index = (lgrp < 0) ?
        (total_tasks > 0 ? task.id * this->num_queues / total_tasks : rand()) :
        lgrp;
    index %= this->num_queues;
    queues[index].push_back(task);
}



int task_queue::dequeue (task_t& task, thread_loc const& loc)
{
    int index = (loc.lgrp < 0) ? loc.cpu : loc.lgrp;

   // Do task stealing if nothing on our queue.
   //   Cycle through all indexes until success or exhaustion 
    int ret = 0;
    for (int i = index; i < index + this->num_queues && ret == 0; i++)
    {
        int idx = i % this->num_queues;
        locks[idx]->acquire(loc.thread);
        if(this->queues[idx].size() > 0)
        {
            if(idx == index)
            {
                task = this->queues[idx].front();
                this->queues[idx].pop_front();
            }
            else
            {
                task = this->queues[idx].back();
                this->queues[idx].pop_back();
                dprintf("Stole task from %d to %d\n", idx, index);
            }
            ret = 1;
        }
        locks[idx]->release(loc.thread);
    }

    if(ret) {
        __builtin_prefetch ((void*)task.data, 0, 3);
        dprintf("Task %llu: started on cpu %d\n", task.id, loc.cpu);
    }

    return ret;
}



thread_pool::thread_pool(int num_threads, sched_policy const* policy)
{
    int             ret;
    pthread_attr_t  attr;

    this->num_threads = num_threads;
    this->num_workers = num_threads;

    this->args = new void*[num_threads];
    this->threads = new pthread_t[num_threads];
    this->thread_args = new thread_arg_t[num_threads];

    CHECK_ERROR (pthread_attr_init (&attr));
    //CHECK_ERROR (pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM));
    CHECK_ERROR (pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED));

    this->die = 0;
    for (int i = 0; i < num_threads; ++i) {
        // Initialize thread argument.
        this->thread_args[i].pool = this;
        this->thread_args[i].loc.thread = i;
        this->thread_args[i].loc.cpu = policy != NULL ? policy->thr_to_cpu(i) : -1;
        // we'll get this when the thread runs...
        this->thread_args[i].loc.lgrp = -1;
        this->thread_args[i].loc.seed = i;

        ret = pthread_create (
            &this->threads[i], &attr, loop, &this->thread_args[i]);

        


    }
}


thread_pool::~thread_pool()
{
    assert (this->die == 0);

    this->num_workers = this->num_threads;
    this->num_workers_done = 0;

    this->die = 1;
    for (int i = 0; i < this->num_threads; ++i) {
        this->thread_args[i].sem_run.post();
    }
    this->sem_all_workers_done.wait();

    delete [] this->args;
    delete [] this->threads;
    delete [] this->thread_args;
}

int thread_pool::set(thread_func thread_func, void** args, int num_workers)
{
    this->thread_function = thread_func;
    assert (num_workers <= this->num_threads);
    this->num_workers = num_workers;

    for (int i = 0; i < this->num_workers; ++i)
    {
        int j = i * this->num_threads / num_workers;
        this->args[j] = args[i];
    }

    return 0;
}

int thread_pool::begin()
{
    if (this->num_workers == 0)
        return 0;

    this->num_workers_done = 0;

    for (int i = 0; i < this->num_workers; ++i)
    {
        int j = i * this->num_threads / num_workers;
        this->thread_args[j].sem_run.post();
    }

    return 0;
}

int thread_pool::wait()
{
    if (this->num_workers == 0)
        return 0;

    this->sem_all_workers_done.wait();

    return 0;
}


void* thread_pool::loop(void* arg)
{
    thread_arg_t*    thread_arg = (thread_arg_t*)arg;

    assert (thread_arg);

    thread_func     thread_func;
    thread_pool*    pool = thread_arg->pool;
    thread_loc&        loc = thread_arg->loc;
    void            *thread_func_arg;

    if(loc.cpu >= 0)
        proc_bind_thread (loc.cpu);

    loc.lgrp = loc_get_lgrp();

    while (!pool->die)
    {
        thread_arg->sem_run.wait();
        if (pool->die)
            break;

        thread_func = *(pool->thread_function);
        thread_func_arg = pool->args[loc.thread];

        // Run thread function.
        (*thread_func)(thread_func_arg, loc);

        pool->num_workers_done++;
        int num_workers_done = pool->num_workers_done;
        //int num_workers_done = fetch_and_inc(&pool->num_workers_done) + 1;
        if (num_workers_done == pool->num_workers) {
            // Everybody's done.
            pool->sem_all_workers_done.post();
        }
    }

    //int num_workers_done = fetch_and_inc(&pool->num_workers_done) + 1;
    pool->num_workers_done++;
    int num_workers_done = pool->num_workers_done;    
    if (num_workers_done == pool->num_workers) {
        // Everybody's done.
        pool->sem_all_workers_done.post();

	 }

    return NULL;
}


#define DEFAULT_DISP_NUM 10

// a passage from the text. The input data to the Map-Reduce
struct wc_string {
    char* data;
    uint64_t len;
};

// a single null-terminated word
struct wc_word {
    char* data;

    // necessary functions to use this as a key
    bool operator<(wc_word const& other) const {
        return strcmp(data, other.data) < 0;
    }
    bool operator==(wc_word const& other) const {
        return strcmp(data, other.data) == 0;
    }
};


// a hash for the word
struct wc_word_hash
{
    // FNV-1a hash for 64 bits
    size_t operator()(wc_word const& key) const
    {
        char* h = key.data;
        uint64_t v = 14695981039346656037ULL;
        while (*h != 0)
            v = (v ^ (size_t)(*(h++))) * 1099511628211ULL;
        return v;
    }
};


#ifdef MUST_USE_FIXED_HASH
class WordsMR : public MapReduceSort<WordsMR, wc_string, wc_word, uint64_t, fixed_hash_container<wc_word, uint64_t, sum_combiner, 32768, wc_word_hash
#else
class WordsMR : public MapReduceSort<WordsMR, wc_string, wc_word, uint64_t, hash_container<wc_word, uint64_t, sum_combiner, wc_word_hash
#endif
#ifdef TBB
    , tbb::scalable_allocator
#endif
> >
{
    char* data;
    uint64_t data_size;
    uint64_t chunk_size;
    uint64_t splitter_pos;
public:
    explicit WordsMR(char* _data, uint64_t length, uint64_t _chunk_size) :
        data(_data), data_size(length), chunk_size(_chunk_size),
            splitter_pos(0) {}

    void* locate(data_type* str, uint64_t len) const
    {
        return str->data;
    }

    void map(data_type const& s, map_container& out) const
    {
        for (uint64_t i = 0; i < s.len; i++)
        {
            s.data[i] = toupper(s.data[i]);
        }

        uint64_t i = 0;
        while(i < s.len)
        {
            while(i < s.len && (s.data[i] < 'A' || s.data[i] > 'Z'))
                i++;
            uint64_t start = i;
            while(i < s.len && ((s.data[i] >= 'A' && s.data[i] <= 'Z') || s.data[i] == '\''))
                i++;
            if(i > start)
            {
                s.data[i] = 0;
                wc_word word = { s.data+start };
                emit_intermediate(out, word, 1);
            }
        }
    }

    /// wordcount split()
    //  Memory map the file and divide file on a word border i.e. a space.
    int split(wc_string& out)
    {
        // End of data reached, return FALSE. 
        if ((uint64_t)splitter_pos >= data_size)
        {
            return 0;
        }

        // Determine the nominal end point. 
        uint64_t end = std::min(splitter_pos + chunk_size, data_size);

        // Move end point to next word break 
        while(end < data_size &&
            data[end] != ' ' && data[end] != '\t' &&
            data[end] != '\r' && data[end] != '\n')
            end++;

        // Set the start of the next data. 
        out.data = data + splitter_pos;
        out.len = end - splitter_pos;

        splitter_pos = end;

        // Return true since the out data is valid. 
        return 1;
    }

    bool sort(keyval const& a, keyval const& b) const
    {
        return a.val < b.val || (a.val == b.val && strcmp(a.key.data, b.key.data) > 0);
    }
};

FILE* fopen_createTextFile(const char *testname) {

  FILE *f = fopen("testdata.txt", "wt");
  if (NULL == f) {
    return NULL;
  }

  return  f;
}




//#define NO_MMAP
int start_map_reduce( )
{
    int fd;
    char * fdata;
    unsigned int disp_num;
    struct stat finfo;
    char * fname, * disp_num_str;
    struct timespec begin, end;
    FILE *fp;
    int data_size = 400000000;
    int index =0;
    void **ptr = NULL;


    nacl_irt_sysb(ptr);
    
    //output file
    get_time (begin);

    disp_num_str = 0;
    printf("Wordcount: Running...\n");

    uint64_t r = 0;
    fdata = (char *) _nvmalloc_r(data_size);

    for(index=0; index < data_size; index++){ 
        
       if(index % 10 == 0){  
	 fdata[index] = ' ';
       }else {
	 fdata[index] = 'a';
       }
     }
   
    CHECK_ERROR (fdata == NULL);
    //CHECK_ERROR (r != (uint64_t)data_size);

   // Get the number of results to display
    CHECK_ERROR((disp_num = (disp_num_str == NULL) ?
      DEFAULT_DISP_NUM : atoi(disp_num_str)) <= 0);

    get_time (end);

#ifdef TIMING
    print_time("initialize", begin, end);
#endif

    printf("Wordcount: Calling MapReduce Scheduler Wordcount\n");
    get_time (begin);
    std::vector<WordsMR::keyval> result;
    WordsMR mapReduce(fdata, data_size, 1024*1024);
    CHECK_ERROR( mapReduce.run(result) < 0);
    get_time (end);

#ifdef TIMING
    print_time("library", begin, end);
#endif
    printf("Wordcount: MapReduce Completed\n");

    get_time (begin);

    unsigned int dn = std::min(disp_num, (unsigned int)result.size());
    printf("\nWordcount: Results (TOP %d of %lu):\n", dn, result.size());
    uint64_t total = 0;
    for (size_t i = 0; i < dn; i++)
    {
        printf("%15s - %lu\n", result[result.size()-1-i].key.data, result[result.size()-1-i].val);
    }
    for(size_t i = 0; i < result.size(); i++)
    {
        total += result[i].val;
    }
    printf("Total: %lu\n", total);
    get_time (end);

#ifdef TIMING
print_time("finalize", begin, end);
#endif

    return 0;
}

// vim: ts=8 sw=4 sts=4 smarttab smartindent	

		

	








