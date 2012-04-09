/* src/config.h.  Generated from config.h.in by configure.  */
/* src/config.h.in.  Generated from configure.in by autoheader.  */

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the <features.h> header file. */
#define HAVE_FEATURES_H 1

/* Define to 1 if you have the `getpagesize' function. */
//#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* ncurses needed for readline */
/* #undef HAVE_LIBNCURSES */

/* readline needed for interactive mailinspect */
/* #undef HAVE_LIBREADLINE */

/* slang needed for interactive mailinspect */
/* #undef HAVE_LIBSLANG */

/* Define to 1 if you have the `madvise' function. */
//#define HAVE_MADVISE 1

/* Define to 1 if mbrtowc and mbstate_t are properly declared. */
//#define HAVE_MBRTOWC 1

/* Define to 1 if you have the `memalign' function. */
/* #undef HAVE_MEMALIGN */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <mman.h> header file. */
/* #undef HAVE_MMAN_H */

/* Define to 1 if you have a working `mmap' system call. */
#define HAVE_MMAP 1

/* Define to 1 if you have the <netinet/in.h> header file. */
//#define HAVE_NETINET_IN_H 1

/* Define to 1 if `posix_memalign' works. */
//#define HAVE_POSIX_MEMALIGN 1

/* Define to 1 if you have the `sigaction' function. */
//#define HAVE_SIGACTION 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `valloc' function. */
/* #undef HAVE_VALLOC */

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the `wcstol' function. */
#define HAVE_WCSTOL 1

/* Define to 1 if you have the <wctype.h> header file. */
#define HAVE_WCTYPE_H 1

/* this is a Mac system */
/* #undef OS_DARWIN */

/* this is an HP-UX system */
/* #undef OS_HPUX */

/* this is a Linux system */
#define OS_LINUX 

/* this is a Sun system */
/* #undef OS_SUN */

/* this is an unrecognized system */
/* #undef OS_UNKNOWN */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "laird@lbreyer.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "dbacl"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "dbacl 1.13"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "dbacl"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.13"

/* Define to 1 if the C compiler supports function prototypes. */
#define PROTOTYPES 1

/* Define to 1 if the `setvbuf' function takes the buffering type as its
   second argument and the buffer pointer as the third, as on System V before
   release 3. */
/* #undef SETVBUF_REVERSED */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* identify processor if categories not portable */
#define TARGETCPU "x86_64-unknown-linux-gnu"

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#define YYTEXT_POINTER 1

/* Define to 1 if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* # undef _ALL_SOURCE */
#endif

/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define like PROTOTYPES; this can be used by system headers. */
#define __PROTOTYPES 1

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */


#define _POSIX_PATH_MAX 256
