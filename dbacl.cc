/* 
 * Copyright (C) 2002 Laird Breyer
 *  
 * This program is myfree software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * Author:   Laird Breyer <laird@lbreyer.com>
 */

/* 
 * Note to regenerate the Makefile and configure script:
 * aclocal
 * autoconf
 * touch NEWS README AUTHORS ChangeLog
 * automake -a
 *
 * You can also just run the bootstrap script.
 */

//uncomment this code
//errormsg(E_WARNING, "dirichlet measure did not converge.\n");

#define NACL_UNSECURED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef NULL
#define NULL   ((void *) 0)
#endif

#if defined HAVE_UNISTD_H
#include <unistd.h> 
#include <sys/types.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <locale.h>
#include <sys/mman.h>

#if defined HAVE_LANGINFO_H
#include <langinfo.h>
#if !defined CODESET
/* on OpenBSD, CODESET doesn't seem to be defined - 
   we use 3, which should be US-ASCII, but it's not ideal... */
#define CODESET 3
#endif
#endif

#include "util.h"
#include "dbacl.h" /* make sure this is last */
#include "nvmalloc_wrap.h"

#include <sys/mman.h>

#ifdef NACL
#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#endif

#include "dbacl.h"

#define TRACK_MEMORY

#ifdef TRACK_MEMORY
#include <map>
#include <string>
#include <iostream>
using namespace std;
std::map<int,unsigned long> results;
static int memcntr;
#endif 


/* global variables */

struct MAPLIST  maplist[256];
struct MAPLIST maplist_arr[256];
int temp_file;
int online_fd;
extern size_t region_size;

extern signal_cleanup_t cleanup;

extern hash_bit_count_t default_max_hash_bits;
extern hash_count_t default_max_tokens;

extern hash_bit_count_t default_max_grow_hash_bits;
extern hash_count_t default_max_grow_tokens;

hash_bit_count_t decimation;
token_count_t ftreshold = 0;

learner_t learner;
dirichlet_t dirichlet;
int filter[MAX_CAT];
category_count_t filter_count = 0;

extern category_t cat[MAX_CAT];
extern category_count_t cat_count;

extern myregex_t re[MAX_RE];
extern regex_count_t regex_count;

extern empirical_t empirical;

extern options_t u_options;
extern options_t m_options;
extern char *extn;

extern token_order_t ngram_order; /* defaults to 1 */

/* for counting emails */
bool_t not_header; 
extern MBOX_State mbox;
extern XML_State xml;

/* for option processing */
extern char *optarg;
extern int optind, opterr, optopt;

char *title = (char *)"";
char *online = (char *)"";
extern char *progname;
extern char *inputfile;
extern long inputline;

extern int cmd;
int exit_code = 0; /* default */

int overflow_warning = 0;
int digramic_overflow_warning = 0;
int skewed_constraints_warning = 0;

extern long system_pagesize;
extern void *in_iobuf;
extern void *out_iobuf;
extern void init_fram_var();

#ifdef STATS
extern unsigned long learner_write_bytes;
extern unsigned long learner_read_bytes;
extern long glob_read_time;
extern unsigned int hash_tokens;
#endif

/* tolerance for the error in divergence - this can be changed with -q.
   More accuracy means slower learning. This should be the same order
   of magnitude as the error due to digitization, but we tweak them
   to obtain timely output */

int quality = 0;
double qtol_alpha = 0.0001;
double qtol_div = 0.01;
double qtol_lam = CLIP_LAMBDA_TOL(0.05);
double qtol_logz = 0.05;
bool_t qtol_multipass = 0;



/***********************************************************
 * MISCELLANEOUS FUNCTIONS                                 *
 ***********************************************************/

static void usage(/*@unused@*/ char **argv) {
  LOG(stderr, 
	  "\n");
  LOG(stderr, 
	  "dbacl [-vniNR] [-T type] -c CATEGORY [-c CATEGORY]...\n");
  LOG(stderr, 
	  "      [-f KEEP]... [FILE]...\n");
  LOG(stderr, 
	  "\n");
  LOG(stderr, 
	  "      classifies FILE or STDIN using CATEGORY, optionally\n");
  LOG(stderr, 
	  "      removing all lines which don't fit the category KEEP.\n");
  LOG(stderr, 
	  "\n");
  LOG(stderr, 
	  "dbacl [-vnirNDL] [-h size] [-T type] -l CATEGORY \n");
  LOG(stderr, 
	  "      [-g regex]... [FILE]...\n");
  LOG(stderr, 
	  "\n");
  LOG(stderr, 
	  "      builds a maximum entropy model from the words in FILE or STDIN\n");
  LOG(stderr, 
	  "      or concatenated regex submatches if using the -g option.\n");
  LOG(stderr, 
	  "\n");
  LOG(stderr, 
	  "dbacl -V\n");
  LOG(stderr, 
	  "\n");
  LOG(stderr, 
	  "      prints program version.\n");
}

void print_content(char *mmap_addr) {

  // this is needed in case we try to open with write permissions,
  //   which would otherwise create the file 
	if( mmap_addr ) {
  	    LOG(stderr, "print_content:mmap_addr %s \n", mmap_addr);
    }else{
		LOG(stderr,"print_content: mapping failed \n");
	}
 }



/***********************************************************
 * CATEGORY FUNCTIONS                                      *
 ***********************************************************/

   void init() {

      filter_count = 0;

      title = (char *)"";
      online = (char *)"";
      exit_code = 0; /* default */

      overflow_warning = 0;
      digramic_overflow_warning = 0;
      skewed_constraints_warning = 0;

      init_fram_var();

    }


/* note: don't forget to flush after each line */
void line_score_categories(char *textbuf) {
  category_count_t i;
  category_count_t map;
  score_t c, cmax;

  if( !textbuf ) { return; }

  /* find MAP */
  cmax = cat[0].score; 
  map = 0;
  for(i = 0; i < cat_count; i++) {
    if(cmax < cat[i].score) {
      cmax = cat[i].score;
      map = i;
    }
    /* finish sample variance calculation */
    cat[i].score_s2 = (cat[i].complexity * cat[i].score_s2 -
		       cat[i].score * cat[i].score)/
      ((score_t)cat[i].complexity * ((score_t)cat[i].complexity - 1));
  }

  if( !(u_options & (1<<U_OPTION_DUMP)) ) {

    if(u_options & (1<<U_OPTION_POSTERIOR) ) {
      /* compute probabilities given exclusive choices */
      c = 0.0;
      for(i = 0; i < cat_count; i++) {
	c += exp((cat[i].score - cmax));
      }
      
      if( *textbuf ) {
	for(i = 0; i < cat_count; i++) {
	  LOG(stdout, "%s %6.2" FMT_printf_score_t "%% ", 
		  cat[i].filename, 100 * exp((cat[i].score - cmax))/c);
	}
	LOG(stdout, "%s", textbuf);
	fflush(stdout);
      }
    } else if( u_options & (1<<U_OPTION_SCORES) ) {
      /* display normalized divergence scores */
      if( *textbuf ) {
	for(i = 0; i < cat_count; i++) {
	  if( u_options & (1<<U_OPTION_VERBOSE) ) {
	    LOG(stdout, "%s %6.2" FMT_printf_score_t " * %-4.1f ", 
		    cat[i].filename, 
		    -nats2bits(cat[i].score/cat[i].complexity), 
		    cat[i].complexity);
	  } else {
	    LOG(stdout, "%s %6.2" FMT_printf_score_t " ", 
		    cat[i].filename, -nats2bits(cat[i].score));
	  }
	}
	LOG(stdout, "%s", textbuf);
	fflush(stdout);
      }
    } else {
      /* prune the text which doesn't fit */
      for(i = 0; i < filter_count; i++) {
	if( cat[map].score <= cat[filter[i]].score ) {
	  LOG(stdout, "%s", textbuf);
	  fflush(stdout);
	  break;
	}
      }
    }

  }    
  /* clean up for next line */
  for(i = 0; i < cat_count; i++) {
    cat[i].score = 0.0;
    cat[i].score_s2 = 0.0;
    cat[i].score_div = 0.0;
    cat[i].score_shannon = 0.0;
    cat[i].score_exp = 0.0;
    cat[i].complexity = 0.0;
    cat[i].fcomplexity = 0;
  }
  if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
    clear_empirical(&empirical);
  }
}

void score_categories() {
  bool_t no_title;
  category_count_t i;
  score_t c, cmax;



  /* find MAP */
  cmax = cat[0].score; 
  exit_code = 0;
  for(i = 0; i < cat_count; i++) {
    if(cmax < cat[i].score) {
      cmax = cat[i].score;
      exit_code = (int)i;
    }
    /* finish sample variance calculation */
    cat[i].score_s2 = (cat[i].complexity * cat[i].score_s2 -
		       cat[i].score * cat[i].score)/
      ((score_t)cat[i].complexity * ((score_t)cat[i].complexity - 1));
#ifdef STATS
    //number of tokens generated for input
    //all input test file sizes are same, so lets not
    //increment it for now
    hash_tokens =cat[i].model_unique_token_count;
#endif
 
  }

  /* finish computing entropies */
  if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
    for(i = 0; i < cat_count; i++) {
      cat[i].score_shannon = 
	-( cat[i].score_shannon / ((weight_t)cat[i].complexity) -
	   log((weight_t)cat[i].complexity) );
      cat[i].score_div = 
	-(cat[i].score/cat[i].complexity + cat[i].score_shannon);
    }
  }


  /* there are three cases: posterior, scores, nothing */

  if( u_options & (1<<U_OPTION_POSTERIOR) ) {

    if( u_options & (1<<U_OPTION_APPEND) ) {
      LOG(stdout, "\n# probabilities ");
    }

    /* here we compute probabilities given exclusive choices */
    c = 0.0;
    for(i = 0; i < cat_count; i++) {
      c += exp((cat[i].score - cmax));
    }

    for(i = 0; i < cat_count; i++) {
      LOG(stdout, "%s %5.2" FMT_printf_score_t "%% ", 
	      cat[i].filename, 100 * exp((cat[i].score - cmax))/c);
      if( u_options & (1<<U_OPTION_CONFIDENCE) ) {
	LOG(stdout, "@ %5.1f%% ", 
		(float)gamma_pvalue(&cat[i], cat[i].score_div)/10);
      }
    }
    LOG(stdout, "\n");

  } else if( u_options & (1<<U_OPTION_SCORES) ) {

    if( u_options & (1<<U_OPTION_APPEND) ) {
      LOG(stdout, "\n# scores ");
    }


    /* display logarithmic score */
    for(i = 0; i < cat_count; i++) {
      if( u_options & (1<<U_OPTION_VERBOSE) ) {
	if( u_options & (1<<U_OPTION_VAR) ) {
	  LOG(stdout, "%s ( %5.2" FMT_printf_score_t 
		  " # %5.2" FMT_printf_score_t " )* %-.1f ", 
		  cat[i].filename, 
		  -nats2bits(cat[i].score/cat[i].complexity),
		  nats2bits(sqrt(cat[i].score_s2/cat[i].complexity)),
		  cat[i].complexity);
	} else {
	  LOG(stdout, "%s %5.2" FMT_printf_score_t " * %-.1f ", 
		  cat[i].filename, 
		  -nats2bits(cat[i].score/cat[i].complexity),
		  cat[i].complexity);
	}
	if( u_options & (1<<U_OPTION_CONFIDENCE) ) {
	  LOG(stdout, "@ %5.1f%% ", 
		  (float)gamma_pvalue(&cat[i], cat[i].score_div)/10);
	}
      } else {
	LOG(stdout, "%s %5.2" FMT_printf_score_t " ", 
		cat[i].filename,
		-nats2bits(cat[i].score));
      }
    }

    LOG(stdout, "\n");

    if( u_options & (1<<U_OPTION_APPEND) ) {
      no_title = (bool_t)1;
      for(i = 0; i < cat_count; i++) {
	if( cat[i].model_num_docs > 0 ) {
	  if( no_title ) {
	    LOG(stdout, "# mean_complexity ");
	    no_title = (bool_t)0;
	  }
	  LOG(stdout, "%s %5.2" FMT_printf_score_t " ", 
		  cat[i].filename, 
		  (double)cat[i].model_full_token_count/cat[i].model_num_docs);
	}
      }
      if( !no_title ) { LOG(stdout, "\n"); }
    }

  } else {

    if( u_options & (1<<U_OPTION_VERBOSE) ) {
      if( (u_options & (1<<U_OPTION_CONFIDENCE)) ||
	  (u_options & (1<<U_OPTION_VAR)) ) {
	for(i = 0; i < cat_count; i++) {
	  LOG(stdout, "%s ( %5.2" FMT_printf_score_t " + "
		  "%-5.2" FMT_printf_score_t,
		  cat[i].filename, 
		  nats2bits(cat[i].score_div),
		  nats2bits(cat[i].score_shannon));
	  if( u_options & (1<<U_OPTION_VAR) ) {
	    LOG(stdout, " # %5.2" FMT_printf_score_t,
		    nats2bits(sqrt(cat[i].score_s2/cat[i].complexity)));
	  }
	  LOG(stdout, " )* %-6.1f", cat[i].complexity);
	  if( u_options & (1<<U_OPTION_CONFIDENCE) ) {
	    LOG(stdout, " @ %5.1f%% ", 
		    (float)gamma_pvalue(&cat[i], cat[i].score_div)/10);
	  }
	}
	LOG(stdout, "\n");
      } else {
	if( u_options & (1<<U_OPTION_APPEND) ) {
	  LOG(stdout, "\n# category ");
	}
	LOG(stdout, "%s\n", cat[exit_code].filename);
	     }
    } else {
      if( u_options & (1<<U_OPTION_VAR) ) {
	if( u_options & (1<<U_OPTION_APPEND) ) {
	  LOG(stdout, "\n# category ");
	}

	cmax = 1.0;
	for(i = 0; i < cat_count; i++) {
	  if( (int)i != exit_code ) {
	    /* c is a standard normal variable */
	    c = (-cat[i].score/cat[i].complexity - 
		 -cat[exit_code].score/cat[exit_code].complexity) /
	      sqrt(cat[i].score_s2/cat[i].complexity + 
		   cat[exit_code].score_s2/cat[exit_code].complexity);
	    /* c will always be positive, but let's be safe */
	    if( isnan(c) ) {
	      c = 0.0;
	    } else if( c > 5.0 ) {
	      c = 1.0;
	    } else if( c < -5.0 ) {
	      c = 0.0;
	    } else {
	      /* normal_cdf(c) has a range of 0.5 - 1.0, we convert to a full range */
	      c = 2 * normal_cdf(c) - 1.0;
	    }
	    cmax = (cmax > c) ? c : cmax; 
	  }
	}

	LOG(stdout, "%s # %d%%\n", 
		cat[exit_code].filename, (int)ceil(cmax * 100.0)); 
      }
    }

  }


  exit_code++; /* make number between 1 and cat_count+1 */
}

/***********************************************************
 * FILE MANAGEMENT FUNCTIONS                               *
 ***********************************************************/

bool_t check_magic_write(char *path, char *magic, size_t len) {
  FILE *output;
  char buf[MAGIC_BUFSIZE];

  output = fopen(path, "rb");
  if( output ) {
    /* output file exists already */
    if( (!feof(output) && !fgets(buf, MAGIC_BUFSIZE, output)) ||
	(strncmp(buf, magic, len) != 0) ) {
      errormsg(E_ERROR,"the file %s is already used for something, "
	       "use another filename. Nothing written.\n", path);
      fclose(output);
      return (bool_t)0;
    } else {
      /* it's an existing category file */
      fclose(output);
    }
  }
  return (bool_t)1;
}

/* the standard tmpfile() call doesn't tell the filename,
   the standard mkstemp() is unreliable,
   and the moronic unix system doesn't remember filenames on open().
   1) remember to myfree the tmpname when done.
   2) the tmplate is appended with a pattern .tmp.xx,
   3) if you want a particular directory, prepend it to tmplate.
   4) file is opened for read/write, but truncated to zero.
*/
/*@null@*/ 
FILE *mytmpfile(const char *tmplate, /*@out@*/ char **tmpname) {
  FILE *result = NULL;
  size_t l;
  int i, fd;

  l = strlen(tmplate);
  *tmpname = (char *)mymalloc(sizeof(char)*(l + 8));
  if( *tmpname ) {
    strcpy(*tmpname, tmplate);
#if defined ATOMIC_CATSAVE
    for(i = 0; i < 100; i++) {
      snprintf(*tmpname + l, 8, ".tmp.%d", i);
      /* this may have problems on NFS systems? */
      fd = ATOMIC_CREATE(*tmpname);
      if( fd != -1 ) {
	result = fdopen(fd, "w+b");
	/* no need to close fd */
	break;
      }
    }
#else
#ifndef NACL_UNSECURED
    snprintf(*tmpname + l, 8, ".tmp.%d", rand() % 100);
#endif
    result = fopen(*tmpname, "w+b");
#endif

    if( !result ) {
      myfree(*tmpname);
      *tmpname = NULL;
    }
  }
  return result;
}

bool_t myrename(const char *src, const char *dest) {

#ifdef NACL_UNSECURED
	return (bool_t)1; /* just pretend */
#endif

#if defined ATOMIC_CATSAVE
  /* the rename is atomic on posix */
  return (bool_t)(rename(src, dest) == 0);
#else
  return (bool_t)1; /* just pretend */
#endif
}


bool_t write_category_headers(learner_t *learner, FILE *output) {
  regex_count_t c;
  char scratchbuf[MAGIC_BUFSIZE];
  char smb[MAX_SUBMATCH+1];
  token_order_t s;
  char *p;

#ifdef DEBUG
  LOG(stderr,"Writing category headers \n");
#endif

  bool_t ok = (bool_t)1;

  /* print out standard category file headers */
  ok = ok && 
    (0 < fprintf(output, MAGIC1, learner->filename, 
		 (m_options & (1<<M_OPTION_REFMODEL)) ? "(ref)" : ""));
  ok = ok &&
    (0 < fprintf(output, 
		 MAGIC2_o, learner->divergence, learner->logZ, 
		 (short int)learner->max_order,
		 (m_options & (1<<M_OPTION_MULTINOMIAL)) ? "multinomial" : "hierarchical" ));
  ok = ok &&
    (0 < fprintf(output, MAGIC3, 
		 (short int)learner->max_hash_bits, 
		 (long int)learner->full_token_count, 
		 (long int)learner->unique_token_count,
		 (long int)(learner->doc.count)));

  ok = ok &&
    (0 < fprintf(output, MAGIC8_o,
		 learner->shannon, 
		 learner->alpha, learner->beta,
		 learner->mu, learner->s2));

  /* print out any regexes we might need */
  for(c = 0; c < regex_count; c++) {
    /* write the bitmap */
    smb[0] = '\0';
    for(p = smb, s = 1; s <= MAX_SUBMATCH; s++) {
      if( re[c].submatches & (1<<s) ) {
	*p++ = (char)s + '0';
      }
    }
    *p = '\0';

    ok = ok && 
      (0 < fprintf(output, MAGIC5_o, re[c].string, smb));
  }

  /* print options */
  ok = ok &&
    (0 < fprintf(output, MAGIC4_o, m_options, 
		 print_model_options(m_options, scratchbuf)));

  ok = ok &&
    (0 < fprintf(output, MAGIC6)); 

#ifdef STATS
       learner_write_bytes += ftell(output);
#endif

  return ok;
}





bool_t nvram_write_category_headers(learner_t *learner, FILE *output, char *input, int *len) {
  regex_count_t c;
  char scratchbuf[MAGIC_BUFSIZE];
  char smb[MAX_SUBMATCH+1];
  token_order_t s;
  char *p;

  char buffer[256];
  char buffer1[256];
  char buffer2[256];
  char buffer3[256];
  char buffer4[256];
  char buffer5[256];
  char buffer6[1024];
  char buffer7[256];
  char buffer8[256];
  char buffer9[8192];
  char buffer10[256];

  bool_t ok = (bool_t)1;


  if(!(input && len)){
        LOG(stderr,"input is null\n");
        return ok;
  }


  bzero(buffer, 256);
  bzero(buffer1, 256);
  bzero(buffer2, 256);
  bzero(buffer3, 256);
  bzero(buffer4, 256);
  bzero(buffer5, 256);
  bzero(buffer6, 1024);
  bzero(buffer7, 256);
  bzero(buffer8, 256);
  bzero(buffer9, 8192);
  bzero(buffer10,256);


  ok = ok &&
  (0 <
  sprintf(buffer, MAGIC1, learner->filename,
                 (m_options & (1<<M_OPTION_REFMODEL)) ? "(ref)" : ""));

  strcpy(buffer9,buffer);


  ok = ok &&
  (0 <
  sprintf(buffer1,
                 MAGIC2_o, learner->divergence, learner->logZ,
                 (short int)learner->max_order,
                 (m_options & (1<<M_OPTION_MULTINOMIAL)) ? "multinomial" : "hierarchical" ));

   strcat(buffer9,buffer1);


   ok = ok &&
   (0 < sprintf(buffer2, MAGIC3,
                 (short int)learner->max_hash_bits,
                 (long int)learner->full_token_count,
                 (long int)learner->unique_token_count,
                 (long int)(learner->doc.count)));

  strcat(buffer9,buffer2);


  ok = ok &&
   (0 <
   sprintf(buffer3, MAGIC8_o,
                 learner->shannon,
                 learner->alpha, learner->beta,
                 learner->mu, learner->s2));


   strcat(buffer9,buffer3);


  /* print out any regexes we might need */
  for(c = 0; c < regex_count; c++) {
    /* write the bitmap */
    smb[0] = '\0';
    for(p = smb, s = 1; s <= MAX_SUBMATCH; s++) {
      if( re[c].submatches & (1<<s) ) {
        *p++ = (char)s + '0';
      }
    }
    *p = '\0';


     bzero(buffer5, 256);

      if(ok) {
         ok = sprintf(buffer5, MAGIC5_o, re[c].string, smb);
        printf("NNNN 7  %s \n", buffer5);
        strcat(buffer6,buffer5);
      }

  }

   strcat(buffer9,buffer6);


   ok = ok &&
    (0 < sprintf(buffer7, MAGIC4_o, m_options,
                 print_model_options(m_options, scratchbuf))); 


    strcat(buffer9,buffer7);


  ok = ok &&
    (0 < sprintf(buffer8, MAGIC6));


   strcat(buffer9,buffer8);

   *len = strlen(buffer9);

   memcpy(input, buffer9, *len );


  return ok;
}


#if defined DIGITIZE_DIGRAMS
  typedef digitized_weight_t myweight_t;
#else
  typedef weight_t myweight_t;
#endif

error_code_t save_learner(learner_t *learner) {

  alphabet_size_t i, j;
  hash_count_t t;
  FILE *output;
  char *tempname = NULL;

  bool_t ok;
  size_t n;
  c_item_t ci;
  c_item_t *ci_ptr;
  myweight_t shval;
  myweight_t *shval_ptr = NULL;

  long mmap_offset = 0;
  size_t mmap_length = 0;
  byte_t *mmap_start = NULL;


  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    LOG(stdout, "saving category to file %s\n", learner->filename);
  }

  /* don't overwrite data files */
  /*if( !check_magic_write(learner->filename, (char *)MAGIC1, 10) ) {
    exit(1);
  }*/

  /* In case we have both the -m and -o switches we try to write the
     data with mmap. We don't do this in general, because mmap can
     cause corruption, but if -m and -o are used together, we assume the 
     user knows that a single process must read/write the file at a time.
     Also, we don't try to create the file - if the file doesn't exist,
     we won't gain much time by using mmap on that single occasion. */
  //NVRAM changesdangerous
  if( *online && (u_options & (1<<U_OPTION_MMAP)) )
  {
    ok = (bool_t)0;
    //output = fopen(learner->filename, "r+b");
    output = fopen(learner->filename, "w+");
    if( output ) {
      ok = (bool_t)1;
      if( out_iobuf ) {
        setvbuf(output, (char *)out_iobuf, (int)_IOFBF, (size_t)(BUFFER_MAG * system_pagesize));
      }

      ok = ok && write_category_headers(learner, output);
      if( !ok ) {
        goto skip_mmap;
      }
      /* now mmap the file and write out the arrays real quick */
      mmap_offset = ftell(output);
      if( mmap_offset == (long)-1 ) {
        ok = 0;
        goto skip_mmap;
      }

#ifdef DEBUG
      printf("After write_category_headers \n");
       printf("After ftell(output) %d \n", mmap_offset);
#endif
      mmap_length = (size_t)mmap_offset + (ASIZE * ASIZE * SIZEOF_DIGRAMS) +
        learner->max_tokens * sizeof(c_item_t);

#ifndef NVRAM
      //ok = ok && (-1 != ftruncate(fileno(output), (off_t)mmap_length));
      //if( !ok ) {
        //goto skip_mmap;
     // }
#endif

#ifdef DEBUG
      printf("After truncate %d\n",mmap_length);
#endif
      mmap_start = (byte_t *)mmap(0, mmap_length,
                        PROT_READ|PROT_WRITE, MAP_SHARED, fileno(output), 0);
      if( mmap_start == MAP_FAILED ) { mmap_start = NULL; }
      if( !mmap_start ) {
         printf("mmap failed \n");
        ok = 0;
        goto skip_mmap;
      }
#ifdef DEBUG
      printf("After MMAP\n");
#endif
      MLOCK(mmap_start, mmap_length);
      MADVISE(mmap_start, mmap_length, MADV_SEQUENTIAL|MADV_WILLNEED);

      /* character frequencies */
      shval_ptr = (myweight_t *)(mmap_start + mmap_offset);
      for(i = 0; i < ASIZE; i++) {
        for(j = 0; j < ASIZE; j++) {
          *shval_ptr = HTON_DIGRAM(PACK_DIGRAMS(learner->dig[i][j]));
          shval_ptr++;
        }
      }

      /* token/feature weights */
      ci_ptr = (c_item_t *)(mmap_start + mmap_offset +
                            (ASIZE * ASIZE * SIZEOF_DIGRAMS));
      for(t = 0; t < learner->max_tokens; t++) {
        /* write each element so that it's easy to read back in a
           c_item_t array */
        SET(ci_ptr->id,learner->hash[t].id);
        ci_ptr->lam = learner->hash[t].lam;

        ci_ptr->id = HTON_ID(ci_ptr->id);
        ci_ptr->lam = HTON_LAMBDA(ci_ptr->lam);
        ci_ptr++;
      }

    skip_mmap:
      fclose(output);

      if( mmap_start ) {
        MUNMAP(mmap_start, mmap_length);
        mmap_start = NULL;
      }
    }

    if ( ok ) {
      return 1; /* we're done */
    } else {
      errormsg(E_WARNING, "could not mmap %s, trying stdio.\n",
               learner->filename);
      /* oh oh, file is corrupt. We delete it and try again below
         without mmap */
      unlink(learner->filename);
    }
  }
  /* this keeps track to see if writing is successful, 
     it's not foolproof, but probably good enough */
  ok = (bool_t)1;

  output = mytmpfile(learner->filename, &tempname);
  if( output ) {

    if( out_iobuf ) {
      setvbuf(output, (char *)out_iobuf, (int)_IOFBF, (size_t)(BUFFER_MAG * system_pagesize));
    }
    ok = ok && write_category_headers(learner, output);
    /* end of readable stuff */
    if( ok ) {
      /* now write arrays: this is far(!) from efficient, but the bits
         we need are not nicely tucked into a block of memory, and due
         to the sizes involved when dealing with possibly millions of tokens, we
         can't afford to mymymalloc a brand new block just to pack everything 
         nicely. At least, the file buffering should mitigate the performance
         hit, and we prefer to learn slowly and classify fast. */

      /* character frequencies */
      for(i = 0; i < ASIZE; i++) {
        for(j = 0; j < ASIZE; j++) {
          shval = HTON_DIGRAM(PACK_DIGRAMS(learner->dig[i][j]));
          // LOG(stderr,"learner->dig[i][j] %u \n", PACK_DIGRAMS(learner->dig[i][j]));	 
          for(n = 0; n < 1; ) {
#ifdef STATS
            learner_write_bytes += SIZEOF_DIGRAMS;
#endif
            if( 0 > (n = fwrite(&shval, SIZEOF_DIGRAMS, (size_t)1, output)) ) {
              ok = 0;
              goto skip_remaining;
            }
          }
        }
      }

      MADVISE(learner->hash, sizeof(l_item_t) * learner->max_tokens,
              MADV_SEQUENTIAL|MADV_WILLNEED);

      /* token/feature weights */
      for(t = 0; t < learner->max_tokens; t++) {
        /* write each element so that it's easy to read back in a c_item_t array */
        SET(ci.id,learner->hash[t].id);
        ci.lam = learner->hash[t].lam;

        ci.id = HTON_ID(ci.id);
        ci.lam = HTON_LAMBDA(ci.lam);

        for(n = 0; n < 1; ) {

#ifdef STATS
            learner_write_bytes += sizeof(ci); 
#endif
        if( 0 > (n = fwrite(&ci, sizeof(ci), (size_t)1, output)) ) {
            ok = 0;
            goto skip_remaining;
          }
        }
      }
    }

  skip_remaining:

    fclose(output);

     if(ok)
#ifdef DEBUG
    LOG(stderr,"All functionalities for writing into file worked \n");
#endif

    /* the rename is atomic on posix */
    if( !ok || !myrename(tempname, learner->filename) ) {
      errormsg(E_ERROR,
               "due to a potential file corruption, category %s was not updated\n",
               learner->filename);
      unlink(tempname);
      cleanup.tempfile = NULL;
    }
    myfree(tempname);
  } else {
    errormsg(E_ERROR, "cannot open tempfile for writing %s\n", learner->filename);
    return 0;
  }


  return 1;
}


/* writes the learner to a file for easily readable category */
/* the category file is first constructed as a temporary file,
   then renamed if no problems occured. Because renames are 
   atomic on POSIX, this guarantees that a loaded category file
   used for classifications is never internally corrupt */
error_code_t nvram_save_learner(learner_t *learner) {

  alphabet_size_t i, j;
  hash_count_t t;
  FILE *output;
  char *tempname = NULL;

  bool_t ok;
  size_t n;
  c_item_t ci;
  c_item_t *ci_ptr;
  myweight_t shval;
  myweight_t *shval_ptr = NULL;

  long mmap_offset = 0;
  size_t mmap_length = 0;
  byte_t *mmap_start = NULL;

  char buff_alloc[2048];
  int mmap_len = 0;

  bzero(buff_alloc,2048);
  

  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    LOG(stdout, "saving category to file %s\n", learner->filename);
  }
  
  /* don't overwrite data files */
  /*if( !check_magic_write(learner->filename, (char *)MAGIC1, 10) ) {
    exit(1);
  }*/
  
  /* In case we have both the -m and -o switches we try to write the
     data with mmap. We don't do this in general, because mmap can
     cause corruption, but if -m and -o are used together, we assume the 
     user knows that a single process must read/write the file at a time.
     Also, we don't try to create the file - if the file doesn't exist,
     we won't gain much time by using mmap on that single occasion. */
  //NVRAM changesdangerous
  //if( *online && (u_options & (1<<U_OPTION_MMAP)) )
  {
    ok = (bool_t)0; 
      output = learner->outfp;

    if( output ) {
      ok = (bool_t)1;
#ifdef DEBUG
       LOG(stderr, "Calling nvram_write_category_headers \n");
#endif
      ok = ok && nvram_write_category_headers(learner, output, buff_alloc, &mmap_len);
      if( !ok ) {
		 LOG(stderr, "nvram_write_category_header failed \n");		
         goto skip_mmap;
      }

#ifdef DEBUG
       LOG(stderr, "After nvram_write_category_headers \n");
#endif

      /* now mmap the file and write out the arrays real quick */
      //mmap_offset = ftell(output);
      mmap_offset = mmap_len;

      if( mmap_offset == (long)-1 ) {
                ok = 0;
                goto skip_mmap;
      }
      mmap_length = (size_t)mmap_offset + (ASIZE * ASIZE * SIZEOF_DIGRAMS) +
                                        learner->max_tokens * sizeof(c_item_t);

      mmap_start  = (byte_t *)learner->outmapaddr;	
      if( mmap_start == MAP_FAILED || !mmap_start) { mmap_start = NULL; }
      if( !mmap_start ) {
        LOG(stderr,"mmap failed %d\n", fileno(output));
        ok = 0;
        goto skip_mmap;
      }

      //MLOCK(mmap_start, mmap_length);
      //MADVISE(mmap_start, mmap_length, MADV_SEQUENTIAL|MADV_WILLNEED);
      //LOG(stderr, "mmap_len %d, mmap_length %d buff_alloc %s\n",mmap_len,mmap_length, buff_alloc);

      if(mmap_len){

           memcpy(mmap_start, buff_alloc, mmap_len);
       }

#ifdef DEBUG
      LOG(stderr, "After successful memcpy %d %lu\n", mmap_offset, (unsigned long)mmap_start);
#endif
      /* character frequencies */
      shval_ptr = (myweight_t *)(mmap_start + mmap_offset);
      for(i = 0; i < ASIZE; i++) {
        for(j = 0; j < ASIZE; j++) {
#ifdef STATS
            learner_write_bytes += SIZEOF_DIGRAMS;
#endif
          *shval_ptr = HTON_DIGRAM(PACK_DIGRAMS(learner->dig[i][j]));
          shval_ptr++;
        }
      }

      /* token/feature weights */
      ci_ptr = (c_item_t *)(mmap_start + mmap_offset +
                            (ASIZE * ASIZE * SIZEOF_DIGRAMS));
      for(t = 0; t < learner->max_tokens; t++) {

        /* write each element so that it's easy to read back in a
           c_item_t array */
        SET(ci_ptr->id,learner->hash[t].id);
        ci_ptr->lam = learner->hash[t].lam;

        ci_ptr->id = HTON_ID(ci_ptr->id);
        ci_ptr->lam = HTON_LAMBDA(ci_ptr->lam);
        ci_ptr++;
#ifdef STATS
            learner_write_bytes += sizeof(ci_ptr);
#endif
      }


    skip_mmap:
      //fclose(output);

      if( mmap_start ) {
        //MUNMAP(mmap_start, mmap_length);
        mmap_start = NULL;
      }
    }

    if( ok ) {
      //LOG(stderr,"returning after writing data \n");
      return 1; /* we're done */
    } else {
      errormsg(E_WARNING, "could not mmap %s, trying stdio.\n",
               learner->filename);
      /* oh oh, file is corrupt. We delete it and try again below
         without mmap */
      //NVRAM changes dangerous
      unlink(learner->filename);
    }

  /* this keeps track to see if writing is successful, 
     it's not foolproof, but probably good enough */
  ok = (bool_t)1; 

  output = mytmpfile(learner->filename, &tempname);
  if( output ) {

    if( out_iobuf ) {
      setvbuf(output, (char *)out_iobuf, (int)_IOFBF, (size_t)(BUFFER_MAG * system_pagesize));
    }

    ok = ok && write_category_headers(learner, output);

    /* end of readable stuff */
    if( ok ) {
      /* now write arrays: this is far(!) from efficient, but the bits
	 we need are not nicely tucked into a block of memory, and due
	 to the sizes involved when dealing with possibly millions of tokens, we
	 can't afford to mymymalloc a brand new block just to pack everything 
	 nicely. At least, the file buffering should mitigate the performance
	 hit, and we prefer to learn slowly and classify fast. */

      /* character frequencies */
      for(i = 0; i < ASIZE; i++) {
	for(j = 0; j < ASIZE; j++) {
	  shval = HTON_DIGRAM(PACK_DIGRAMS(learner->dig[i][j]));
	  for(n = 0; n < 1; ) {
	    if( 0 > (n = fwrite(&shval, SIZEOF_DIGRAMS, (size_t)1, output)) ) {
	      ok = 0;
	      goto skip_remaining;
	    } 
	  }	    
	}
      }

      MADVISE(learner->hash, sizeof(l_item_t) * learner->max_tokens, 
	      MADV_SEQUENTIAL|MADV_WILLNEED);

      /* token/feature weights */
      for(t = 0; t < learner->max_tokens; t++) {
	/* write each element so that it's easy to read back in a c_item_t array */
	SET(ci.id,learner->hash[t].id);
	ci.lam = learner->hash[t].lam;

	ci.id = HTON_ID(ci.id);
	ci.lam = HTON_LAMBDA(ci.lam);

	for(n = 0; n < 1; ) {
	  if( 0 > (n = fwrite(&ci, sizeof(ci), (size_t)1, output)) ) {
	    ok = 0;
	    goto skip_remaining;
	  } 
	}
      }
    }

  skip_remaining:
#ifdef DEBUG
    LOG(stderr,"Closing output file \n");
#endif
    //fclose(output);

    /* the rename is atomic on posix */
    if( !ok || !myrename(tempname, learner->filename) ) { 
      errormsg(E_ERROR, 
	       "due to a potential file corruption, category %s was not updated\n", 
	       learner->filename);
      unlink(tempname);
      cleanup.tempfile = NULL;
    }
    myfree(tempname);
  } else {
    errormsg(E_ERROR, "cannot open tempfile for writing %s\n", learner->filename);
    return 0;
  }


 }

  return 1;
}










/* Instead of saving a full new category file, we try to
 * overwrite the contents of an already open, memory mapped category.
 * CAREFUL: THIS CAN CORRUPT THE CATEGORY!
 * If the return value is 0, then you should assume the category no 
 * longer exists on disk.
 */
error_code_t fast_partial_save_learner(learner_t *learner, category_t *xcat) {
  char *p, *q;
#define REPLBUF (3*128)
  char buf[REPLBUF];
  charbuf_len_t n, max;

  alphabet_size_t i, j;
  hash_count_t t;
  c_item_t *ci_ptr;
  myweight_t *shval_ptr;

  if( xcat->mmap_start && 
      (xcat->m_options == learner->m_options) &&
      (xcat->max_order == learner->max_order) &&
      (xcat->max_hash_bits == learner->max_hash_bits) ) {
    max = 0;
    buf[0] = '\0';
    /* we only overwrite the header if there's exactly enough space */
    /* we must overwrite 3 lines which all start with # */
    q = p = strchr((char *)xcat->mmap_start + 1, '#');
    if( p ) {
      n = snprintf(buf, (size_t)(REPLBUF - max), MAGIC2_o, 
		   learner->divergence, learner->logZ, 
		   (short int)learner->max_order,
		   (m_options & (1<<M_OPTION_MULTINOMIAL)) ? "multinomial" : "hierarchical" );
      max += n;
    }
    q = strchr(q + 1, '#');
    if( q && (max < REPLBUF) ) {
      n = snprintf(buf + max, (size_t)(REPLBUF - max), MAGIC3, 
		   (short int)learner->max_hash_bits, 
		   (long int)learner->full_token_count, 
		   (long int)learner->unique_token_count,
		   (long int)(learner->doc.count));
      max += n;
    }
    q = strchr(q + 1, '#');
    if( q && (max < REPLBUF) ) {
      n = snprintf(buf + max, (size_t)(REPLBUF - max), MAGIC8_o, 
		   learner->shannon, 
		   learner->alpha, learner->beta,
		   learner->mu, learner->s2);
      max += n;
    }
    /* now check it fits into the existing space */
    q = strchr(q + 1, '#');
    if( (q - p) == max ) {
      /* from here on, there's no turning back. Since we don't
        overwrite everything, if there's a problem, we'll have to
        unlink the category */

      /* header */
      memcpy(p, buf, (size_t)max);

      /* character frequencies */
      shval_ptr = (myweight_t *)(xcat->mmap_start + xcat->mmap_offset - 
				 ASIZE * ASIZE * sizeof(myweight_t));
      for(i = 0; i < ASIZE; i++) {
		for(j = 0; j < ASIZE; j++) {
		  *shval_ptr = HTON_DIGRAM(PACK_DIGRAMS(learner->dig[i][j]));
		  shval_ptr++;
	}
      }
      /* token/feature weights 
	 (we assume cat->max_tokens <= learner->max_tokens, and
	 that filled hash values in cat are also filled in learner) */
      ci_ptr = (c_item_t *)(xcat->mmap_start + xcat->mmap_offset);
      for(t = 0; t < learner->max_tokens; t++) {
	/* write each element so that it's easy to read back in a
	   c_item_t array */
	SET(ci_ptr->id,learner->hash[t].id);
	ci_ptr->lam = learner->hash[t].lam;

	ci_ptr->id = HTON_ID(ci_ptr->id);
	ci_ptr->lam = HTON_LAMBDA(ci_ptr->lam);
	ci_ptr++;
      }

      if( u_options & (1<<U_OPTION_VERBOSE) ) {
	LOG(stdout, "partially overwriting category file %s\n",
		learner->filename);
      }
      return 1;
    }
  }
  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    LOG(stdout, "can't make fast save\n");
  }
  return 0;
}


bool_t tmp_seek_start(learner_t *learner) {
  if( learner->tmp.mmap_start ) {
    learner->tmp.mmap_cursor = learner->tmp.mmap_offset;
    return (bool_t)1;
  } else if( learner->tmp.file ) {
    clearerr(learner->tmp.file);
    return (fseek(learner->tmp.file, learner->tmp.offset, SEEK_SET) == 0);
  }
  return (bool_t)0;
}

bool_t tmp_seek_end(learner_t *learner) {
  if( learner->tmp.mmap_start ) {
    learner->tmp.mmap_cursor = learner->tmp.mmap_offset + learner->tmp.used;
    return (bool_t)1;
  } else if( learner->tmp.file ) {
    return (fseek(learner->tmp.file, 
		  learner->tmp.offset + learner->tmp.used, SEEK_SET) == 0);
  }
  return (bool_t)0;
}

long tmp_get_pos(learner_t *learner) {
  if( learner->tmp.mmap_start ) {
    return learner->tmp.mmap_cursor - learner->tmp.mmap_offset;
  } else if( learner->tmp.file ) {
    return ftell(learner->tmp.file) - learner->tmp.offset;
  }
  return (bool_t)0;
}

size_t tmp_read_block(learner_t *learner, byte_t *buf, size_t bufsiz,
		      const byte_t **startp) {
  long left = learner->tmp.used - tmp_get_pos(learner);
  if( bufsiz > (size_t)left ) { bufsiz = (left >= 0) ? (size_t)left : 0; }
  if( learner->tmp.mmap_start ) {
/*     memcpy(buf, learner->tmp.mmap_start + learner->tmp.mmap_cursor, bufsiz); */
    *startp = learner->tmp.mmap_start + learner->tmp.mmap_cursor;
    learner->tmp.mmap_cursor += bufsiz;
    return bufsiz;
  } else if( learner->tmp.file ) {
    if( ferror(learner->tmp.file) || feof(learner->tmp.file) ) {
      return 0;
    }
    *startp = buf;
    return fread(buf, 1, bufsiz, learner->tmp.file);
  }
  return 0;
}

/* must unmap/ftruncate/remap if using mmap, don't touch mmap_cursor */
bool_t tmp_grow(learner_t *learner) {
  long offset;
  if( learner->tmp.file &&
      ((learner->tmp.used + 2 * MAX_TOKEN_LEN) >= learner->tmp.avail) ) {

    if( learner->tmp.mmap_start ) {
      MUNLOCK(learner->tmp.mmap_start, learner->tmp.mmap_length);
      MUNMAP(learner->tmp.mmap_start, learner->tmp.mmap_length);
    }

    learner->tmp.avail += TOKEN_LIST_GROW;

    /*if( -1 == ftruncate(fileno(learner->tmp.file), 
			learner->tmp.offset + learner->tmp.avail) ) {
      errormsg(E_FATAL,
	       "could not resize the tempfile, unable to proceed.\n"); 
    }*/

    /* mmap_start is still old value, but invalid */
    if( learner->tmp.mmap_start ) { 
      offset = PAGEALIGN(learner->tmp.offset);
      learner->tmp.mmap_length = learner->tmp.avail + system_pagesize;
      learner->tmp.mmap_start =
	(byte_t *)MMAP(learner->tmp.mmap_start, learner->tmp.mmap_length,
		       PROT_READ|PROT_WRITE, MAP_SHARED,
		       fileno(learner->tmp.file), offset);
      if( learner->tmp.mmap_start == MAP_FAILED ) { learner->tmp.mmap_start = NULL; }
      if( !learner->tmp.mmap_start ) {
	if( u_options & (1<<U_OPTION_VERBOSE) ) {
	  errormsg(E_WARNING, "could not mmap token file after resize\n");
	}
	/* this isn't fatal, we just switch to ordinary file ops, since
	 the file is still open :) */
	if( -1 == fseek(learner->tmp.file,learner->tmp.offset + 
			(learner->tmp.mmap_cursor - learner->tmp.mmap_offset),
			SEEK_SET) ) {
	  /* ok, this is fatal */
	  errormsg(E_FATAL,
		   "could not resize and seek in tempfile, unable to proceed.\n");
	}
	learner->tmp.mmap_length = 0;
	learner->tmp.mmap_offset = 0;
	learner->tmp.mmap_cursor = 0;
      } else {
	/* all is well, relock the pages */
	MLOCK(learner->tmp.mmap_start, learner->tmp.mmap_length);
	MADVISE(learner->tmp.mmap_start, learner->tmp.mmap_length,
		MADV_SEQUENTIAL|MADV_WILLNEED);
      }
      /* if there was a fatal error, we simply don't return */
      return (bool_t)1;
    }
  }
  return (bool_t)0;
}

/* assume we are at the end of the token list and there is enough room */
void tmp_write_token(learner_t *learner, const char *tok) {
  byte_t *p;

  if(!learner->tmp.mmap_start) {
#ifdef NVRAM
    initialize_tmp_file();
#endif
    //LOG(stderr,"initializing temp file \n");
  }


  if( learner->tmp.mmap_start ) {

     //LOG(stderr,"using learner->tmp.mmap_start \n");

    p = learner->tmp.mmap_start + learner->tmp.mmap_cursor;
    while( *tok ) { *p++ = *tok++; }
    *p++ = TOKENSEP;
    learner->tmp.mmap_cursor = p - learner->tmp.mmap_start;
    learner->tmp.used = learner->tmp.mmap_cursor - learner->tmp.mmap_offset;
  } else if( learner->tmp.file ) {
    /* now save token to temporary file */
    if( (EOF == fputs(tok, learner->tmp.file)) ||
	(EOF == fputc(TOKENSEP, learner->tmp.file)) ) {
      errormsg(E_ERROR, 
	       "could not write a token to tempfile, calculations will be skewed.\n");
    } else {
      learner->tmp.used += (strlen(tok) + 1);
    }
  }
}

void tmp_close(learner_t *learner) {
  if( learner->tmp.mmap_start ) {
    //MUNLOCK(learner->tmp.mmap_start, learner->tmp.mmap_length);
    //MUNMAP(learner->tmp.mmap_start, learner->tmp.mmap_length);
    learner->tmp.mmap_start = NULL;
  }
  if( learner->tmp.file ) {
    fclose(learner->tmp.file);
    learner->tmp.file = NULL;
    if( learner->tmp.filename ) { 
      unlink(learner->tmp.filename);
      cleanup.tempfile = NULL;
      myfree(learner->tmp.filename);
      learner->tmp.filename = NULL;
    }
  }
}

/***********************************************************
 * EMPLIST FUNCTIONS                                       *
 ***********************************************************/
bool_t emplist_grow(emplist_t *emp) {
  hash_count_t *n;

  if( emp ) {
    if( u_options & (1<<U_OPTION_VERBOSE) ) {
      errormsg(E_WARNING, "growing emp.stack %d\n", 
	       emp->max * 2);
    }

    n = (hash_value_t *)realloc(emp->stack,
				sizeof(hash_value_t *) * emp->max * 2);
    if( n != NULL ) {
      emp->max *= 2;
      emp->stack = n;
      return 1;
    } 
  }
  errormsg(E_WARNING,
	   "disabling emp.stack. Not enough memory? I couldn't allocate %li bytes\n", 
	   (sizeof(l_item_t *) * ((long int)emp->max) * 2));
  m_options &= ~(1<<M_OPTION_CALCENTROPY);

  return 0;
}

void emplist_replace(emplist_t *reservoir, document_count_t n, emplist_t *emp) {
  if( reservoir[n].stack && (emp->top > reservoir[n].max) ) {
    myfree(reservoir[n].stack);
    reservoir[n].stack = NULL;
  }
  if( !reservoir[n].stack ) {
    for(reservoir[n].max = 1; emp->top > reservoir[n].max; 
	reservoir[n].max *=2);
    reservoir[n].stack = (hash_value_t *)mymalloc(reservoir[n].max * sizeof(hash_value_t));
  }
  if( reservoir[n].stack ) {
    memcpy(reservoir[n].stack, emp->stack, emp->top * sizeof(hash_value_t));
    reservoir[n].top = emp->top;
    reservoir[n].shannon = emp->shannon;
  } else {
    errormsg(E_WARNING,
	     "couldn't replace document #%d in reservoir, ignoring\n", n);
  }
}

/*
 * Adds emp to the reservoir in such a way that we 
 * end up with a uniform random sample of size RESERVOIR_SIZE.
 * This algorithm is a short form of reservoir sampling, and we use
 * it here because we don't know the total number of documents 
 * until it's too late.
 */

void emplist_add_to_reservoir(emplist_t *reservoir, document_count_t docno, 
			      emplist_t *emp) {
  double xi;
  if( docno < RESERVOIR_SIZE) {
    emplist_replace(reservoir, docno, emp);
  } else {
    xi = ((double)rand()/(RAND_MAX + 1.0));
    if( (docno + 1) * xi < RESERVOIR_SIZE ) {
      xi = ((double)rand()/(RAND_MAX + 1.0));
      emplist_replace(reservoir, (int)(RESERVOIR_SIZE * xi), emp);
    }
  }
}

/***********************************************************
 * LEARNER FUNCTIONS                                       *
 ***********************************************************/
void myfree_learner_hash(learner_t *learner) {
  if( learner->hash ) {
    if( learner->mmap_start != NULL ) {
      MUNMAP(learner->mmap_start, 
	     learner->mmap_hash_offset + cat->max_tokens * sizeof(c_item_t));
      learner->mmap_start = NULL;
      learner->mmap_learner_offset = 0;
      learner->mmap_hash_offset = 0;
      learner->hash = NULL;
    }
    if( learner->hash ) {
      myfree(learner->hash);
      learner->hash = NULL;
    }
  }
}

bool_t create_learner_hash(learner_t *learner, FILE *input) {
  size_t j, n;
  byte_t *mmap_start;
  long mmap_learner_offset;
  long mmap_hash_offset;
  
  if( learner->hash ) { myfree_learner_hash(learner); }

  if( u_options & (1<<U_OPTION_MMAP) ) {
    /* here we mmap the fixed size portion of the dump file. First
       we mmap the learner structure portion as a fast way of loading 
       the learner variable, then we must undo/redo the mmap because
       we cannot know the hash table size until we've read the dumped learner_t.
    */

    LOG(stderr,"create_learner_hash Entering %d \n",fileno(input));

    mmap_learner_offset = strlen(MAGIC_ONLINE);
    mmap_hash_offset = mmap_learner_offset + sizeof(learner_t);

    mmap_start =
      (byte_t *)mmap(0, mmap_hash_offset,
		     PROT_READ|PROT_WRITE, MAP_SHARED, fileno(input), 0);
    if( mmap_start == MAP_FAILED ) { mmap_start = NULL; }
    if( mmap_start ) {
      /* first we overwrite the learner struct with the contents of
	 the mmapped region */
      memcpy(learner, mmap_start + mmap_learner_offset, sizeof(learner_t));
      
      MUNMAP(mmap_start, mmap_hash_offset);

#ifdef DEBUG
      //LOG(stderr,"create_learner_hash: After memcpy %d \n",learner->out_fd); 
#endif
      
      mmap_start =
	(byte_t *)mmap(mmap_start,
		       mmap_hash_offset + sizeof(l_item_t) * learner->max_tokens,
		       PROT_READ|PROT_WRITE, MAP_SHARED, fileno(input), 0);
      if( mmap_start == MAP_FAILED ) { mmap_start = NULL; }
      if( mmap_start ) {
	/* now fill some member variables */
	learner->mmap_start = mmap_start;
	learner->mmap_learner_offset = mmap_learner_offset;
	learner->mmap_hash_offset = mmap_hash_offset;
	learner->hash = (l_item_t *)(mmap_start + mmap_hash_offset);
	MADVISE(learner->mmap_start, 
		learner->mmap_hash_offset + sizeof(l_item_t) * learner->max_tokens,
		MADV_SEQUENTIAL|MADV_WILLNEED);
	/* lock the pages to prevent swapping - on Linux, this
	   works without root privs so long as the user limits
	   are big enough - mine are unlimited ;-) 
	   On other OSes, root may me necessary. */
	MLOCK(learner->mmap_start,
	      learner->mmap_hash_offset + sizeof(l_item_t) * learner->max_tokens);
      }
    }
  }

  if( !learner->hash ) {
    
    for(n = 0; n < 1; ) {
      n = fread(learner, sizeof(learner_t), 1, input);
      if( (n == 0) && ferror(input) ) {
	return 0;
      }
    }

    /* allocate hash table normally */
    learner->hash = (l_item_t *)mymalloc(learner->max_tokens * sizeof(l_item_t));
    if( !learner->hash ) {
      errormsg(E_WARNING, "failed to allocate %li bytes for learner hash.\n",
	       (sizeof(l_item_t) * ((long int)learner->max_tokens)));
      /* this error is not fatal, because we have another opportunity to 
	 allocate the hash later */
      return 0;
    }

    MADVISE(learner->hash, sizeof(l_item_t) * learner->max_tokens, 
	    MADV_SEQUENTIAL);

    for(n = j = 0; n < learner->max_tokens; n += j) {
      j = fread(&learner->hash[n], sizeof(l_item_t), 
		learner->max_tokens - n, input);
      if( (j == 0) && ferror(input) ) {
	errormsg(E_WARNING, "could not read online learner dump, ignoring.\n");
	return 0;
      }
    }

  }
  return (learner->hash != NULL);
}

/* returns true if the hash could be grown, false otherwise.
   When the hash is grown, the old values must be redistributed.
   To do this, we reuse the l_item.lam member as a flag, since this
   is only needed later during minimization */
bool_t grow_learner_hash(learner_t *learner) {
  hash_count_t c, new_size;
  l_item_t *i, temp_item;

  if( !(u_options & (1<<U_OPTION_GROWHASH)) ) {
    return 0;
  } else {
    if( learner->max_hash_bits < default_max_grow_hash_bits ) {

      /* 
	 this warning is mostly useless and distracting

	 errormsg(E_WARNING,
	 "growing hash table, next time perhaps consider using -h %d\n",
	 learner.max_hash_bits + 1);

      */

      /* there are two cases - if learner is mmapped, we must mymalloc a
       * new hash, otherwise we can realloc. An easy way to reduce the
       * first case to the second is to create a throwaway hash with
       * the old size and copy the mmapped contents to it. 
       */
      if( learner->mmap_start ) {
	if( (i = (l_item_t *)mymalloc(learner->max_tokens * 
				    sizeof(l_item_t))) == NULL ) {
	  errormsg(E_WARNING,
		   "failed to mymalloc hash table for growing.\n");
	  return 0;
	}
	memcpy(i, learner->hash, sizeof(l_item_t) * learner->max_tokens);
	myfree_learner_hash(learner);
	learner->hash = i;

	/* there's a MUNLOCK call below, so in theory we should
	   balance this out by calling MLOCK here, since the i pointed
	   region was never locked. However mlock/munlock calls do not nest,
	   so there's no need. 
	*/
/* 	if( u_options & (1<<U_OPTION_MMAP) ) { */
/* 	  MLOCK(learner->hash, sizeof(l_item_t) * learner->max_tokens); */
/* 	} */
      }


      if( u_options & (1<<U_OPTION_MMAP) ) {
	MUNLOCK(learner->hash, sizeof(l_item_t) * learner->max_tokens);
      }

      /* grow the memory around the hash */
      if( (i = (l_item_t *)realloc(learner->hash, 
		       sizeof(l_item_t) * 
		       (1<<(learner->max_hash_bits+1)))) == NULL ) {
	errormsg(E_WARNING,
		"failed to grow hash table.\n");
	return 0;
      }
      /* we need the old value of learner->max_tokens for a while yet */
      learner->max_hash_bits++;

      if( u_options & (1<<U_OPTION_MMAP) ) {
	MLOCK(i, sizeof(l_item_t) * (1<<learner->max_hash_bits));
      }

      MADVISE(i, sizeof(l_item_t) * (1<<learner->max_hash_bits), MADV_SEQUENTIAL);

      /* realloc doesn't initialize the memory */
      memset(&((l_item_t *)i)[learner->max_tokens], 0, 
	     ((1<<learner->max_hash_bits) - learner->max_tokens) * 
	     sizeof(l_item_t));
      learner->hash = i; 
      
      /* now mark every used slot */
      for(c = 0; c < learner->max_tokens; c++) {
	if( FILLEDP(&learner->hash[c]) ) {
	  SETMARK(&learner->hash[c]);
	}
      }

      MADVISE(i, sizeof(l_item_t) * (1<<learner->max_hash_bits), MADV_RANDOM);

      /* now relocate each marked slot and clear it */
      new_size = (1<<learner->max_hash_bits) - 1;
      for(c = 0; c < learner->max_tokens; c++) {
	while( MARKEDP(&learner->hash[c]) ) {
	  /* find where it belongs */
	  i = &learner->hash[learner->hash[c].id & new_size];
	  while( FILLEDP(i) && !MARKEDP(i) ) {
	    i++;
	    i = (i > &learner->hash[new_size]) ? learner->hash : i;
	  } /* guaranteed to exit since hash is larger than original */

	  /* clear the mark - this must happen after we look for i,
	   since it should be possible to find i == learner->hash[c] */
	  UNSETMARK(&learner->hash[c]); 

	  /* now refile */
	  if( i != &learner->hash[c] ) {
	    if( MARKEDP(i) ) {
	      /* swap */
	      memcpy(&temp_item, i, sizeof(l_item_t));
	      memcpy(i, &learner->hash[c], sizeof(l_item_t));
	      memcpy(&learner->hash[c], &temp_item, sizeof(l_item_t));
	    } else {
	      /* copy and clear */
	      memcpy(i, &learner->hash[c], sizeof(l_item_t));
	      memset(&learner->hash[c], 0, sizeof(l_item_t));
	    }
	  } 
	  /* now &learner->hash[c] is marked iff there was a swap */
	}
      }
      learner->max_tokens = (1<<learner->max_hash_bits);
    } else {
      u_options &= ~(1<<U_OPTION_GROWHASH); /* it's the law */
      errormsg(E_WARNING,
	       "the token hash table is nearly full, slowing down.\n"); 
    }
  }
  return 1;
}




/* This code mymalloc()s and fread()s the learner structure. */
bool_t read_online_learner_struct(learner_t *learner, char *path) {
  FILE *input;
  char buf[BUFSIZ+1]; /* must be greater than MAGIC_BUFSIZE */
  bool_t ok = 0;
  char *sav_filename;
  long offset;
  
#ifdef DEBUG
   LOG(stderr, "read_online_learner_struct getting called %s\n", path);
#endif
  /* 
   * This code mymalloc()s and fread()s the learner structure, or alternatively
   * it mmap()s it for speed. Note we open for read/write, because the
   * file might be left open and reused later.
   */
#ifdef NVRAM

#else
  input = fopen(path, "r+b");
#endif

  if( input ) {

    if( out_iobuf ) {
      setvbuf(input, (char *)out_iobuf, (int)_IOFBF, (size_t)(BUFFER_MAG * system_pagesize));
    }

    if( !fgets(buf, MAGIC_BUFSIZE, input) ||
	(strncmp(buf, MAGIC_ONLINE, strlen(MAGIC_ONLINE)) != 0) ) {
	if( strncmp(buf, MAGIC_ONLINE, 35) == 0 ) {
	  errormsg(E_WARNING,
		  "the file %s has the wrong version number, it will be ignored\n", 
		  path);
	} else {
	  errormsg(E_WARNING,
		  "the file %s is not a dbacl online memory dump, it will be ignored\n", 
		  path);
	}
    } else {

      /* from here on, if a problem arises then learner is hosed, so we exit */
      ok = 1;

      /* must save some prefilled members because learner is read from disk */
      sav_filename = learner->filename;

      if( !create_learner_hash(learner, input) ) {

         LOG(stderr,"read_online_learner_struct: skip_read_online \n");
	ok = 0;
	goto skip_read_online;
      }

#ifdef DEBUG
      //LOG(stderr,"read_online_learner_struct:finished setting filename and few others %d\n",  learner->out_fd);
#endif

      /* restore members */
      learner->filename = sav_filename;

      /* override options */
      if( m_options != learner->m_options ) {
	/* we don't warn about changes in u_options, as they can happen
	   during computations */
	errormsg(E_WARNING, 
		 "some command line options were changed when loading %s\n",
		 path);
      }
      m_options = learner->m_options;
      u_options = learner->u_options;

      /* last, there is a long list of token strings, we compute the
       start offset, and seek to the end so we can append more
       tokens */
      learner->tmp.file = input;
      learner->tmp.filename = NULL;
      learner->tmp.offset = strlen(MAGIC_ONLINE) + sizeof(learner_t) + 
	sizeof(l_item_t) * learner->max_tokens;
      learner->tmp.iobuf = NULL;

      //if( u_options & (1<<U_OPTION_MMAP) ) {
	offset = PAGEALIGN(learner->tmp.offset);
	/* since offset can be up to a page less than the true beginning,
	   we must allow an extra page in the mmap length */
        LOG(stderr,"read_online_learner_struct: Setting tmp.mmap \n");
	learner->tmp.mmap_length = learner->tmp.avail + system_pagesize;
	learner->tmp.mmap_start = 
	  (byte_t *)mmap(0, learner->tmp.mmap_length,
			 PROT_READ|PROT_WRITE, MAP_SHARED,
			 fileno(learner->tmp.file), offset);
	if( learner->tmp.mmap_start == MAP_FAILED ) { learner->tmp.mmap_start = NULL; }
	if( learner->tmp.mmap_start ) {
	  MLOCK(learner->tmp.mmap_start, learner->tmp.mmap_length);
	  MADVISE(learner->tmp.mmap_start, learner->tmp.mmap_length,
		  MADV_SEQUENTIAL|MADV_WILLNEED);
	  learner->tmp.mmap_offset = learner->tmp.offset - offset;
	  learner->tmp.mmap_cursor = 0;
	} else {
	  learner->tmp.mmap_length = 0;
	  if( u_options & (1<<U_OPTION_VERBOSE) ) {
	    errormsg(E_WARNING, "couldn't mmap temporary tokens\n");
	  }
	}
      //}

      if( !tmp_seek_end(learner) ) {
	ok = 0;
	learner->tmp.file = NULL;
	learner->tmp.filename = NULL;
	goto skip_read_online;
      }
      /* consistency check */
      if( tmp_get_pos(learner) != learner->tmp.used ) {
	ok = 0;
	learner->tmp.file = NULL;
	learner->tmp.filename = NULL;
	goto skip_read_online;
      }
/*       printf("tmp.used = %ld tmp.avail = %ld tmp.offset = %ld\n", */
/* 	     learner->tmp.used, learner->tmp.avail, learner->tmp.offset); */
    }
  skip_read_online:
    /* if we're ok, we must leave the file open, even if it's mmapped,
     because we must be able to unmap/remap it on the fly */
    if( !ok ) {
      if( learner->tmp.mmap_start ) {
	MUNMAP(learner->tmp.mmap_start, learner->tmp.mmap_length);
	learner->tmp.mmap_start = 0;
	learner->tmp.mmap_length = 0;
	learner->tmp.mmap_offset = 0;
	learner->tmp.mmap_cursor = 0;
      }
      fclose(input); 
      errormsg(E_FATAL,
	       "the file %s could not be read, learning from scratch.\n", 
	       path);
    }
  }else{
#ifdef DEBUG
	LOG(stderr,"Read online struct failure \n");
#endif
  }


  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    LOG(stdout, "%s %s\n", ok ? "loaded" : "couldn't load", path);
  }

  return ok;
}

/* this is a straight memory dump  */
void write_online_learner_struct(learner_t *learner, char *path) {
  FILE *output;
  char *tempname = NULL;
  bool_t ok;
  byte_t buf[BUFSIZ+1];
  size_t t, j, n;
  learner_t *mml;
  long tokoff;
  const byte_t *sp;

  /*if( !check_magic_write(path, (char *)MAGIC_ONLINE, strlen(MAGIC_ONLINE)) ) {
    // we simply ignore this. Note that check_magic_write() already
    //   notifies the user 
    return;
  }*/

  /* if the hash table is memory mapped, then we simply update the 
     learner structure and exit */

  if( learner->mmap_start != NULL ) {
    mml = (learner_t *)(learner->mmap_start + learner->mmap_learner_offset);
    memcpy(mml, learner, sizeof(learner_t));
    /* clear some variables just to be safe */
    mml->mmap_start = NULL;
    mml->mmap_learner_offset = 0;
    mml->mmap_hash_offset = 0;
    mml->hash = NULL;
#ifdef DEBUG
    LOG(stderr,"write_online_learner_struct: using learner->mmap_start to write data \n");
#endif
    return;
  }


  /* if we're here, then we must write out a completely new memory dump,
   * and the mmap_ variables are all zero. */
   LOG(stderr,"Creating temp file %s \n", path); 
  output = mytmpfile(path, &tempname);
  if( output ) {

    if( out_iobuf ) {
      setvbuf(output, (char *)out_iobuf, (int)_IOFBF, (size_t)(BUFFER_MAG * system_pagesize));
    }

    ok = 1;
    ok = ok &&
      (0 < fprintf(output, MAGIC_ONLINE));

    /* save current model options - don't want them to change next time we learn */
    learner->m_options = m_options;
    learner->u_options = u_options;

    /* make sure some stuff is zeroed out */
    learner->mmap_start = NULL; /* assert mmap_start == NULL */
    learner->mmap_learner_offset = 0;
    learner->mmap_hash_offset = 0;
    learner->doc.emp.top = 0;
    learner->doc.emp.max = 0;
    learner->doc.emp.stack = NULL;
    memset(learner->doc.reservoir, 0, RESERVOIR_SIZE * sizeof(emplist_t));

    /* write learner */
    ok = ok &&
      (0 < fwrite(learner, sizeof(learner_t), 1, output));
    for(t = j = 0; t < learner->max_tokens; t += j) {
      j = fwrite(&(learner->hash[t]), sizeof(l_item_t), 
		 learner->max_tokens - t, output);
      if( (j == 0) && ferror(output) ) {
	ok = 0;
	goto skip_write_online;
      }
    }

    tokoff = ftell(output);
    /* extend the size ofthe file - this is needed mostly for mmapping,
     but it might also marginally speed up the repeated writes below */
    /*if( -1 == ftruncate(fileno(output), tokoff + learner->tmp.avail) ) {
      ok = 0;
      goto skip_write_online;
    }*/

    /* write temporary tokens */
    if( !tmp_seek_start(learner) ) {
      errormsg(E_ERROR, "cannot seek in online token file %s\n", tempname);
      ok = 0;
      goto skip_write_online;
    }
    n = 0;
    while( (n = tmp_read_block(learner, buf, BUFSIZ, &sp)) > 0 ) {
      for(t = j = 0; t < n; t += j) {
	j = fwrite(sp + t, 1, n - t, output);
	if( (j == 0) && ferror(output) ) {
	  ok = 0;
	  goto skip_write_online;
	}
      }
    }
    /* consistency check */
    if( (tokoff + learner->tmp.used) != ftell(output) ) {
      ok = 0;
      goto skip_write_online;
    } 

  skip_write_online:
    fclose(output);

    if( u_options & (1<<U_OPTION_VERBOSE) ) {
      LOG(stdout, "writing online memory dump: %s\n", ok ? "success" : "failed");
    }

    /* the rename is atomic on posix */
    if( !ok || (rename(tempname, path) < 0) ) {
      errormsg(E_ERROR,
	       "due to a potential file corruption, %s was not updated\n", 
	       path);
      unlink(tempname);
    }

    myfree(tempname);
  }
}


/* returns an approximate binomial r.v.; if np < 10 and n > 20, 
 * a Poisson approximation is used, else the variable is exact.
 */   
token_count_t binomial(token_count_t n, double p) {
  double sum, thresh;
  token_count_t i, x;

  x = 0;
  if( (n > 20) && (n * p < 10) ) {
    /* approximate poisson has lambda = np */
    sum = -log( ((double)rand())/(RAND_MAX + 1.0) );
    thresh = n * p;
    /* printf("poisson with sum = %g thresh = %g\n", sum, thresh);       */
    while( (sum < thresh) && (x < n) ) {
      x++;
      sum += -log( ((double)rand())/(RAND_MAX + 1.0) );
      /* printf("poisson with sum = %g thresh = %g, x = %d\n", sum, thresh, x); */
    }
  } else {
    /* direct calculation of binomial */
    for(i = 0; i < n; i++) {
      x += (int)(p + ((double)rand())/(RAND_MAX + 1.0));
    }
  }
/*   if( x > 0 ) { printf("binomial x = %d, n = %d, p = %g, np = %g\n", x, n, p, n * p); } */
  return x;
}

void update_shannon_partials(learner_t *learner) {
  hash_count_t i;
  weight_t ell, lell;

  if( m_options & (1<<M_OPTION_CALCENTROPY) ) {

    learner->doc.emp.shannon = 0;
    if( learner->doc.emp.top > 0 ) {
      /* assume all marks are zero to start with */
      for(i = 0; i < learner->doc.emp.top; i++) {
	l_item_t *p = find_in_learner(learner, learner->doc.emp.stack[i]);
	if( p && !MARKEDP(p) ) {
	  ell = ((weight_t)p->tmp.read.eff)/learner->doc.emp.top;

	  /* it would be nice to be able to digitize p->B, but ell is
	     often smaller than the smallest value, and if there are
	     many documents, there could simultaneously be overflow on the most
	     frequent features. So we need p->B to be a floating point type. */
	  p->B += ell;

	  /* the standard entropy convention is that 0*inf = 0. here, if
	   * ell is so small that log(ell) is infinite, we pretend ell
	   * == 0 this is slightly better than testing for ell == 0
	   * directly.
	   */

	  lell = -log(ell);
	  if( !isinf(lell) ) {
	    learner->doc.emp.shannon += ell * lell;
	  }

	  SETMARK(p);
	}
      }

      emplist_add_to_reservoir(learner->doc.reservoir, 
			       learner->doc.count, 
			       &learner->doc.emp);

      /* clear the empirical counts and marks */
      for(i = 0; i < learner->doc.emp.top; i++) {
	l_item_t *p = find_in_learner(learner, learner->doc.emp.stack[i]);
	if( p && MARKEDP(p) ) {
	  p->tmp.read.eff = 0;
	  UNSETMARK(p);
	}
      }

      learner->doc.A += -learner->doc.emp.shannon;
    }

    /* now reset the empirical features */
    learner->doc.emp.top = 0;

  }
}

void calc_shannon(learner_t *learner) {
  l_item_t *i, *e;
  document_count_t c, n;
  hash_count_t q;
  emplist_t *empl;
  score_t score;
  score_t effective_count = 0.0;
  double Lambda, jensen, s2, mu;
 

  learner->mu = 0.0;
  learner->s2 = 0.0;
  learner->alpha = 0.0;
  learner->beta = 0.0;
  
  if( (m_options & (1<<M_OPTION_CALCENTROPY)) &&
      (learner->doc.count > 0) ) {

    effective_count = learner->doc.count;
    /* shannon was computed during update */
    jensen = 0.0;
    e = learner->hash + learner->max_tokens;
    for(i = learner->hash; i != e; i++) {
      if( NOTNULL(i->lam) ) {
	Lambda = UNPACK_LAMBDA(i->lam);
	if( i->typ.order == 1 ) {
	  Lambda += UNPACK_RWEIGHTS(i->tmp.min.dref) - learner->logZ;
	}
	learner->mu += (Lambda * i->B);
	jensen += (Lambda * Lambda * i->B);
      }
    }

    learner->mu /= effective_count;
    learner->shannon = -(learner->doc.A/effective_count);
    learner->mu = -(learner->shannon + learner->mu);

    mu = 0.0;
    s2 = 0.0;
    n = 1;
    for(c = 0; c < RESERVOIR_SIZE; c++) {
      empl = &(learner->doc.reservoir[c]);
      if( !empl->stack  ) {
	n++;
	continue;
      } else {
	score = 0;
	for(q = 0; q < empl->top; q++) {
	  i = find_in_learner(learner, empl->stack[q]);
	  if( i && NOTNULL(i->lam) ) {
	    Lambda = UNPACK_LAMBDA(i->lam);
	    if( i->typ.order == 1 ) {
	      Lambda += UNPACK_RWEIGHTS(i->tmp.min.dref) - learner->logZ;
	    }
	    score += Lambda;
	  }
	}

	score = score/empl->top + empl->shannon;
	if( u_options & (1<<U_OPTION_VERBOSE) &&
	    u_options & (1<<U_OPTION_CONFIDENCE) ) {
	  LOG(stdout, 
		  "%s ( %5.2" FMT_printf_score_t " + "
		  "%-5.2" FMT_printf_score_t " )* %-d \n",
		  "reservoir",
		  -nats2bits(score),
		  nats2bits(empl->shannon),
		  empl->top);
	}

	score = -score - learner->mu;
	s2 += (score)*(score);
      }
    }

    /* the reservoir doesn't contain enough extreme samples, so
       the variance is necessarily underestimated. 
    */
    if( (c <= n) || isinf(learner->s2) ) {
      learner->s2 = 0.0;
      learner->alpha = 0.0;
      learner->beta = 0.0;
    } else {
      learner->s2 = s2/(c-n);
      learner->alpha = (learner->mu*learner->mu)/learner->s2;
      learner->beta = learner->s2/learner->mu;
    }

  } else {

    learner->shannon = 0.0;
    e = learner->hash + learner->max_tokens;
    for(i = learner->hash; i != e; i++) {
      if( FILLEDP(i) && 
	  (i->typ.order == learner->max_order) ) {
	learner->shannon += log((weight_t)i->count) * (weight_t)i->count; 
      }
    }
    learner->shannon = 
      -( learner->shannon/learner->full_token_count -
	 log((weight_t)learner->full_token_count) );

  }

  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    LOG(stdout, 
	    "documents %ld shannon %" FMT_printf_score_t 
	    " mu %" FMT_printf_score_t 
	    " s2 %" FMT_printf_score_t 
	    " alpha %" FMT_printf_score_t 
	    " beta %" FMT_printf_score_t "\n",
	    (long int)effective_count, 
	    nats2bits(learner->shannon), 
	    nats2bits(learner->mu), 
	    nats2bits(learner->s2), 
	    nats2bits(learner->alpha),
	    nats2bits(learner->beta));
  }

}

void reset_mbox_messages(learner_t *learner, MBOX_State *mbox) {
  not_header = 1;
  learner->doc.count = 0;
  learner->doc.nullcount = 0; /* first header doesn't count */
  learner->doc.skip = 0;
}

void count_mbox_messages(learner_t *learner, Mstate mbox_state, char *textbuf) {

  if( !textbuf ) { 
    update_shannon_partials(learner);
    /* don't count document */
  } else {
    switch(mbox_state) {
    case msHEADER:
      if(not_header) {
	if( u_options & (1<<U_OPTION_DECIMATE) ) {
	  learner->doc.skip = ( rand() > (int)(RAND_MAX>>decimation) );
	} 
	if( !learner->doc.skip ) {
	  if( m_options & (1<<M_OPTION_CALCENTROPY) &&
	      (learner->doc.emp.top == 0) ) {
	    learner->doc.nullcount++;
	  }
	  update_shannon_partials(learner);
	  learner->doc.count++;
	}
      }
      not_header = 0;
      break;
    default:
      not_header = 1;
      break;
    }
  }

}


l_item_t *find_in_learner(learner_t *learner, hash_value_t id) {
    register l_item_t *i, *loop;
    /* start at id */
    i = loop = &learner->hash[id & (learner->max_tokens - 1)];

    while( FILLEDP(i) ) {
	if( EQUALP(i->id,id) ) {
	    return i; /* found id */
	} else {
	    i++; /* not found */
	    /* wrap around */
	    i = (i >= &learner->hash[learner->max_tokens]) ? learner->hash : i; 
	    if( i == loop ) {
		return NULL; /* when hash table is full */
	    }
	}
    }

    /* empty slot, so not found */

    return i; 
}


/* places the token in the global hash and writes the
   token to a temporary file for later, then updates
   the digram frequencies. Tokens have the format 
   DIAMOND t1 DIAMOND t2 ... tn DIAMOND CLASSEP class NUL */
void hash_word_and_learn(learner_t *learner, 
			 char *tok, token_type_t tt, regex_count_t re) {
  hash_value_t id;
  l_item_t *i;
  char *s;
  alphabet_size_t p,q;

  for(s = tok; s && *s == DIAMOND; s++);
  if( s && (*s != EOTOKEN) ) { 

    if( overflow_warning ) {
      return; /* for there be troubles ahead */
    }

    if( m_options & (1<<M_OPTION_MULTINOMIAL) ) { tt.order = 1; }

    if( u_options & (1<<U_OPTION_DECIMATE) ) {
      if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
	if( learner->doc.skip ) {
	  return;
	}
      } else {
	if( rand() > (int)(RAND_MAX>>decimation) ) {
	  return;
	}
      }
    }

    id = hash_full_token(tok);
    i = find_in_learner(learner, id);

    if( i && !FILLEDP(i) &&
	((100 * learner->unique_token_count) >= 
	 (HASH_FULL * learner->max_tokens)) && grow_learner_hash(learner) ) {
      i = find_in_learner(learner,id);
      /* new i, go through all tests again */
    }

    if( i ) {

      if( FILLEDP(i) ) {

	/* redundant :-) */

      } else if( /* !FILLEDP(i) && */
		((100 * learner->unique_token_count) < 
		 (HASH_FULL * learner->max_tokens) ) ) {

	/* fill the hash and write to file */

	SET(i->id,id);

	if( learner->unique_token_count < K_TOKEN_COUNT_MAX )
	  { learner->unique_token_count++; } else { overflow_warning = 1; }

	i->typ = tt;

	/* order accounting */
	learner->max_order = (learner->max_order < i->typ.order) ? 
	  i->typ.order : learner->max_order;

	if( learner->fixed_order_unique_token_count[i->typ.order] < K_TOKEN_COUNT_MAX ) 
	  { learner->fixed_order_unique_token_count[i->typ.order]++; } else 
	    { overflow_warning = 1; }

     	if( (u_options & (1<<U_OPTION_DEBUG)) ) {
     	  LOG(stdout, "match %8lx ", (long unsigned int)i->id);
	  print_token(stdout, tok);
	  LOG(stdout, " %d\n", i->typ.order);
     	}

	tmp_grow(learner); /* just in case we're full */
	tmp_write_token(learner, tok);
      }

      if( i->count < K_TOKEN_COUNT_MAX ) { 
	i->count++; 
	if( learner->t_max < i->count ) {
	  learner->t_max = i->count;
	}
	if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
	  if( (learner->doc.emp.top < learner->doc.emp.max) ||
	      emplist_grow(&learner->doc.emp) ) {
	    i->tmp.read.eff++; /* this can never overflow before i->count */
	    UNSETMARK(i); /* needed for calculating shannon */
	    learner->doc.emp.stack[learner->doc.emp.top++] = i->id;
	  }
	}
      } else { 
	overflow_warning = 1; 
      }

      if( learner->full_token_count < K_TOKEN_COUNT_MAX )
	{ learner->full_token_count++; } else 
	  { /* this number is just cosmetic */ }

      if( learner->fixed_order_token_count[i->typ.order] < K_TOKEN_COUNT_MAX ) 
	{ learner->fixed_order_token_count[i->typ.order]++; } else 
	  { skewed_constraints_warning = 1; }

    }

    if( digramic_overflow_warning ) {
      return;
    }


    /* now update digram frequency counts */
    if( tt.order == 1 ) { /* count each token only once */
      p = *tok++;
      /* finally update character frequencies */
      while( *tok != EOTOKEN ) {
	q = (unsigned char)*tok;
	if( learner->dig[p][q] < K_DIGRAM_COUNT_MAX ) 
	  { learner->dig[p][q]++; } else { digramic_overflow_warning = 1; }
	p = q;
	tok++;
      }

      if( digramic_overflow_warning ) {
	errormsg(E_WARNING,
		"ran out of integers (too much data), "
		"reference measure may be skewed.\n");
	m_options |= (1<<M_OPTION_WARNING_BAD);
      }
    }

    if( overflow_warning ) {
      errormsg(E_WARNING,
	      "ran out of integers (too much data), "
	      "results may be skewed.\n");
      m_options |= (1<<M_OPTION_WARNING_BAD);
    }
  }
}



/* initialize global learner object */
void init_learner(learner_t *learner) {
  alphabet_size_t i, j;
  regex_count_t c;
  token_order_t z;

  /* some constants */
   learner->max_tokens = default_max_tokens;
  learner->max_hash_bits = default_max_hash_bits;
  learner->full_token_count = 0;
  learner->unique_token_count = 0;
  learner->t_max = 0;
  learner->b_count = 0;
  learner->logZ = 0.0;
  learner->shannon = 0.0;
  learner->alpha = 0.0;
  learner->beta = 0.0;
  learner->mu = 0.0;
  learner->s2 = 0.0;
  learner->max_order = 0;

  learner->doc.A = 0.0;
  learner->doc.C = 0.0;
  learner->doc.count = 0;
  learner->doc.nullcount = 0;
  learner->doc.emp.top = 0;
  learner->doc.emp.max = 0;
  learner->doc.emp.stack = NULL;
  memset(learner->doc.reservoir, 0, RESERVOIR_SIZE * sizeof(emplist_t));

  learner->m_options = m_options;
  learner->u_options = u_options;

  learner->mmap_start = NULL;
  learner->mmap_learner_offset = 0;
  learner->mmap_hash_offset = 0;
  learner->hash = NULL;

  /* init character frequencies */
  for(i = 0; i < ASIZE; i++) { 
    for(j = 0; j < ASIZE; j++) { 
      learner->dig[i][j] = 0.0;
    }
  }

  for(c = 0; c < MAX_RE + 1; c++) {
    learner->regex_token_count[c] = 0;
  }

  for(z = 0; z < MAX_SUBMATCH; z++) {
    learner->fixed_order_token_count[z] = 0;
    learner->fixed_order_unique_token_count[z] = 0;
  }

  if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
    reset_mbox_messages(learner, &mbox);
  }

  if( !(*online && read_online_learner_struct(learner,online)) ) {
    /* if we're here, we must do what read_online_... didn't do */

#ifdef DEBUG
    LOG(stderr, "learner->max_tokens %d \n", learner->max_tokens *  sizeof(l_item_t));
#endif

    /* allocate and zero room for hash */
    learner->hash = (l_item_t *)calloc(learner->max_tokens, sizeof(l_item_t));
    if( !learner->hash ) {
      errormsg(E_FATAL,
	       "not enough memory? I couldn't allocate %li bytes\n",
	       (sizeof(l_item_t) * ((long int)learner->max_tokens)));
    }

#ifdef NVRAM

   learner->tmp.file = fdopen(temp_file,"w+");
#else
    /* open temporary file for writing tokens */
    learner->tmp.file = 
      mytmpfile(learner->filename ? learner->filename : progname, 
		&learner->tmp.filename);
#endif
    /* set this: in case of a fatal signal, we can unlink the temprile */
    cleanup.tempfile = learner->tmp.filename;

    learner->tmp.iobuf = NULL;   /* can't reuse in_iobuf or out_iobuf */
    learner->tmp.offset = 0;
    learner->tmp.avail = 0;
    learner->tmp.used = 0;
    learner->tmp.mmap_start = NULL;
    learner->tmp.mmap_offset = 0;
    learner->tmp.mmap_length = 0;
    learner->tmp.mmap_cursor = 0;

    if( learner->tmp.file ) {
#if defined HAVE_POSIX_MEMALIGN
      /* buffer must exist until after fclose() */
      if( 0 != posix_memalign(&learner->tmp.iobuf, system_pagesize, 
			      BUFFER_MAG * system_pagesize) ) {
	learner->tmp.iobuf = NULL;
      }
#elif defined HAVE_MEMALIGN
      /* buffer can't be myfree()'d */
      learner->tmpiobuf = (void *)memalign(system_pagesize, 
					   BUFFER_MAG * system_pagesize);
#elif defined HAVE_VALLOC
      /* buffer can't be myfree()'d */
      learner->tmpiobuf = (void *)valloc(BUFFER_MAG * system_pagesize);
#endif
      if( learner->tmp.iobuf ) {
	setvbuf(learner->tmp.file, (char *)learner->tmp.iobuf, (int)_IOFBF, 
		(size_t)(BUFFER_MAG * system_pagesize));
      }
    }

  }

  if( u_options & (1<<U_OPTION_MMAP) ) {
    MLOCK(learner->hash, sizeof(l_item_t) * learner->max_tokens);
  }

  MADVISE(learner->hash, sizeof(l_item_t) * learner->max_tokens, 
	  MADV_SEQUENTIAL);

  if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
    learner->doc.emp.max = system_pagesize/sizeof(hash_count_t);
    learner->doc.emp.stack = 
      (hash_value_t *)mymalloc(learner->doc.emp.max * sizeof(hash_value_t));
    if( !learner->doc.emp.stack ) {
      errormsg(E_WARNING,
	       "disabling emp.stack. Not enough memory? I couldn't allocate %li bytes\n",
	       sizeof(hash_count_t *) * ((long int)learner->doc.emp.max));
      m_options &= ~(1<<M_OPTION_CALCENTROPY);
    }
  }

}

void myfree_learner(learner_t *learner) {
  document_count_t i;

  myfree_learner_hash(learner);

  if( learner->doc.emp.stack ) { 
    myfree(learner->doc.emp.stack); 
    learner->doc.emp.stack = NULL;
  }

  for( i = 0; i < RESERVOIR_SIZE; i++ ) {
    if( learner->doc.reservoir[i].stack ) {
      myfree(learner->doc.reservoir[i].stack); 
      learner->doc.reservoir[i].stack = NULL;
    }
  }

  if( learner->tmp.file ) {
    fclose(learner->tmp.file);
    learner->tmp.file = NULL;
  }

  cleanup_tempfiles();
}

/* calculates the most probable Dirichlet parameters 
   given digram counts. Method described in MacKay and Peto (1995)
   Since we don't count the DIAMOND-DIAMOND digram, including the prior will 
   overestimate the DIAMOND-DIAMOND transition probability. However, for this 
   transition as well as the others, the Dirichlet will have practically
   no effect when there's enough data in the corpus.

   HOWEVER: 
   * A property of the MP posterior estimates is that if character j
   * never appears as a transition *target*, then the corresponding
   * u[j] entry is null. Sometimes, this makes sense, as j isn't part
   * of the "vocabulary", but other times it's not sensible, becasue sooner
   * or later some spam message will make use of that character in
   * some combination with another character. When that happens, we're
   * in trouble.
   *
   * Previous versions of dbacl.c had a simple hack to prevent this
   * possibility: we pretend we saw a DIAMOND-j-DIAMOND
   * double transition, for each j. Equivalently, we simply increment
   * the count by 1 for each such transition, i.e.
   *     learner->dig[i][(alphabet_size_t)DIAMOND]++;
   *     learner->dig[(alphabet_size_t)DIAMOND][i]++;
   *
   * This hack has now been removed again, to prevent confusion.
   * It's not really needed, since the uniform measure can be used
   * for spam filtering.
*/
void make_dirichlet_digrams(learner_t *learner) {
  alphabet_size_t i, j;
  token_count_t k;
  float G[ASIZE];
  float H[ASIZE];
  float V[ASIZE];
  float F[ASIZE];
  double prev_alpha;
  double K, t;

  /* initialize */
  dirichlet.alpha = 1.0;
  for(i = AMIN; i < ASIZE; i++) {
    dirichlet.u[i] = 1.0/((double)(ASIZE - AMIN));
  }

  /* precompute useful constants */
  for(j = AMIN; j < ASIZE; j++) {
    G[j] = 0.0;
    H[j] = 0.0;
    V[j] = 0.0;
    F[j] = 0.0;
    for(i = AMIN; i < ASIZE; i++) {
      for(k = AMIN; k < (token_count_t)learner->dig[i][j]; k++) {
	G[j] += 1.0/k;
	H[j] += 1.0/((double)k*k);
      }
      if( learner->dig[i][j] > 0 ) { 
	V[j]++; 
	F[j] += learner->dig[i][j];
      }
    }
    if( 0 && u_options & (1<<U_OPTION_DEBUG) ) { 
      LOG(stdout, 
	      "symbol %d has G = %g H = %g V = %g F = %g\n", 
	      j, G[j], H[j], V[j], F[j]);
    }
  }

  /* now optimize the dirichlet parameters */
#define MAX_DIRITER 300 
  k = 0; 
  do {
    prev_alpha = dirichlet.alpha;
    K = 0.0;
    for(i = AMIN; i < ASIZE; i++) {
      K += log((F[i] + prev_alpha)/prev_alpha) +
	(0.5) * F[i]/(prev_alpha * (F[i] + prev_alpha));
    }
    dirichlet.alpha = 0.0;
    for(j = AMIN; j < ASIZE; j++) {
      t = K - G[j];
      dirichlet.u[j] = 2 * V[j] /(t + sqrt(t * t + 4 * H[j] * V[j]));
      dirichlet.alpha += dirichlet.u[j];
    }
    if( u_options & (1<<U_OPTION_VERBOSE) ) {
      LOG(stdout, 
	      "dirichlet alpha %g --> %g\n", 
	      prev_alpha, dirichlet.alpha);
    }
  } while((k++ < MAX_DIRITER) && 
	  (fabs(prev_alpha - dirichlet.alpha) > qtol_alpha) );

  if( k >= MAX_DIRITER ) {
    //errormsg(E_WARNING, "dirichlet measure did not converge.\n");
  }
  
  if(u_options & (1<<U_OPTION_VERBOSE) ) {
    for(j = AMIN; j < ASIZE; j++) {
      LOG(stdout, 
	      "dirichlet u[%d] =  %g\n", 
	      j, dirichlet.u[j]);
    }
  }

  /* now make the probabilities */

  for(i = AMIN; i < ASIZE; i++) {
    t = 0.0;
    for(j = AMIN; j < ASIZE; j++) {
      t += learner->dig[i][j];
    }
    for(j = AMIN; j < ASIZE; j++) {
      /* note: simulate the effect of digitizing the digrams */
      learner->dig[i][j] = 
	UNPACK_DIGRAMS(PACK_DIGRAMS(log(((weight_t)learner->dig[i][j] + 
					 dirichlet.u[j]) / 
					(t + dirichlet.alpha)))); 
      if( 1 && u_options & (1<<U_OPTION_DEBUG) ) {
	LOG(stdout,
		"learner->dig[%d][%d] = %f\n", 
		i, j, learner->dig[i][j]);
      }
    }
  }
}

void make_uniform_digrams(learner_t *learner) {
  alphabet_size_t i, j;
  for(i = AMIN; i < ASIZE; i++) {
    for(j = AMIN; j < ASIZE; j++) {
      /* note: simulate the effect of digitizing the digrams */
      learner->dig[i][j] = UNPACK_DIGRAMS(PACK_DIGRAMS(-log(ASIZE - AMIN)));
      if(0 && u_options & (1<<U_OPTION_DEBUG) ) {
	LOG(stdout,
		"learner->dig[%d][%d] = %f\n", 
		i, j, learner->dig[i][j]);
      }
    }
  }
}

/* use 0.0 for small datasets, 1.0 for huge datasets. 
 * 0.6 works well generally. I don't understand this :-( 
 */
void transpose_digrams(learner_t *learner) {
  register alphabet_size_t i, j;
  register weight_t t;

  /* we skip transitions involving DIAMOND, TOKENSEP */
  /* code below uses fact that DIAMOND == 0x01 */
  /* code below uses fact that TOKENSEP == 0x02 */
  for(i = AMIN; i < ASIZE; i++) {
    for(j = AMIN; j < i; j++) {
      t = (learner->dig[i][j] + learner->dig[j][i])/2.0;
      learner->dig[j][i] =  0.6 * learner->dig[j][i] + 0.4 * t;
      learner->dig[i][j] =  0.6 * learner->dig[i][j] + 0.4 * t; 
    }
  }
}

void recompute_ed(learner_t *learner, weight_t *plogzon, weight_t *pdiv, 
		  int i, weight_t lam[ASIZE], weight_t Xi) {
  register alphabet_size_t j;
  weight_t logA = log((weight_t)(ASIZE - AMIN));
  weight_t logzon, div;
  weight_t maxlogz, maxlogz2, tmp;

  maxlogz = 0.0;
  maxlogz2 = log(0.0);
  for(j = AMIN; j < ASIZE; j++) {
    if( lam[j] > 0.0 ) {
      tmp = lam[j] + log(1.0 - exp(-lam[j])) - logA;
      if( maxlogz < tmp ) {
	maxlogz2 = maxlogz;
	maxlogz = tmp;
      }
    }
  }


  logzon = exp(0.0 - maxlogz);
  div = 0.0;
  for(j = AMIN; j < ASIZE; j++) {
    if( lam[j] > 0.0 ) {
      tmp = -logA - maxlogz;
      logzon += (exp( lam[j] + tmp) - exp(tmp));
      div += lam[j] * learner->dig[i][j];
    }
  }


  tmp = maxlogz + log(logzon);

  if( isnan(tmp) ) {
    errormsg(E_FATAL,"sorry, digramic partition function went kaboom.\n");
  }

  logzon = tmp;
  div = div/Xi - logzon;

  *plogzon = logzon;
  *pdiv = div;
}

void make_entropic_digrams(learner_t *learner) {
  weight_t lam[ASIZE];
  weight_t lam_delta, old_lam, logt, maxlogt;
  weight_t Xi, logXi, logzon, div, old_logzon, old_div;
  weight_t logA = log((weight_t)(ASIZE - AMIN));
  register alphabet_size_t i, j;
  int itcount;

  for(i = AMIN; i < ASIZE; i++) {

    /* initializations */
    Xi = 0.0;
    for(j = AMIN; j < ASIZE; j++) {
      Xi += learner->dig[i][j];
      lam[j] = 0.0;
    }
    logXi = log(Xi);

    recompute_ed(learner, &logzon, &div, i, lam, Xi);

    /* now iterate - this is like minimize_learner_divergence() */
    itcount = 0;
    do {
      itcount++;

      old_logzon = logzon;
      old_div = div;

      lam_delta = 0.0;
      for(j = AMIN; j < ASIZE; j++) {
	if( learner->dig[i][j] > 0.0 ) {

	  old_lam = lam[j];
	  lam[j] = log(learner->dig[i][j]) - logXi + logA + logzon;

	  if( isnan(lam[j]) ) {

	    lam[j] = old_lam;
	    recompute_ed(learner, &logzon, &div, i, lam, Xi);

	  } else {

	    if( lam[j] > (old_lam + MAX_LAMBDA_JUMP) ) {
	      lam[j] = (old_lam + MAX_LAMBDA_JUMP);
	    } else if( lam[j] < (old_lam - MAX_LAMBDA_JUMP) ) {
	      lam[j] = (old_lam - MAX_LAMBDA_JUMP);
	    }

	    /* don't want negative weights */
	    if( lam[j] < 0.0 ) { lam[j] = 0.0; }

	    if( lam_delta < fabs(lam[j] - old_lam) ) {
	      lam_delta = fabs(lam[j] - old_lam);
	    }

	  }
	}
      }


      recompute_ed(learner, &logzon, &div, i, lam, Xi);

      if( u_options & (1<<U_OPTION_VERBOSE) ) {
	LOG(stdout, "digramic (%d) entropy change %" FMT_printf_score_t \
		" --> %" FMT_printf_score_t " (%10f, %10f)\n", i,
		old_div, div, lam_delta, fabs(logzon - old_logzon));
      }

    
    } while( ((fabs(div - old_div) > 0.001) || (lam_delta > 0.001) ||
	      (fabs(logzon - old_logzon) > 0.001)) && (itcount < 500) );


    /* now make the probabilities */

    /* transitions should already sum to 1, but better safe than sorry */
    maxlogt = log(0.0);
    for(j = AMIN; j < ASIZE; j++) {
      if( maxlogt < lam[j] ) {
	maxlogt = lam[j];
      }
    }
    maxlogt += -logA -logzon;
    logt = 0.0;
    for(j = AMIN; j < ASIZE; j++) {
      logt += exp(lam[j] - logA - logzon - maxlogt);
    }
    logt = maxlogt + log(logt);

    for(j = AMIN; j < ASIZE; j++) {
      /* note: simulate the effect of digitizing the digrams */
      learner->dig[i][j] = 
	UNPACK_DIGRAMS(PACK_DIGRAMS(lam[j] - logA - logzon - logt));

      if(0 && u_options & (1<<U_OPTION_DEBUG) ) {
	LOG(stdout,
		"learner->dig[%d][%d] = %f\n", 
		i, j, learner->dig[i][j]);
      }
    }
  }
}


void interpolate_digrams(weight_t N, weight_t count[], weight_t p[]) {
  weight_t lambda = 0.5;
  weight_t Clam, Dlam, Zlam;
  alphabet_size_t i;
  int q = 0;

  for(q = 0; q < 100; q++) {
    Zlam = Dlam = Clam = 0.0;
    for(i = AMIN; i < ASIZE; i++) {
      Clam += log(p[i]) * (count[i]/N);
      Zlam += pow(p[i], lambda);
      Dlam += log(p[i]) * pow(p[i], lambda);
    }
    lambda += 0.1 * (Clam - Dlam/Zlam);
/*     if( lambda > 1.0 ) { lambda = 1.0; } */
/*     else if( lambda < 0.0 ) { lambda = 0.0; } */
/*     printf("Clam = %g Dlam = %g, Zlam = %g, lamdba = %g\n", Clam, Dlam, Zlam, lambda); */
  } 

}


weight_t calc_learner_digramic_excursion(learner_t *learner, char *tok) {
  register alphabet_size_t p, q;
  register weight_t t = 0.0;

  /* now update digram frequency counts */
  p = (unsigned char)*tok++;
  /* finally update character frequencies */
  while( *tok != EOTOKEN ) {
    q = (unsigned char)*tok;
    t += learner->dig[p][q];
    p = q;
    tok++;
  }
  return t;
}

/* calculates the rth-order divergence but needs normalizing constant
   note: this isn't the full divergence from digref, just the bits
   needed for the r-th optimization - fwd indicates the traversal
   direction in the hash (alternate between them to reduce numerical
   errors */
score_t learner_divergence(learner_t *learner, 
			   score_t logzonr, score_t Xi,
			   token_order_t r, bool_t fwd) {
  register l_item_t *i, *e;
  register token_count_t c = 0;
  score_t t = 0.0;

  e = fwd ? (learner->hash + learner->max_tokens) : learner->hash - 1;
  for(i = fwd ? learner->hash : learner->hash + learner->max_tokens - 1; 
      i != e; fwd ? i++ : i--) {
    if( FILLEDP(i) && (i->typ.order == r) ) {

      t += UNPACK_LAMBDA(i->lam) * (score_t)i->count;

      if( ++c >= learner->fixed_order_unique_token_count[r] ) {
	c = 0;
	break;
      }
    }
  }

  return -logzonr + t/Xi;
}

/* calculates the normalizing constant - fwd indicates the traversal
   direction in the hash (alternate between them to reduce numerical
   errors */
score_t learner_logZ(learner_t *learner, token_order_t r, 
		     score_t log_unchanging_part, bool_t fwd) {
  register l_item_t *i, *e;
  register token_count_t c = 0;

  score_t maxlogz, maxlogz2, tmp;
  score_t t =0.0;
  score_t R = (score_t)r;

/*   printf("learner_logZ(%d, %f)\n", r, log_unchanging_part); */

  e = fwd ? (learner->hash + learner->max_tokens) : learner->hash - 1;
  maxlogz = log_unchanging_part;
  maxlogz2 = log(0.0);
  for(i = fwd ? learner->hash : learner->hash + learner->max_tokens - 1; 
      i != e; fwd ? i++ : i--) {
    if( FILLEDP(i) && (i->typ.order == r) ) {

      tmp = R * UNPACK_LAMBDA(i->lam) + R * UNPACK_LWEIGHTS(i->tmp.min.ltrms) + 
	UNPACK_RWEIGHTS(i->tmp.min.dref);

      if( maxlogz < tmp ) {
	maxlogz2 = maxlogz;
	maxlogz = tmp;
      }

      if( ++c >= learner->fixed_order_unique_token_count[r] ) {
	c = 0;
	break;
      }
    }
  }

  t =  exp(log_unchanging_part - maxlogz);
  for(i = fwd ? learner->hash : learner->hash + learner->max_tokens - 1; 
      i != e; fwd ? i++ : i--) {
    if( FILLEDP(i) && (i->typ.order == r) ) {

      tmp = R * UNPACK_LWEIGHTS(i->tmp.min.ltrms) + 
	UNPACK_RWEIGHTS(i->tmp.min.dref) - maxlogz;
      t += (exp(R * UNPACK_LAMBDA(i->lam) + tmp) - exp(tmp)); 

      if( ++c >= learner->fixed_order_unique_token_count[r] ) {
	c = 0;
	break;
      }
    }
  }

  tmp =  (maxlogz + log(t))/R;
/*   printf("t =%f maxlogz = %f logZ/R = %f\n", t, maxlogz, tmp); */

  /* investigate this someday */
  if( isnan(tmp) ) {
    errormsg(E_FATAL,"sorry, partition function went kaboom.\n");
  }

  return tmp;
}

/* reconstructs the order of a token (can be zero for empty token) */
token_order_t get_token_order(char *tok) {
  token_order_t o = 0;
  if( tok && *tok ) { 
    while( *(++tok) ) {
      if( *tok == DIAMOND ) {
	o++;
      }
    }
  }
  return o;
}


/* fills the hash with partial calculations */
void fill_ref_vars(learner_t *learner, l_item_t *k, char *tok) {
  hash_value_t id;
  char *t, *e;
  l_item_t *l;

  /* weight of the r-th order excursion */
  k->tmp.min.dref = PACK_RWEIGHTS(calc_learner_digramic_excursion(learner,tok));

  /* for each suffix of tok, add its weight */
  k->tmp.min.ltrms = PACK_LWEIGHTS(0.0);

  if( tok && *tok ) {
    e = strchr(tok + 1, EOTOKEN);
    for( t = tok + 1; t + 1 < e; t++ ) {
      if( *t == DIAMOND ) {
	id = hash_partial_token(t, e - t, e);
	l = find_in_learner(learner, id); 
	if( l ) {
	  k->tmp.min.ltrms += PACK_LWEIGHTS(UNPACK_LAMBDA(l->lam));
	}
      }
    }

    /* extra smoothing doesn't seem that good */
#undef EXTRA_SMOOTHING
#if defined EXTRA_SMOOTHING
    /* rather than adding only suffixes, this adds all 
     * the prefixes also, then divides by two. */
    if( e ) {
      /* for each prefix of tok, add its weight */
      for(t = e - 2; t > tok; t--) {
	if( *t == DIAMOND ) {
	  id = hash_partial_token(tok, t - tok + 1, e);
	  l = find_in_learner(id);
	  if( l ) {
	    k->ltrms += PACK_LWEIGHTS(UNPACK_LAMBDA(l->lam));
	  }
	}
      }
      /* now we have twice as many weights as we need */
      k->ltrms /= 2;
    }  
#endif
  }

}

/* fills hash with partial calculations and returns the
 * log unchanging part of the normalizing constant, for all
 * orders < r (not r). On return, kappa contains the reference probability
 * mass of the set of all features <= r (including r).
 */
score_t recalculate_reference_measure(learner_t *learner, token_order_t r, score_t *kappa) {
  hash_value_t id;
  byte_t buf[BUFSIZ+1];
  //char tok[(MAX_TOKEN_LEN+1)*MAX_SUBMATCH+EXTRA_TOKEN_LEN];
  char tok[8192];
  size_t n = 0;
  int counter =0, max_val=0;
  const byte_t *p;
  char *q;

  l_item_t *k, *i;

  score_t tmp, lunch;
  score_t R = (score_t)r;
  score_t max = 0.0;
  score_t mykappa = 0.0;

  max_val = (MAX_TOKEN_LEN+1)*MAX_SUBMATCH+EXTRA_TOKEN_LEN;

#ifdef DEBUG
  LOG(stderr,"recalculate_reference_measure: called \n");
#endif


  /* for r == 1, the partial_z is not modified, but
     we still have to go through the tokens and prebuild k-dref */

  /* now we calculate the logarithmic word weight
     from the digram model, for each token in the hash */
  if( !tmp_seek_start(learner) ) {
    errormsg(E_ERROR, "cannot seek in temporary token file, reference weights not calculated.\n");
  } else {

    q = tok;
    while( (n = tmp_read_block(learner, buf, BUFSIZ, &p)) > 0 ) {
/*       p = buf; */
/*       p[n] = '\0'; */
      while( n-- > 0 && counter < max_val  ) {
	if( *p != TOKENSEP) {
          counter++; 
	  *q++ = *p; /* copy into tok */
	} else { /* interword space */ 
	  *q = 0; /* append NUL to tok */
	  /* now write weight in hash */
	  id = hash_full_token(tok);
	  k = find_in_learner(learner, id); 
	  if( k && (get_token_order(tok) == k->typ.order) ) {
	    if( k->typ.order <= r) {
	      if( k->typ.order == r ) {
		fill_ref_vars(learner, k, tok);
	      } else if(k->typ.order < r) {
		/* assume ref_vars were already filled */
		if( NOTNULL(k->lam) ) {
		  tmp = R * UNPACK_LAMBDA(k->lam) + 
		    R * UNPACK_LWEIGHTS(k->tmp.min.ltrms) +
		    UNPACK_RWEIGHTS(k->tmp.min.dref);
		  if( max < tmp ) {
		    max = tmp;
		  }
		}
	      }
	      mykappa += exp(UNPACK_RWEIGHTS(k->tmp.min.dref));
	    }
	  }
	  q = tok; /* reset q */
	}
	p++;
      }
    }
  }

  lunch = 1.0;
  k = learner->hash + learner->max_tokens;
  for(i = learner->hash; i != k; i++) {
    if( FILLEDP(i) ) {
      if( i->typ.order < r ) {
	if( NOTNULL(i->lam) ) {
	  tmp = -max + R * UNPACK_LWEIGHTS(i->tmp.min.ltrms) + 
	    UNPACK_RWEIGHTS(i->tmp.min.dref);
	  lunch += (exp(R * UNPACK_LAMBDA(i->lam) + tmp) - exp(tmp));
	}
      }
    }
  }
  lunch = max + log(lunch);

  /* kappa is the reference mass of all features <= rth order. */
  *kappa = mykappa;

#ifdef DEBUG
  LOG(stderr,"recalculate_reference_measure exit()\n");
#endif

  return lunch;
}


/* just for debugging */
void print_score(learner_t *learner, token_order_t r) {
  hash_count_t i;
  double logprob = 0.0;
  double lpapprox = 0.0;
  double norm;

  for(i = 0; i < learner->max_tokens; i++) {
    if( FILLEDP(&learner->hash[i]) &&
	(learner->hash[i].typ.order <= r) ) {
      logprob += learner->hash[i].count * (learner->hash[i].lam);
      if( learner->hash[i].typ.order == 1 ) {
	logprob += learner->hash[i].count * 
	  UNPACK_RWEIGHTS(learner->hash[i].tmp.min.dref)/((weight_t)r);
      }
    }
    if( FILLEDP(&learner->hash[i]) &&
	(learner->hash[i].typ.order == r) ) {
      lpapprox += learner->hash[i].count * 
	((learner->hash[i].lam) + 
	 UNPACK_LWEIGHTS(learner->hash[i].tmp.min.ltrms) + 
	 UNPACK_RWEIGHTS(learner->hash[i].tmp.min.dref)/((weight_t)r));
    }

  }
  norm = learner->fixed_order_token_count[r] * learner->logZ;

  LOG(stdout,
	  "*** logprob = %" FMT_printf_score_t " * %d (r = %d, logZ = %" FMT_printf_score_t ")\n", 
	 ((score_t)(logprob / learner->fixed_order_token_count[r])), 
	 learner->fixed_order_token_count[r], 
	 r, learner->logZ);
  LOG(stdout,
	  "*** lpapprox = %" FMT_printf_score_t " * %d (r = %d, logZ = %" FMT_printf_score_t ")\n", 
	 ((score_t)(lpapprox / learner->fixed_order_token_count[r])), 
	 learner->fixed_order_token_count[r], 
	 r, learner->logZ);
}


/* experimental: this sounded like a good idea, but isn't ?? */
void theta_rescale(learner_t *learner, token_order_t r, score_t logupz, score_t Xi, bool_t fwd) {
  register l_item_t *i, *e;
  register token_count_t c = 0;

  score_t tmp;
  score_t theta =0.0;
  score_t R = (score_t)r;
  score_t mu;
  score_t sum1, sum2;
  score_t msum1, msum2;
  score_t s = 0;

  e = fwd ? (learner->hash + learner->max_tokens) : learner->hash - 1;

  msum1 = msum2 = log(0.0);
  for(i = fwd ? learner->hash : learner->hash + learner->max_tokens - 1; 
      i != e; fwd ? i++ : i--) {
    if( FILLEDP(i) && (i->typ.order == r) ) {
      if( NOTNULL(i->lam) ) {
	s += i->count;
	tmp = R * UNPACK_LWEIGHTS(i->tmp.min.ltrms) + 
	  UNPACK_RWEIGHTS(i->tmp.min.dref);
	if( tmp > msum1 ) {
	  msum1 = tmp;
	}
	tmp += R * UNPACK_LAMBDA(i->lam);
	if( tmp > msum2 ) {
	  msum2 = tmp;
	}
      }
      if( ++c >= learner->fixed_order_unique_token_count[r] ) {
	c = 0;
	break;
      }
    }
  }  

  mu = ((score_t)s)/Xi;
  s = log(mu) - log(1 - mu) + logupz; 
  if( s > msum1 ) {
    msum1 = s;
  }

  sum1 = exp(s - msum1);
  sum2 = 0.0;
  for(i = fwd ? learner->hash : learner->hash + learner->max_tokens - 1; 
      i != e; fwd ? i++ : i--) {
    if( FILLEDP(i) && (i->typ.order == r) ) {
      if( NOTNULL(i->lam) ) {
	tmp = R * UNPACK_LWEIGHTS(i->tmp.min.ltrms) + 
	  UNPACK_RWEIGHTS(i->tmp.min.dref);
	sum1 += exp(tmp - msum1);
	tmp += R * UNPACK_LAMBDA(i->lam);
	sum2 += exp(tmp - msum2);
      }
      if( ++c >= learner->fixed_order_unique_token_count[r] ) {
	c = 0;
	break;
      }
    }
  }

  theta = ((msum1 + log(sum1)) - (msum2 + log(sum2)))/R;
  if( isnan(theta) ) {
    return; /* ignore */
  }
  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    LOG(stdout, "theta = %f\n", theta);
  }

  for(i = fwd ? learner->hash : learner->hash + learner->max_tokens - 1; 
      i != e; fwd ? i++ : i--) {
    if( FILLEDP(i) && (i->typ.order == r) ) {
      if( NOTNULL(i->lam) ) {
	i->lam = PACK_LAMBDA(UNPACK_LAMBDA(i->lam) + theta);
      }
      if( ++c >= learner->fixed_order_unique_token_count[r] ) {
	c = 0;
	break;
      }
    }
  }

}

/* minimizes the divergence by solving for lambda one 
   component at a time.  */
void minimize_learner_divergence(learner_t *learner) {
  l_item_t *i, *e;
  token_order_t r;
  token_count_t lzero, c = 0;
  int itcount, mcount;
  score_t d, dd, b;
  score_t lam_delta, old_lam, new_lam;
  score_t logzonr, old_logzonr;
  score_t logupz, div_extra_bits, kappa, thresh;
  score_t R, Xi, logXi;
  score_t mp_logz;
  bool_t fwd = 1;

  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    LOG(stdout, "now maximizing model entropy\n");
  }

#ifdef DEBUG
  LOG(stderr,"Entering  minimize_learner_divergence \n");
#endif

  learner->logZ = 0.0;
  learner->divergence = 0.0;
  mp_logz = 0.0;

  /* disable multipass if we only have one order to play with */
  if( (m_options & (1<<M_OPTION_MULTINOMIAL)) || 
      (learner->max_order == 1) ) {
    qtol_multipass = 0;
  }

  e = learner->hash + learner->max_tokens;
  for(mcount = 0; mcount < (qtol_multipass ? 50 : 1); mcount++) {
    for(r = 1; 
	r <= ((m_options & (1<<M_OPTION_MULTINOMIAL)) ? 1 : learner->max_order); 
	r++) {

      /* here we precalculate various bits and pieces
	 which aren't going to change during this iteration */
      logupz = recalculate_reference_measure(learner, r, &kappa);

      R = (score_t)r;
      Xi = (score_t)learner->fixed_order_token_count[r];
      logXi = log(Xi);

      if( r == 1 ) {
/*       Xi /= kappa; */
	div_extra_bits = 0.0;
      } else {
      /* calculate extra bits for divergence score display */
	b = 0.0;

	for(i = learner->hash; i != e; i++) {
	  if( FILLEDP(i) && (i->typ.order < r) ) {
	    b += UNPACK_LAMBDA(i->lam) * (score_t)i->count;
	  }
	}
	div_extra_bits = b/Xi;
      }

      if( m_options & (1<<M_OPTION_REFMODEL) ) {
	break;
      }

      if( u_options & (1<<U_OPTION_VERBOSE) ) {
	LOG(stdout, 
		"* optimizing order %d weights (%i tokens, %i unique, kappa = %f)\n", 
		r, learner->fixed_order_token_count[r],
		learner->fixed_order_unique_token_count[r], kappa);
      }

      logzonr = learner_logZ(learner, r, logupz, fwd);
      dd = learner_divergence(learner, logzonr, Xi, r, fwd);
      fwd = 1 - fwd;
/*       printf("logzonr = %f dd = %f logupz = %f Xi = %f\n", logzonr, dd, logupz, Xi); */


      itcount = 0;
      thresh = 0.0;
      do {
	itcount++;
	lzero= 0;

	d = dd;     /* save old divergence */
	old_logzonr = logzonr;

	lam_delta = 0.0;


	c = 0;
	for(i = learner->hash; i != e; i++) {
	  if( FILLEDP(i) && (i->typ.order == r) 
/* 	    && (rand() > RAND_MAX/2)  */
	      ) { 

	    old_lam = UNPACK_LAMBDA(i->lam);

	    if( (i->typ.order == 1) || (i->count > ftreshold) ) {
	      /* "iterative scaling" lower bound */
	      new_lam = (log((score_t)i->count) - logXi -  
			 UNPACK_RWEIGHTS(i->tmp.min.dref))/R + logzonr -
		UNPACK_LWEIGHTS(i->tmp.min.ltrms);
	    } else {
	      new_lam = 0.0;
	    }

	    if( isnan(new_lam) ) {
	      /* precision problem, just ignore, don't change lambda */
	      logzonr = learner_logZ(learner, r, logupz, fwd);
	      thresh = 0.0;
	    } else {
	      
	      /* this code shouldn't be necessary, but is crucial */
	      if( new_lam > (old_lam + MAX_LAMBDA_JUMP) ) {
		new_lam = (old_lam + MAX_LAMBDA_JUMP);
	      } else if( new_lam < (old_lam - MAX_LAMBDA_JUMP) ) {
		new_lam = (old_lam - MAX_LAMBDA_JUMP);
	      }

	      /* don't want negative weights */
	      if( new_lam < 0.0 ) { new_lam = 0.0; lzero++; }

	      if( lam_delta < fabs(new_lam - old_lam) ) {
		lam_delta = fabs(new_lam - old_lam);
	      }
	      i->lam = PACK_LAMBDA(new_lam);
	    }

	    if( ++c >= learner->fixed_order_unique_token_count[r] ) {
	      c = 0;
	      break;
	    }
	  }

	}

/* 	theta_rescale(r, logupz, Xi, fwd); */

	/* update values */
	logzonr = learner_logZ(learner, r, logupz, fwd);
	dd = learner_divergence(learner, logzonr, Xi, r, fwd);
	fwd = 1 - fwd;

	if( u_options & (1<<U_OPTION_VERBOSE) ) {
/* 	LOG(stdout, "lzero = %ld\n", lzero); */
	  LOG(stdout, "entropy change %" FMT_printf_score_t \
		  " --> %" FMT_printf_score_t " (%10f, %10f)\n", 
		  d + div_extra_bits, 
		  dd + div_extra_bits,
		  lam_delta, fabs(logzonr - old_logzonr));
	}

	process_pending_signal(NULL);
      
      } while( ((fabs(d - dd) > qtol_div) || (lam_delta > qtol_lam) ||
		(fabs(logzonr - old_logzonr) > qtol_logz)) && (itcount < 50) );

      learner->logZ = logzonr;
      learner->divergence = dd + div_extra_bits;
    }
    /* for multipass, we wait until logZ stabilizes */
    if( fabs(1.0 - mp_logz/learner->logZ) < 0.01 ) {
      break;
    }
    mp_logz = learner->logZ;
  }
#ifdef DEBUG
  LOG(stderr,"Exiting minimize_learner_divergence \n");
#endif

}


/* dumps readable model weights to the output */
void dump_model(learner_t *learner, FILE *out, FILE *in) {

  hash_value_t id;
  byte_t buf[BUFSIZ+1];
  char tok[(MAX_TOKEN_LEN+1)*MAX_SUBMATCH+EXTRA_TOKEN_LEN];
  char smb[MAX_SUBMATCH+1];
  token_order_t s;
  regex_count_t c;
  const byte_t *p;
  char *q;
  l_item_t *k;
  size_t n = 0;

  /* preamble - this is copied from save_learner */

  fprintf(out, MAGIC1, learner->filename, 
	  (m_options & (1<<M_OPTION_REFMODEL)) ? "(ref)" : "");
  fprintf(out, MAGIC2_o, learner->divergence, learner->logZ, learner->max_order,
	  (m_options & (1<<M_OPTION_MULTINOMIAL)) ? "multinomial" : "hierarchical" );
  fprintf(out, MAGIC3, 
	  (short int)learner->max_hash_bits, 
	  (long int)learner->full_token_count, 
	  (long int)learner->unique_token_count,
	  (long int)learner->doc.count);

  fprintf(out, MAGIC8_o, learner->shannon, 
	  learner->alpha, learner->beta,
	  learner->mu, learner->s2);

  fprintf(out, MAGIC9, (long int)learner->t_max, (long int)learner->b_count);

  /* print out any regexes we might need */
  for(c = 0; c < regex_count; c++) {
    /* write the bitmap */
    for(q = smb, s = 1; s <= MAX_SUBMATCH; s++) {
      if( re[c].submatches & (1<<s) ) {
	*q++ = s + '0';
      }
    }
    *q = '\0';

    fprintf(out, MAGIC5_o, re[c].string, smb);
  }

  /* print options */
  fprintf(out, MAGIC4_o, m_options, print_model_options(m_options, (char*)buf));

  fprintf(out, MAGIC6); 

  fprintf(out, MAGIC_DUMP);


  /* now go through hash printing values */
  if( !tmp_seek_start(learner) ) {
    errormsg(E_ERROR, "cannot seek in temporary token file.\n");
  } else {
    q = tok;
    while( (n = tmp_read_block(learner, buf, BUFSIZ, &p)) > 0 ) {
/*       p = buf; */
/*       p[n] = '\0'; */
      while( n-- > 0 ) {
	if( *p != TOKENSEP) {
	  *q++ = *p; /* copy into tok */
	} else { /* interword space */ 
	  *q = 0; /* append NUL to tok */
	  /* now write weight in hash */
	  id = hash_full_token(tok);
	  k = find_in_learner(learner, id); /* guaranteed to be found */
	  fprintf(out, MAGIC_DUMPTBL_o, 
		  (weight_t)UNPACK_LAMBDA(k->lam), 
		  UNPACK_RWEIGHTS(k->tmp.min.dref), k->count, 
		  (long unsigned int)k->id);
	  print_token(out, tok);
	  fprintf(out, "\n");
	  q = tok; /* reset q */
	}
	p++;
      }
    }
  }
}

/* This is a quick hack to 
   1) speed up the optimization and 
   2) stabilise the estimated category model over a sequence of learning runs.

   Normally, if the dataset is nearly the same, then the optimal lambda weights 
   should be fairly similar (This is not strictly true if the digramic
   measure varies too much).  

   By reading the old category and presetting the lambdas from it, we typically
   get close enough to the true weights that one or two optimization iterations
   suffice. Moreover in that case, most weights should stay close to their
   "previous" values in the category, so the user doesn't think it's weird.

   A side effect is that there is entropy creep when relearing the same
   dataset over and over. Unless there's a bug I haven't noticed, this occurs
   because the entropy maximum we compute is affected by numerical precision
   and feature ordering within the hash. Each time we relearn the same dataset,
   we start off with a perturbation from the previous best weights, and obtain 
   a small improvement.
   
   WARNING: THIS USES cat[0], SO IS NOT COMPATIBLE WITH CLASSIFYING.
 */
void learner_prefill_lambdas(learner_t *learner, category_t **pxcat) {
  hash_count_t c;
  c_item_t *i, *e;
  l_item_t *k;
  category_t *xcat = &(cat[0]);

  *pxcat = NULL;
  xcat->fullfilename = strdup(learner->filename);
  if( open_category(cat) ) {
    if( xcat->m_options & (1<<M_OPTION_WARNING_BAD) ) {
      if( u_options & (1<<U_OPTION_VERBOSE) ) {
	errormsg(E_WARNING, "old category file %s may have bad data\n",
		xcat->fullfilename);
      }
    } else if( (xcat->m_options == m_options) &&
	       (xcat->retype == learner->retype) &&
	       (fabs((xcat->model_unique_token_count/
		      (double)learner->unique_token_count) - 1.0) < 0.15) ) {
      c = xcat->model_unique_token_count;
      if( u_options & (1<<U_OPTION_VERBOSE) ) {
	fprintf(stdout, "preloading %ld token weights\n", (long int)c);
      }
    
      MADVISE(xcat->hash, sizeof(c_item_t) * xcat->max_tokens, MADV_SEQUENTIAL);

      e = xcat->hash + xcat->max_tokens;
      for(i = xcat->hash; i != e; i++) {
	if( FILLEDP(i) ) {
	  k = find_in_learner(learner, NTOH_ID(i->id));
	  if( k ) {
	    k->lam = NTOH_LAMBDA(i->lam);
	  }
	  if( --c <= 0 ) {
	    break;
	  }
	}
      }

      /* tweak default quality settings */
      if( !quality ) {
	qtol_div = 0.01;
	qtol_lam = CLIP_LAMBDA_TOL(0.01);
	qtol_logz = 0.05;
      }

    }
    /* we're done, but don't close the category yet, as we might want to
       overwrite it */
    *pxcat = xcat;
  } else {
    if( u_options & (1<<U_OPTION_VERBOSE) ) {
      errormsg(E_WARNING, "cannot open file for reading %s\n", 
	      xcat->fullfilename);
    }
    myfree(xcat->fullfilename);
  }
}


void optimize_and_save(learner_t *learner) {

  hash_count_t i;
  token_order_t c;
  category_t *opencat = NULL;

#ifdef NVRAM
  create_online_file();
  initialize_tmp_file(); 
#endif

#ifdef STATS
    //number of tokens generated for input
    //all input test file sizes are same, so lets not
    //increment it for now
	//hash_tokens = learner->unique_token_count;
#endif

  if(100 * learner->unique_token_count >= HASH_FULL * learner->max_tokens) { 
    errormsg(E_WARNING,
	    "table full, some tokens ignored - "
	    "try with option -h %i\n",
	    learner->max_hash_bits + 1);
  } else if( learner->unique_token_count <= 0 ) {
    errormsg(E_WARNING,
	    "no tokens matched - have I learned nothing?\n");
    tmp_close(learner);
    exit(0); /* exit code for success */
  }

  if( skewed_constraints_warning ) {
    errormsg(E_WARNING,
	    "ran out of integers (too much data) constraints will be skewed.\n");
    m_options |= (1<<M_OPTION_WARNING_BAD);
  }

  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    fprintf(stdout, 
	    "picked up %i (%i distinct) tokens\n", 
	    learner->full_token_count, learner->unique_token_count);
    fprintf(stdout, 
	    "calculating reference word weights\n");
  }

  if( *online ) {

#ifdef DEBUG
    LOG(stderr, "WRITING ONLINE STRUCTURE \n");
#endif
    write_online_learner_struct(learner, online);
#ifdef DEBUG
    LOG(stderr, "After write_online_learner_struct \n");
#endif
  }

  /* transposition smoothing */
/*   transpose_digrams(); */

  if( u_options & (1<<U_OPTION_LAPLACE) ) {
    make_uniform_digrams(learner);
  } else if( u_options & (1<<U_OPTION_DIRICHLET) ) {
    make_dirichlet_digrams(learner);
  } else if( u_options & (1<<U_OPTION_JAYNES) ) {
    make_entropic_digrams(learner);
  } else {
    make_uniform_digrams(learner);
  }

  if( learner->fixed_order_token_count[1] == 0 ) {
    /* it's a higher order model but there are no first
       order tokens! We can't handle that! Go through the 
       hash converting everything to first order.
       This will result in incorrect calculations, unless
       the higher order tokens don't overlap. Suitable only for
       geniuses and fools. */
    errormsg(E_WARNING,
	    "\n"
	    "         You have not defined any unigrams, so in this model\n"
	    "         features will be treated independently, which is quite\n"
	    "         likely incorrect. I hope you know what you're doing,\n"
	    "         because I don't!\n\n");

    m_options |= (1<<M_OPTION_MULTINOMIAL);
    for(c = 2; c <= learner->max_order; c++) {
      learner->fixed_order_token_count[1] += learner->fixed_order_token_count[c];
      learner->fixed_order_unique_token_count[1] += 
	learner->fixed_order_unique_token_count[c];
    }
    for(i = 0; i < learner->max_tokens; i++) {
      if( FILLEDP(&learner->hash[i]) ) {
	learner->hash[i].typ.order = 1;
      }
    }

  }

#ifdef DEBUG
  LOG(stderr,"before minimize_learner_divergence \n");
#endif

  /* if the category already exists, we read its lambda values.
     this should speed up the minimization slightly */
  if( u_options & (1<<U_OPTION_NOZEROLEARN) ) {
    learner_prefill_lambdas(learner, &opencat);
  }

  minimize_learner_divergence(learner);

#ifdef DEBUG
   LOG(stderr,"before calc_shannon \n");
#endif

  calc_shannon(learner);

  if( u_options & (1<<U_OPTION_DUMP) ) {
    dump_model(learner, stdout, learner->tmp.file);
  }

  tmp_close(learner);

#ifdef DEBUG
  LOG(stderr,"before nvram_save_learner \n");
#endif

  /* we don't myfree learner->tmpiobuf, as we'll be exiting soon anyway */
#if defined HAVE_POSIX_MEMALIGN
/*   if( learner->tmpiobuf ) { myfree(learner->tmpiobuf); } */
#endif

  /* now save the model to a file */
  if( !opencat || !fast_partial_save_learner(learner, opencat) ) {

#ifndef NVRAM
 #ifdef DEBUG
    LOG(stderr, "Call save learner\n");
 #endif 
    save_learner(learner);    
#else
    nvram_save_learner(learner);
#endif
  }
  if( opencat ) { free_category(opencat); }
}

/***********************************************************
 * MULTIBYTE FILE HANDLING FUNCTIONS                       *
 * this is suitable for any locale whose character set     *
 * encoding doesn't include NUL bytes inside characters    *
 ***********************************************************/

/* this code executed before processing each line of input.
   - handles indents and appends via -Aa switches  */
char *handle_indents_and_appends(char *textbuf) {

  char *pptextbuf = textbuf; /* default */

  if( u_options & (1<<U_OPTION_INDENTED) ) {
    if( textbuf[0] == ' ' ) {
      pptextbuf = textbuf + 1; /* processing should ignore indent */
    } else {
      if( u_options & (1<<U_OPTION_APPEND) ) {
	fprintf(stdout, "%s", textbuf);
      }
      pptextbuf = NULL; /* no further processing */
    }
  }

  /* if appending, print the lines as they 
     come in before they are processed */
  if( u_options & (1<<U_OPTION_APPEND) ) {
    fprintf(stdout, " %s", pptextbuf);
  }

  return pptextbuf;
}

/***********************************************************
 * WIDE CHARACTER FILE HANDLING FUNCTIONS                  *
 * this is needed for any locale whose character set       *
 * encoding can include NUL bytes inside characters        *
 ***********************************************************/


/***********************************************************
 * MAIN FUNCTIONS                                          *
 ***********************************************************/
void learner_preprocess_fun() {
  init_learner(&learner);
}

void learner_post_line_fun(char *buf) {
  count_mbox_messages(&learner, mbox.state, buf);
}

void learner_postprocess_fun() {
  optimize_and_save(&learner);
}

void learner_cleanup_fun() {
  myfree_learner(&learner);
}

void learner_word_fun(char *tok, token_type_t tt, regex_count_t re) {
#ifdef DEBUG
  //LOG(stderr, "calling learner_word_fun \n");
#endif
  hash_word_and_learn(&learner, tok, tt, re);
#ifdef DEBUG
  //LOG(stderr, "After learner_word_fun \n");
#endif
}

void classifier_preprocess_fun() {
  category_count_t c;
  /* no need to "load" the categories, this is done in set_option() */

  if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
    init_empirical(&empirical, 
		   default_max_tokens, 
		   default_max_hash_bits); /* sets cached to zero */
  }

  if( u_options & (1<<U_OPTION_DUMP) ) {
    for(c = 0; c < cat_count; c++) {
      fprintf(stdout, "%s%10s ", (c ? " " : "# categories: "), cat[c].filename);
    }
    fprintf(stdout, "\n# format: ");

    if( u_options & (1<<U_OPTION_POSTERIOR) ) {
      fprintf(stdout, "score_delta\n");
    } else if( u_options & (1<<U_OPTION_SCORES) ) {
      fprintf(stdout, "avg_score * complexity\n");
    } else {
      fprintf(stdout, "lambda digref -renorm multi shannon id\n");
    }
  }
}

void classifier_cleanup_fun() {
#undef GOODGUY
#if defined GOODGUY
  /* normally we should myfree everything nicely, but there's no
     point, since the kernel will myfree the process memory. It's actually
     faster to not myfree... */
  category_count_t c;
  for(c = 0; c < cat_count; c++) {
    myfree_category(&cat[c]);
  }
  myfree_empirical(&empirical);
#endif
}


int email_line_filter(MBOX_State *mbox, char *buf) {
  return mbox_line_filter(mbox, buf, &xml);
}

#if defined HAVE_MBRTOWC
int w_email_line_filter(MBOX_State *mbox, wchar_t *buf) {
  return w_mbox_line_filter(mbox, buf, &xml);
}
#endif


int nvram_set_option(int op, char *optarg, FILE* input, struct MAPLIST  *maplist) {

	int c = 0;
	FILE *fp;
	int category_size = 0;

#ifdef NVRAM
	 int id; 
     int tid;
     int size;   
     char *outmap_addr = NULL;
#endif

#ifdef STATS
 //read time
 struct timeval start, end;
#endif

  switch(op) {
  case '0':
    u_options &= ~(1<<U_OPTION_NOZEROLEARN);
    break;
  case '1':
    u_options |= (1<<U_OPTION_NOZEROLEARN);
    break;
  case 'A':
    u_options |= (1<<U_OPTION_INDENTED);
    /* fall through */
  case 'a':
    u_options |= (1<<U_OPTION_APPEND);
    break;
  case 'e':
    if( !strcasecmp(optarg, "alnum") ) {
      m_options |= (1<<M_OPTION_CHAR_ALNUM);
    } else if( !strcasecmp(optarg, "alpha") ) {
      m_options |= (1<<M_OPTION_CHAR_ALPHA);
    } else if( !strcasecmp(optarg, "cef") ) {
      m_options |= (1<<M_OPTION_CHAR_CEF);
    } else if( !strcasecmp(optarg, "graph") ) {
      m_options |= (1<<M_OPTION_CHAR_GRAPH);
    } else if( !strcasecmp(optarg, "adp") ) {
      m_options |= (1<<M_OPTION_CHAR_ADP);
    } else if( !strcasecmp(optarg, "char") ) {
      m_options |= (1<<M_OPTION_CHAR_CHAR);
    } else {
      errormsg(E_WARNING,
	       "unrecognized option \"%s\", ignoring.\n", 
	       optarg);
    }
    c++;
    break;
  case 'D':
    u_options |= (1<<U_OPTION_DEBUG);
    break;
  case 'm':
#if defined HAVE_MMAP      
    u_options |= (1<<U_OPTION_MMAP);
#ifdef DEBUG
    LOG(stderr,"setting mmap \n");
#endif
#endif
    break;
  case 'M':
    m_options |= (1<<M_OPTION_MULTINOMIAL);
    break;
  case 'N':
    u_options |= (1<<U_OPTION_POSTERIOR);
    break;
  case 'R':
    if( cat_count >= MAX_CAT ) {
      errormsg(E_WARNING,
	       "maximum reached, random text category omitted\n");
    } else if( u_options & (1<<U_OPTION_LEARN) ) {
      errormsg(E_ERROR,"cannot use options -l and -R together\n");
      exit(1);
    } else {
      u_options |= (1<<U_OPTION_CLASSIFY);

      cat[cat_count].fullfilename = (char *)"random";

      init_category(&cat[cat_count]);
      init_purely_random_text_category(&cat[cat_count]);

      cat_count++;
    }
    break;
  case 'T':
    if( !strncasecmp(optarg, "text", 4) ) {
      /* this is the default */
      /* below, make sure the comparisons are in the correct order */
    } else if( !strcasecmp(optarg, "email:noplain") ) {
      m_options |= (1<<M_OPTION_NOPLAIN);
      m_options |= (1<<M_OPTION_MBOX_FORMAT);
    } else if( !strcasecmp(optarg, "email:plain") ) {
      m_options |= (1<<M_OPTION_PLAIN);
      m_options |= (1<<M_OPTION_MBOX_FORMAT);
    } else if( !strcasecmp(optarg, "email:headers") ) {
      m_options |= (1<<M_OPTION_HEADERS);
      m_options |= (1<<M_OPTION_MBOX_FORMAT);
    } else if( !strcasecmp(optarg, "email:noheaders") ) {
      m_options |= (1<<M_OPTION_NOHEADERS);
      m_options |= (1<<M_OPTION_MBOX_FORMAT);
    } else if( !strcasecmp(optarg, "email:atts") ) {
      m_options |= (1<<M_OPTION_ATTACHMENTS);
      m_options |= (1<<M_OPTION_MBOX_FORMAT);
    } else if( !strcasecmp(optarg, "email:xheaders") ) {
      m_options |= (1<<M_OPTION_XHEADERS);
      m_options |= (1<<M_OPTION_MBOX_FORMAT);
    } else if( !strcasecmp(optarg, "email:theaders") ) {
      m_options |= (1<<M_OPTION_THEADERS);
      m_options |= (1<<M_OPTION_MBOX_FORMAT);
    } else if( !strcasecmp(optarg, "email") ) {
      m_options |= (1<<M_OPTION_MBOX_FORMAT);
    } else if( !strcasecmp(optarg, "html:links") ) {
      m_options |= (1<<M_OPTION_SHOW_LINKS);
      m_options |= (1<<M_OPTION_HTML);
    } else if( !strcasecmp(optarg, "html:alt") ) {
      m_options |= (1<<M_OPTION_SHOW_ALT);
      m_options |= (1<<M_OPTION_HTML);
    } else if( !strcasecmp(optarg, "html:forms") ) {
      m_options |= (1<<M_OPTION_SHOW_FORMS);
      m_options |= (1<<M_OPTION_HTML);
    } else if( !strcasecmp(optarg, "html:scripts") ) {
      m_options |= (1<<M_OPTION_SHOW_SCRIPT);
      m_options |= (1<<M_OPTION_HTML);
    } else if( !strcasecmp(optarg, "html:styles") ) {
      m_options |= (1<<M_OPTION_SHOW_STYLE);
      m_options |= (1<<M_OPTION_HTML);
    } else if( !strcasecmp(optarg, "html:comments") ) {
      m_options |= (1<<M_OPTION_SHOW_HTML_COMMENTS);
      m_options |= (1<<M_OPTION_HTML);
    } else if( !strcasecmp(optarg, "html") ) {
      m_options |= (1<<M_OPTION_HTML);
    } else if( !strcasecmp(optarg, "xml") ) {
      m_options |= (1<<M_OPTION_XML);
    } else { /* default */
      errormsg(E_WARNING,
	       "unrecognized option -T %s, ignoring.\n", 
	       optarg);
    }
    c++;
    break;
  case 'U':
    u_options |= (1<<U_OPTION_VAR);
    m_options |= (1<<M_OPTION_CALCENTROPY);
    break;
  case 'V':
    fprintf(stdout, "dbacl version %s\n", VERSION);
    fprintf(stdout, COPYBLURB, "dbacl");
#if defined __GNUC__
    fprintf(stdout, "Using GNU modern regexes.\n");
#else
    fprintf(stdout, "Using system regexes.\n");
#endif
#if defined DIGITIZE_DIGRAMS
    fprintf(stdout, "Digrams are digitized.\n");
#endif
#if defined DIGITIZE_LAMBDA
    fprintf(stdout, "Lagrangian multipliers are digitized.\n");
#endif
#if defined DIGITIZE_LWEIGHTS
    fprintf(stdout, "Reference weights are digitized.\n");
#endif
#if defined PORTABLE_CATS
    fprintf(stdout, "Category files are portable to systems with other byte orders.\n");
#else
    fprintf(stdout, "Category files are NOT portable to systems with other byte orders.\n");
#endif
#if ! defined HAVE_MMAP
    fprintf(stdout, "Fast category loading with -m switch unavailable.\n");
#endif
    fprintf(stdout, "Feature memory requirements: %d bytes (classifying), %d bytes (learning)\n", 
	    (int)sizeof(c_item_t), (int)sizeof(l_item_t));
    fprintf(stdout, "To change these settings, recompile from source.\n");
    exit(0);
    break;
  case 'X':
    m_options |= (1<<M_OPTION_CALCENTROPY);
    u_options |= (1<<U_OPTION_CONFIDENCE);
    break;
  case 'd':
    u_options |= (1<<U_OPTION_DUMP);
    break;
  case 'h': /* select memory size in powers of 2 */
    default_max_hash_bits = atoi(optarg);
    if( default_max_hash_bits > MAX_HASH_BITS ) {
      errormsg(E_WARNING,
	       "maximum hash size will be 2^%d\n", 
	       MAX_HASH_BITS);
      default_max_hash_bits = MAX_HASH_BITS;
    }
    default_max_tokens = (1<<default_max_hash_bits);
    c++;
    break;
  case 'H': /* select memory size in powers of 2 */
    default_max_grow_hash_bits = atoi(optarg);
    if( default_max_grow_hash_bits > MAX_HASH_BITS ) {
      errormsg(E_WARNING,
	       "maximum hash size will be 2^%d\n", 
	       MAX_HASH_BITS);
      default_max_grow_hash_bits = MAX_HASH_BITS;
    }
    default_max_grow_tokens = (1<<default_max_grow_hash_bits);
    u_options |= (1<<U_OPTION_GROWHASH);
    c++;
    break;
  case 'j':
    m_options |= (1<<M_OPTION_CASEN);
    break;
  case 'n':
    u_options |= (1<<U_OPTION_SCORES);
    break;
  case 'c':
    if( cat_count >= MAX_CAT ) {
      errormsg(E_WARNING,
	       "maximum reached, category ignored\n");
    } else if( u_options & (1<<U_OPTION_LEARN) ) {
      errormsg(E_ERROR, "cannot use options -l and -c together\n");
      exit(1);
    } else {
      u_options |= (1<<U_OPTION_CLASSIFY);


    	cat[cat_count].fullfilename = sanitize_path(optarg, extn);

#ifdef DEBUG
		LOG(stderr, "After sanitize path \n");	
#endif

      if( !*optarg ) {
		errormsg(E_FATAL, "category needs a name\n");
      }

#ifndef NVRAM
#ifdef STATS
	gettimeofday(&start, NULL);
#endif

#ifdef DEBUG
	LOG(stderr, "cat file %s \n",cat[cat_count].fullfilename);	
#endif

      if( !load_category(&cat[cat_count]) ) {
		 errormsg(E_FATAL, "could not load category %s\n",
                 cat[cat_count].fullfilename);
      }
#ifdef STATS
   gettimeofday(&end, NULL);
   glob_read_time += simulation_time(start, end);
#endif


#else //NVRAM

#ifndef USE_NVMALLOC

	int fdesc;
	struct stat buf;

	fp = (FILE *)maplist[cat_count].dataptr;     
    if(!fp) {
  		return -1;
     }

	fdesc = fileno(fp);

	fstat(fdesc, &buf);

	category_size = buf.st_size;

    outmap_addr = (char *)mmap(0, buf.st_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
    if (outmap_addr == MAP_FAILED) {
	    close(fileno(fp));
        exit(-1);
    }

#else
	outmap_addr = (char *)maplist[cat_count].dataptr;
	category_size = maplist[cat_count].datasize;
#endif

    if(!outmap_addr) {
		LOG(stderr,"setoption:  nvread failed \n");
        return -1; 
    }


#ifdef STATS
	gettimeofday(&start, NULL);
#endif

#ifdef DEBUG
	//print_content(outmap_addr);
	LOG(stderr,"nvram_Set_option: cat_count %d \n", cat_count);
  	LOG(stderr,"nvram_Set_option: before load category \n");
#endif

  	LOG(stderr,"nvram_Set_option: %d \n", category_size);


	if( !nvram_load_category(&cat[cat_count], category_size, outmap_addr) ) {
		errormsg(E_FATAL, "nvmram could not load category %s\n",
		cat[cat_count].fullfilename);
	}

#ifdef STATS
   gettimeofday(&end, NULL);
   //glob_read_time += simulation_time(start, end);
#endif


#ifdef DEBUG
       LOG(stderr,"finished loading category\n");
#endif
#endif

      if( sanitize_model_options(&m_options,&cat[cat_count]) ) {
	ngram_order = (ngram_order < cat[cat_count].max_order) ? 
	  cat[cat_count].max_order : ngram_order;
	cat_count++;
      } 
    }
    c++;
    break;
  case 'q':
    quality = atoi(optarg);
    switch(quality) {
    case 1:
      qtol_div = 0.01;
      qtol_lam = CLIP_LAMBDA_TOL(0.05);
      qtol_logz = 0.05;
      break;
    case 2:
      qtol_div = 0.01;
      qtol_lam = CLIP_LAMBDA_TOL(0.01);
      qtol_logz = 0.05;
      break;
    case 3:
      qtol_div = 0.01;
      qtol_lam = CLIP_LAMBDA_TOL(0.01);
      qtol_logz = 0.01;
      break; 
    case 4:
      qtol_div = 0.01;
      qtol_lam = CLIP_LAMBDA_TOL(0.01);
      qtol_logz = 0.01;
      qtol_multipass = 1;
      break; 
    default:
      errormsg(E_FATAL,
	       "the -q switch needs a number between 1(fastest) and 4(best quality)\n");
      break;
    }
    c++;
    break;
  case 'r':
    m_options |= (1<<M_OPTION_REFMODEL);
    break;
  case 'w':
    ngram_order = atoi(optarg);
    if( !*optarg || (ngram_order < 1) || (ngram_order > 7) ) {
      errormsg(E_FATAL,
	       "the -w switch needs a number between 1 and 7\n");
    }
    c++;
    break;
  case 'S':
    m_options |= (1<<M_OPTION_NGRAM_STRADDLE_NL);
    break;
  case 'x':
    decimation = atoi(optarg);
    if( !*optarg || (decimation < 1) || (decimation > MAX_HASH_BITS) ) {
      errormsg(E_WARNING,
	       "option -x ignored, needs an integer between 1 and %d\n", 
	       MAX_HASH_BITS);
    } else {
      u_options |= (1<<U_OPTION_DECIMATE);
      srand(0);
    }
    c++;
    break;
  case 'f':
    if( filter_count >= MAX_CAT ) {
      errormsg(E_WARNING, "maximum reached, filter ignored\n");
    } else if( u_options & (1<<U_OPTION_LEARN) ) {
      errormsg(E_ERROR, "cannot use options -l and -f together\n");
      exit(1);
    } else if( !*optarg ) {
      errormsg(E_ERROR, "filter must be category name or number\n");
      exit(1);
    } else {
      u_options |= (1<<U_OPTION_FILTER);
      filter[filter_count] = -1;

      /* see if it's a known category */
      for(c = 0; c < cat_count; c++) {
	if( !strcmp(cat[c].filename, optarg) ) {
	  filter[filter_count] = c;
	  break;
	}
      }
      /* if not recognized, see if it's a number */
      if( filter[filter_count] < 0 ) {
	filter[filter_count] = atoi(optarg) - 1;
      }
      if( filter[filter_count] < 0 ) { /* still not recognized */
	errormsg(E_ERROR, "unrecognized category in -f option [%s]\n", 
		 optarg);
	exit(1);
      }
      filter_count++;
    }
    c++;
    break;
  case 'v':
    u_options |= (1<<U_OPTION_VERBOSE);
    break;
  case 'L':
    if( *optarg && !strcmp(optarg, "uniform") ) {
      u_options |= (1<<U_OPTION_LAPLACE);
    } else if(*optarg && !strcmp(optarg, "dirichlet") ) {
      u_options |= (1<<U_OPTION_DIRICHLET);
    } else if(*optarg && !strcmp(optarg, "maxent") ) {
      u_options |= (1<<U_OPTION_JAYNES);
    } else {
      errormsg(E_FATAL, "-L option needs \"uniform\", \"dirichlet\" or \"maxent\"\n");
    }
    c++;
    break;
  case 'l': 
    if( u_options & (1<<U_OPTION_CLASSIFY) ) {
      errormsg(E_ERROR,
	       "cannot use options -l and -c together\n");
      exit(1);
    } else if( u_options & (1<<U_OPTION_LEARN) ) {
      errormsg(E_ERROR,
	       "option -l can only occur once\n");
      exit(1);
    } else if( !*optarg ) {
      errormsg(E_ERROR, "category name must not be empty\n");
    } else {
      u_options |= (1<<U_OPTION_LEARN);
#ifdef DEBUG
      LOG(stderr, "Calling sanitize_path %s \n", optarg); 
#endif
     learner.filename = sanitize_path(optarg, extn);
      if( !*learner.filename ) {
	errormsg(E_ERROR, "category needs a name\n");
	exit(1);
      }
#ifdef DEBUG
      LOG(stderr, "learner.filename %s \n",learner.filename);
#endif
    }
    c++;
    break;
  case 'o':
    if( !*optarg ) {
      errormsg(E_ERROR, "category name must not be empty\n");
    } else {
#ifdef DEBUG
      LOG(stderr,"calling sanitize_path \n");
      LOG(stderr,"optarg %s \n",optarg);
#endif
      online = sanitize_path(optarg, (char *)"");
    }
    c++;
    break;
  case 'G':
  case 'g':
    /* if regex can't be compiled, load_regex() exits */
    learner.retype |= (1<<load_regex(optarg));
    c++;
    break;
  case 'i':
    m_options |= (1<<M_OPTION_I18N);
#if defined HAVE_LANGINFO_H
    if( !strcmp(nl_langinfo(CODESET), "UTF-8") ) {
      errormsg(E_WARNING, "you have UTF-8, so -i is not needed.\n");
    }
#endif
#if !defined HAVE_MBRTOWC
    errormsg(E_WARNING, "this tool was compiled without wide character support. Full internationalization is disabled.\n");
    m_options &= ~(1<<M_OPTION_I18N);
#endif
    break;
  case 'z':
    ftreshold = atoi(optarg);
    c++;
    break;
  default:
    c--;
    break;
  }
  return c;

}


void sanitize_options() {
  category_count_t c;

  /* consistency checks */
  if( ((u_options>>U_OPTION_CLASSIFY) & 1) + 
      ((u_options>>U_OPTION_LEARN) & 1) != 1 ) {
    errormsg(E_ERROR, "please use either -c or -l option.\n");
    exit(1);
  }

  if( *online && (m_options & (1<<M_OPTION_CALCENTROPY)) ) {
    errormsg(E_WARNING, 
	     "-X switch cannot be used with -o switch, disabling -X.\n");
    m_options &= ~(1<<M_OPTION_CALCENTROPY);
  }

  if( (u_options & (1<<U_OPTION_DECIMATE)) &&
      !(u_options & (1<<U_OPTION_LEARN)) ) {
    errormsg(E_WARNING,
	    "option -x ignored, applies only when learning.\n");
    u_options &= ~(1<<U_OPTION_DECIMATE);
  }

  if( u_options & (1<<U_OPTION_DUMP) ) {
    if( u_options & (1<<U_OPTION_VERBOSE) ) {
      u_options &= ~(1<<U_OPTION_VERBOSE); /* verbose writes garbage to stdout */
      u_options &= ~(1<<U_OPTION_DEBUG);
    }
  }

  if( !(m_options & (1<<M_OPTION_TEXT_FORMAT)) &&
      !(m_options & (1<<M_OPTION_MBOX_FORMAT)) &&
      !(m_options & (1<<M_OPTION_HTML)) &&
      !(m_options & (1<<M_OPTION_XML)) ) {
    m_options |= (1<<M_OPTION_TEXT_FORMAT);
  }

  if( ((m_options>>M_OPTION_TEXT_FORMAT) & 1) + 
      ((m_options>>M_OPTION_MBOX_FORMAT) & 1) > 1 ) {
    errormsg(E_ERROR,
	    "please use only one of either -T text or -T email options.\n");
    exit(1);
  }

  if( ((m_options>>M_OPTION_XML) & 1) + 
      ((m_options>>M_OPTION_HTML) & 1) > 1 ) {
    errormsg(E_ERROR,
	    "please use only one of either -T xml or -T html options.\n");
    exit(1);
  }


  if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
    /* mbox format needs html unless user wants xml */
    if( !(m_options & (1<<M_OPTION_XML)) ) {
      m_options |= (1<<M_OPTION_HTML);
    }
  }


  if( (u_options & (1<<U_OPTION_APPEND)) &&
      (u_options & (1<<U_OPTION_FILTER)) ) {
    u_options &= ~(1<<U_OPTION_APPEND);
    errormsg(E_WARNING,
	    "disabling option -a, because it cannot be used with -f.\n");
  }

  /* decide if we need some options */

  if( u_options & (1<<U_OPTION_LEARN) ) {
    if( !regex_count ) {
      m_options |= (1<<M_OPTION_USE_STDTOK);
    } else {
      m_options &= ~(1<<M_OPTION_USE_STDTOK);
    }
  }

  if( m_options & (1<<M_OPTION_I18N) ) {
    /* I've removed the internationalized regex code, because it makes
       the code too complex to handle both multibyte and wide char regexes
       simultaneously. */
    errormsg(E_WARNING,
	     "this version of dbacl does not support -g and -i switches simultaneously, disabling -i.\n");
    m_options &= ~(1<<M_OPTION_I18N);
  }

  if( m_options & (1<<M_OPTION_MULTINOMIAL) ) { 
    if( cat_count == 1 ) {
      if( cat[0].model_type == simple ) {
	m_options |= (1<<M_OPTION_CALCENTROPY);
      }
    } else if( cat_count > 1 ) {
      for(c = 1; c < cat_count; c++) {
	if( cat[c].model_type == sequential ) { break; }
	if( cat[c].retype != cat[c-1].retype ) { break; }
      }
      if( c == cat_count ) {
	m_options |= (1<<M_OPTION_CALCENTROPY);
      }
    }
    if( !(m_options & (1<<M_OPTION_CALCENTROPY)) ) {
      errormsg(E_WARNING,
	      "not all categories support multinomial calculations, disabling (-M switch)\n");
    }
  }

  /* unless overridden, we use alpha character classes only */
  if( !(m_options & (1<<M_OPTION_CHAR_ALPHA)) &&
      !(m_options & (1<<M_OPTION_CHAR_ALNUM)) &&
      !(m_options & (1<<M_OPTION_CHAR_CEF)) &&
      !(m_options & (1<<M_OPTION_CHAR_ADP)) &&
      !(m_options & (1<<M_OPTION_CHAR_CHAR)) &&
      !(m_options & (1<<M_OPTION_CHAR_GRAPH)) ) {
    if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
      m_options |= (1<<M_OPTION_CHAR_ADP);
    } else {
      m_options |= (1<<M_OPTION_CHAR_ALPHA);
    }
  }

  if( !(u_options & (1<<U_OPTION_JAYNES)) &&
      !(u_options & (1<<U_OPTION_LAPLACE)) &&
      !(u_options & (1<<U_OPTION_DIRICHLET)) ) {
    if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
      u_options |= (1<<U_OPTION_LAPLACE);
    } else {
      u_options |= (1<<U_OPTION_DIRICHLET);
    }
  }
}


void* mymalloc(size_t size) {

    int idx =0;

    void *ptr = NULL;

    if(size < 1)
        goto failure;

    ptr = (void *)malloc(size);
    if(!ptr){
        goto failure;
    }

#ifdef TRACK_MEMORY
  results[memcntr] = (unsigned long)ptr;
  memcntr++;
#endif
    return ptr;

failure:
    return NULL;
}


void myfree(void *ptr) {

#ifdef TRACK_MEMORY
    map<int, unsigned long>::iterator ii;   
    unsigned long addr = 0;
#endif

     if(ptr == NULL)
	goto failure;

     //free(ptr);
     ptr = NULL;

#ifdef TRACK_MEMORY
   
   for( ii=results.begin(); ii!=results.end(); ++ii){

	addr = (*ii).second;
   
        //if( addr != 0  &&  addr == (unsigned long)ptr )
          // results.erase ((*ii).first);

          memcntr--;
   }
#endif
   return;	

failure:
 	return;	
}

#ifdef TRACK_MEMORY

int garbage_collect() {

    map<int, unsigned long>::iterator ii;
    unsigned long addr = 0;

   for( ii=results.begin(); ii!=results.end(); ++ii){

        addr = (*ii).second;

        if( addr != 0) {
           free((void *)addr);
           results.erase ((*ii).first);
        }
        memcntr--;
   }

   return 0;
}

#endif


//Needs to develop more
int myfreedouble( void **ptr, int num) {

    int idx =0;
        if(ptr) {
          for(idx = 0; idx < num; idx++){
                if(ptr[idx])
                        free(ptr[idx]);
          }
      //free(ptr);
      return 0;
        }
    return -1;
}

//Needs to develop more
//type is hard coded
void** myalloc( int num, size_t size) {

    int idx =0;

    char **ptr = NULL;

    if(size < 1)
        goto failure;

    ptr = (char **)calloc(num, sizeof(int));
    if(!ptr){
        goto failure;
    }
    for(idx = 0; idx < num; idx++){
        ptr[idx]= (char *)calloc(size, sizeof(char));
        if(!ptr[idx])
         goto failure;
    }
    return (void **)ptr;
failure:
    return NULL;
}


#ifdef NVRAM

int create_online_file() {

  int online_file = -1;
  char *addr = NULL;
 
  if(online){
 	
    //online_file= setup_map_file(online, 50 * 65536);
    //online_file= imc_mem_obj_create(region_size * 5);
#ifdef DEBUG
    LOG(stderr,"create_online_file: %d \n",region_size);    
#endif

    if(online_fd >= 0 ){

    addr = (char *)mmap(0, region_size * 15 , PROT_READ | PROT_WRITE, MAP_SHARED, online_fd, 0);
    if (addr == MAP_FAILED) {
	close(online_fd);
        exit(-1);
    }
    
  }else{
    LOG(stderr, "creating mmap start failed \n");
    return 0;
   }
  
   learner.mmap_start = (byte_t*)addr;
  }
}


int initialize_tmp_file() {

	size_t offset =0;

   if(learner.tmp.file){

	learner.tmp.mmap_length = learner.tmp.avail + system_pagesize + region_size;
    learner.tmp.mmap_start =
          (byte_t *)mmap(0, learner.tmp.mmap_length,
                         PROT_READ|PROT_WRITE, MAP_SHARED,
                         fileno(learner.tmp.file), offset);
    if( learner.tmp.mmap_start == MAP_FAILED ) { learner.tmp.mmap_start = NULL; }
    if( learner.tmp.mmap_start ) {
        learner.tmp.mmap_offset = learner.tmp.offset - offset;
        learner.tmp.mmap_cursor = 0;
     } else {
        LOG(stderr,"initialize_tmp_file: initialize_tmp_file failed \n");
        learner.tmp.mmap_length = 0;
        return 0;
     }
   }else {
		LOG(stderr,"initialize_tmp_file: file null \n");
        return 0;
   }

   return 1;

  }

#endif


//int main( int argc, char *argv[]) {

int learn_or_classify_data(int argc, char **argv, FILE *input, FILE *output_fp, 
							int datasize, struct MAPLIST *maplist, char *input_map,
							char *outmapaddr){

	  //FILE *input;
	  signed char op;
	  struct stat statinfo;
	  int temp =0;

	  void (*preprocess_fun)(void) = NULL;
	  void (*word_fun)(char *, token_type_t, regex_count_t) = NULL;
	  char *(*pre_line_fun)(char *) = NULL;
	  void (*post_line_fun)(char *) = NULL;
	  void (*postprocess_fun)(void) = NULL;
	  void (*cleanup_fun)(void) = NULL;
	  int (*line_filter)(MBOX_State *, char *) = NULL;
	  void (*character_filter)(XML_State *, char *) = NULL; 
#if defined HAVE_MBRTOWC
	  int (*w_line_filter)(MBOX_State *, wchar_t *) = NULL;
	  void (*w_character_filter)(XML_State *, wchar_t *) = NULL; 
#endif

	  progname = (char *)"dbacl";
	  inputfile = (char *)"stdin";
	  inputline = 0;

	  learner.filename = NULL;
	  learner.retype = 0;

#ifdef NVRAM
	  learner.outfp = output_fp;
	  learner.outmapaddr = outmapaddr;
#endif

	   optind = 0;

		//we set the default number of hash token 
	    default_max_hash_bits = 20;
    	if( default_max_hash_bits > MAX_HASH_BITS ) {
		     errormsg(E_WARNING,  "maximum hash size will be 2^%d\n",
	         MAX_HASH_BITS);
    		 default_max_hash_bits = MAX_HASH_BITS;
	    }
    	default_max_tokens = (1<<default_max_hash_bits);

#if defined(HAVE_GETPAGESIZE)
		 system_pagesize = getpagesize();
#endif

	  if( system_pagesize == -1 ) { system_pagesize = BUFSIZ; }

		  init_signal_handling();

		  /* parse the options */
		  while( (op = getopt(argc, argv, 
			      "01Aac:Dde:f:G:g:H:h:ijL:l:mMNno:q:RrST:UVvw:x:Xz:")) > -1 ) {

			   	//LOG(stderr,"CALLING GETOPT\n");
			     nvram_set_option(op, optarg, input, maplist);
#ifdef NVRAM
		     if(op == 'o') {
   			    optind--;
				break;
		     }
#endif
		  }

#ifdef DEBUG
		  LOG(stderr,"optind %d \n", optind);
#endif

  /* end option processing */
  sanitize_options();

  temp_file = imc_mem_obj_create(region_size * 10);
   if(temp_file < 0){
        LOG(stderr,"creating memory object failed \n");
    return -1;
   }
  learner.tmp.file = fdopen(temp_file,"w");

  /* set up callbacks */
  if( u_options & (1<<U_OPTION_CLASSIFY) ) {

    preprocess_fun = classifier_preprocess_fun;
    word_fun = score_word;
    if( u_options & (1<<U_OPTION_FILTER) ) {
      u_options |= (1<<U_OPTION_FASTEMP);
      empirical.track_features = 1; 
      post_line_fun = line_score_categories;
      postprocess_fun = NULL;
    } else {
      post_line_fun = NULL;
      postprocess_fun = score_categories;
    }
    cleanup_fun = classifier_cleanup_fun;

  } else if( u_options & (1<<U_OPTION_LEARN) ) {
    /* category learning */

    preprocess_fun = learner_preprocess_fun;
    word_fun = learner_word_fun;
    if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
      post_line_fun = learner_post_line_fun;
    } else {
      post_line_fun = NULL;
    }
    postprocess_fun = learner_postprocess_fun;
    cleanup_fun = learner_cleanup_fun;

  } else { /* something wrong ? */
    usage(argv);
    exit(1);
  }

  /* handles some common filtering options */
  if( (u_options & (1<<U_OPTION_INDENTED)) ||
      (u_options & (1<<U_OPTION_APPEND)) ) {
    pre_line_fun = handle_indents_and_appends;
  }

  if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
    line_filter = email_line_filter;
  }

  if( (m_options & (1<<M_OPTION_XML)) ||
      (m_options & (1<<M_OPTION_HTML)) ) {
    character_filter = xml_character_filter;
  }

#if defined HAVE_MBRTOWC
  if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
    w_line_filter = w_email_line_filter;
  }

  if( (m_options & (1<<M_OPTION_XML)) ||
      (m_options & (1<<M_OPTION_HTML)) ) {
    w_character_filter = w_xml_character_filter;
  }
#endif

  if( preprocess_fun ) { (*preprocess_fun)(); }

  	init_file_handling();


	  /* now process each file on the command line,
    	 or if none provided read stdin */
	  while( optind < argc && (optind > -1) && *(argv + optind) && !(cmd & (1<<CMD_QUITNOW)) ) {

	    /* if it's a filename, process it */
#ifndef NVRAM
   		    input = fopen(argv[optind], "rb");
#endif

#ifdef DEBUG
			LOG(stderr, "arguments %s %d \n",argv[optind], optind);
			LOG(stderr, "Opening file %s %d \n",argv[optind], optind); 
#endif

		    if( input ) {
    		  inputfile = argv[optind];
		      u_options |= (1<<U_OPTION_STDIN);

		      if( (u_options & (1<<U_OPTION_VERBOSE)) && 
				  !(u_options & (1<<U_OPTION_CLASSIFY))) {
					fprintf(stdout, "processing file %s\n", argv[optind]);
		      }

		      /* set some initial options */
		      reset_xml_character_filter(&xml, xmlRESET);
      
		      if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
					reset_mbox_line_filter(&mbox);
			  }

#ifndef NVRAM
		       process_file(input, line_filter, character_filter,
		       word_fun, pre_line_fun, post_line_fun);
#else
#ifdef DEBUG
		       LOG(stderr, "Going to process file \n");
 #endif
		       nvram_process_file(input, line_filter, character_filter,
        		      word_fun, pre_line_fun, post_line_fun, datasize, input_map);
#endif

#ifndef NVRAM
		      fclose(input);

#else
				break;
#endif

	    } else { /* unrecognized file name */

    	  errormsg(E_ERROR, "couldn't open %s\n", argv[optind]);
	      usage(argv);
	      exit(1);

	    }
	    optind++;
	  }

  /* in case no files were specified, get input from stdin,
   * but note we reopen it in binary mode 
   */
  if( !(u_options & (1<<U_OPTION_STDIN)) && 0 ) {

     LOG(stderr,"Entering stdin \n");
    input = fdopen(fileno(stdin), "rb");
    if( input ) {

      if( (u_options & (1<<U_OPTION_VERBOSE)) && 
	  !(u_options & (1<<U_OPTION_CLASSIFY)) ) {
	fprintf(stdout, "taking input from stdin\n");
      }

      /* set some initial options */
      reset_xml_character_filter(&xml, xmlRESET);

      if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
		reset_mbox_line_filter(&mbox);
      }

      if( !(m_options & (1<<M_OPTION_I18N)) ) {
	process_file(input, line_filter, character_filter,
		     word_fun, pre_line_fun, post_line_fun);
      } else {
#if defined HAVE_MBRTOWC
	w_process_file(input, w_line_filter, w_character_filter,
		       word_fun, pre_line_fun, post_line_fun);
#else
	errormsg(E_ERROR, "international support not available (recompile).\n");
#endif
      }

      /* must close before freeing in_iobuf, in case setvbuf was called */
      fclose(input); 
    }
  }

#ifdef DEBUG
  LOG(stderr,"Before post process \n");
#endif

  if( postprocess_fun ) { (*postprocess_fun)(); }

#ifdef DEBUG
  LOG(stderr,"After post process \n");
#endif

  if(learner.hash) {
   //free(learner.hash);  
   //learner.hash= NULL;
  }
  cleanup_file_handling();

  if( cleanup_fun ) { (*cleanup_fun)(); }

  free_all_regexes();

  cleanup_signal_handling();

#ifdef TRACK_MEMORY
#ifdef DEBUG
   LOG(stderr,"Total Unfreed memory %d \n", memcntr);
   LOG(stderr,"learner.outmapaddr %s %ld \n", learner.outmapaddr,(unsigned long)learner.outmapaddr);

#endif

   if(memcntr) {
//	garbage_collect();
   }

#endif

  //LOG(stderr,"%s \n", argv[0]);
  //free(argv[0]);

  return exit_code;
}




