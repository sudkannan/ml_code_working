/*
 * Copyright (C) 2002 Laird Breyer
 *
 * This program is free software; you can redistribute it and/or modify
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

#ifdef HAVE_CONFIG_H
#undef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>

#if defined HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "util.h"
#include "dbacl.h"

extern options_t u_options;
extern options_t m_options;

extern myregex_t re[MAX_RE];
extern regex_count_t regex_count;
extern regex_count_t antiregex_count;

extern MBOX_State mbox;
extern XML_State xml;

extern char *textbuf;
extern charbuf_len_t textbuf_len;

extern char *aux_textbuf;
extern charbuf_len_t aux_textbuf_len;

#if defined HAVE_MBRTOWC
extern wchar_t *wc_textbuf;
extern charbuf_len_t wc_textbuf_len;
#endif

extern token_order_t ngram_order;

extern long system_pagesize;

extern void *in_iobuf;
extern void *out_iobuf;

extern int cmd;

extern char *inputfile;
extern long inputline;

/***********************************************************
 * EXPERIMENTAL:                                           *
 * this code is an experiment to see if memory mapping is  *
 * faster than buffered I/O. Surprisingly, the gain is at  *
 * best marginal (less than two percent on my test).       *
 * The code is left in for future experiments              *
 *                                                         *
 * I suspect that mmap() isn't faster because we only read *
 * the input files sequentially, never backtrack, and my   *
 * Debian development system probably reads ahead quite    *
 * agressively. So normal buffered I/O does about the same *
 * amount of work, In any case, most of the time is spent  *
 * in the hash tables and calculations anyway.             *
 *                                                         *
 * The mmap() code below is written so as to easily replace*
 * the buffered I/O functions                              *
 ***********************************************************/

//#undef EXPERIMENTAL
#define EXPERIMENTAL
#if defined EXPERIMENTAL

#include <sys/stat.h>
#include <errno.h>

typedef struct {
	FILE *f;
	size_t fsize;
	size_t fmappos;
	size_t fmapsize;
	char *seekpos;
	char *mstart;
} MMFILE;

int MMEOF(MMFILE *stream) {
	return stream->mstart && ((stream->seekpos - stream->mstart) +
			stream->fmappos >= stream->fsize);
}

MMFILE *MMOPEN(FILE *f) {
	MMFILE *stream = NULL;
	struct stat sb;
	if( f ) {
		stream = (MMFILE *)mymalloc(sizeof(MMFILE));
		if( stream ) {
			stream->f = f;
#define MEGABYTES 20
			stream->fmapsize = MEGABYTES*1024*1024;
			if( fstat(fileno(f), &sb) == -1 ) {
				myfree(stream);
				return NULL;
			}
			stream->fsize = sb.st_size;
			stream->fmappos = 0;
			stream->mstart = (char *)MMAP(NULL, stream->fmapsize, PROT_READ, MAP_SHARED,
					fileno(stream->f), stream->fmappos);
			if( stream->mstart == MAP_FAILED ) {
				myfree(stream);
				return NULL;
			}
			stream->seekpos = stream->mstart;

			if( stream->mstart ) {
				MADVISE(stream->mstart, stream->fmapsize, MADV_SEQUENTIAL);
			}
		}
	}
	return stream;
}

int MMCLOSE(MMFILE *stream) {
	int r = 0;
	if( stream ) {
		//r = MUNMAP(stream->mstart, stream->fmapsize);
		r = munmap(stream->mstart, stream->fmapsize);
		myfree(stream);
	}
	return r;
}

void MMFORWARD(MMFILE *stream) {
	size_t offset;
	if( stream->fmappos + stream->fmapsize < stream->fsize ) {
		offset = stream->seekpos - stream->mstart;
		/*if( MUNMAP(stream->mstart, stream->fmapsize) == -1 ) {
      free(stream);
      exit(0);
    }*/

		if( munmap(stream->mstart, stream->fmapsize) == -1 ) {
			myfree(stream);
			exit(0);
		}


		stream->fmappos += system_pagesize * (offset/system_pagesize);
		stream->mstart = (char *)MMAP(0, stream->fmapsize, PROT_READ, MAP_SHARED,
				fileno(stream->f), stream->fmappos);
		if( stream->mstart == MAP_FAILED ) {
			myfree(stream);
			exit(0);
		}
		stream->seekpos = stream->mstart +
				(offset - system_pagesize * (offset/system_pagesize));
		if( stream->mstart ) {
			MADVISE(stream->mstart, stream->fmapsize, MADV_SEQUENTIAL);
		}
	}
}

char *MMGETS(char *s, size_t size, MMFILE *stream) {
	char *result = NULL;
	size_t left;

	left = stream->fmapsize - (stream->seekpos - stream->mstart);
	if( left < size ) {
		MMFORWARD(stream);
		left = stream->fmapsize - (stream->seekpos - stream->mstart);
	}

	if( left <= 0 ) {
		return NULL;
	} else if( size <= left + 1) {
		result = (char *)memccpy(s, stream->seekpos, '\n', size - 1);
		if( result ) {
			*result = '\0';
			stream->seekpos += result - s;
			return s;
		}
	} else {
		result = (char *)memccpy(s, stream->seekpos, '\n', left);
		if( result ) {
			*result = '\0';
			stream->seekpos = result;
		} else {
			memcpy(s, stream->seekpos, left);
			s[left] = '\0';
			stream->seekpos += left;
		}
		return s;
	}
	return NULL;
}

#endif


/***********************************************************
 * MISCELLANEOUS FILE HANDLING                             *
 ***********************************************************/

void init_file_handling() {
	init_buffers();
	init_mbox_line_filter(&mbox);
}

void cleanup_file_handling() {
	free_mbox_line_filter(&mbox);
	cleanup_buffers();
}

/* Given a mime type, this selects an appropriate
 * xml default setting for the character filter.
 */
XML_Reset select_xml_defaults(MIME_Struct *mime) {
	switch(mime->type) {
	case ctUNDEF:
	case ctMESSAGE_RFC822:
	case ctTEXT_PLAIN:
		return (m_options & (1<<M_OPTION_NOPLAIN)) ? xmlDUMB : xmlDISABLE;
	case ctTEXT_RICH:
	case ctTEXT_UNKNOWN:
		return (m_options & (1<<M_OPTION_PLAIN)) ? xmlDISABLE : xmlDUMB;
	case ctTEXT_HTML:
		return (m_options & (1<<M_OPTION_PLAIN)) ? xmlDISABLE : xmlSMART;
	case ctIMAGE:
	case ctAUDIO:
	case ctVIDEO:
	case ctMODEL:
	case ctOTHER:
	case ctOCTET_STREAM:
	case ctAPPLICATION_MSWORD:
	case ctTEXT_XML:
	case ctTEXT_SGML:
		return (m_options & (1<<M_OPTION_NOPLAIN)) ? xmlDUMB : xmlDISABLE;
	}
	return xmlUNDEF;
}

void reset_xml_character_filter(XML_State *xml, XML_Reset reset) {
	if( xml ) {
		switch(reset) {
		case xmlRESET:
			xml->hide = VISIBLE;
			xml->state = TEXT;
			if( m_options & (1<<M_OPTION_HTML) ) {
				xml->parser = xpSMART;
			} else if( m_options & (1<<M_OPTION_XML) ) {
				xml->parser = xpDUMB;
			} else {
				xml->parser = xpSMART;
			}
			break;
		case xmlDISABLE:
			xml->state = DISABLED;
			break;
		case xmlSMART:
			if( (xml->state == DISABLED) ||
					((xml->parser != xpHTML) && (xml->parser != xpSMART)) ) {
				xml->state = TEXT;
				xml->hide = VISIBLE;
				xml->parser = xpSMART;
			}
			break;
		case xmlHTML:
			if( (xml->state == DISABLED) ||
					((xml->parser != xpHTML) && (xml->parser != xpSMART)) ) {
				xml->state = TEXT;
				xml->hide = VISIBLE;
				xml->parser = xpHTML;
			}
			break;
		case xmlDUMB:
			if( (xml->state == DISABLED) ||
					(xml->parser != xpDUMB) ) {
				xml->state = TEXT;
				xml->hide = VISIBLE;
				xml->parser = xpDUMB;
				break;
			}
		case xmlUNDEF:
			/* ignore */
			break;
		}
	}
}


void reset_mbox_line_filter(MBOX_State *mbox) {
	mbox->state = msUNDEF;
	mbox->substate = msuUNDEF;
	mbox->header.type = mbox->body.type = ctUNDEF;
	mbox->header.encoding = mbox->body.encoding = ceUNDEF;
	mbox->prev_line_empty = 1;
	mbox->corruption_check = 0;
	mbox->skip_header = 0;
	mbox->skip_until_boundary = 0;
	mbox->strip_header_char = '\0';
#if defined HAVE_MBRTOWC
	mbox->w_strip_header_char = L'\0';
#endif
	mbox->plainstate = psPLAIN;
	memset(&mbox->boundary, 0, sizeof(mbox->boundary));

	/* no need to reserve space for both char and wide char caches */
	if( m_options & (1<<M_OPTION_I18N) ) {
#if defined HAVE_MBRTOWC
		w_init_decoding_caches(mbox);
#endif
	} else {
		init_decoding_caches(mbox);
	}
}

void init_mbox_line_filter(MBOX_State *mbox) {
	memset(mbox, 0, sizeof(MBOX_State));
	reset_mbox_line_filter(mbox);
}

void free_mbox_line_filter(MBOX_State *mbox) {
	/* no need to reserve space for both char and wide char caches */
	if( m_options & (1<<M_OPTION_I18N) ) {
#if defined HAVE_MBRTOWC
		w_free_decoding_caches(mbox);
#endif
	} else {
		free_decoding_caches(mbox);
	}
}

/* the token class is a common label for a subset of features,
   such as e.g. all features which appear in the header. The label
   should be a number greater than AMIN. If all tokens have the same
   class, then we effectively obtain the dbacl 1.7 and earlier behaviour.
   IT DOESN"T MAKE SENSE to have multiple classes and multiple orders.
   It's one or the other, otherwise we need several normalizing constants.
 */
token_type_t get_token_type(token_order_t o) {
	token_type_t tt;
	tt.order = o;
	tt.mark = 0;

	if( (m_options & (1<<M_OPTION_MBOX_FORMAT)) &&
			!(m_options & (1<<M_OPTION_NGRAM_STRADDLE_NL)) ) {
		switch(mbox.state) {
		case msHEADER:
			switch(mbox.hstate) {
			case mhsTRACE:
				tt.cls = AMIN + 9;
				break;
			case mhsFROM:
				tt.cls = AMIN + 8;
				break;
			case mhsTO:
				tt.cls = AMIN + 7;
				break;
			case mhsSUBJECT:
				tt.cls = AMIN + 6;
				break;
			case mhsXHEADER:
				tt.cls = AMIN + 5;
				break;
			default:
			case mhsUNDEF:
				tt.cls = AMIN + 4;
				break;
			}
			break;
			case msUNDEF:
				tt.cls = AMIN + 3;
				break;
			case msBODY:
				tt.cls = AMIN + 3;
				break;
			case msATTACH:
				tt.cls =  AMIN + 2;
				break;
			default:
				tt.cls = AMIN + 1;
		}
	} else {
		tt.cls = AMIN + 1;
	}

	return tt;
}

/***********************************************************
 * MULTIBYTE FILE HANDLING FUNCTIONS                       *
 * this is suitable for any locale whose character set     *
 * encoding doesn't include NUL bytes inside characters    *
 ***********************************************************/

/* this sets up an artificial empty line to give the
 * various filters a chance to flush their caches
 */
bool_t finalize_textbuf(int *final) {
	if( (*final)-- > 0 ) {
		strcmp(textbuf, "\r\n\r\n");
		return 1;
	}
	return 0;
}

void process_directory(char *name,
		int (*line_filter)(MBOX_State *, char *),
		void (*character_filter)(XML_State *, char *),
		void (*word_fun)(char *, token_type_t, regex_count_t),
		char *(*pre_line_fun)(char *),
		void (*post_line_fun)(char *)) {
	DIR *d;
	struct dirent *sd;
	FILE *input;
	struct stat statinfo;
	char fullp[_POSIX_PATH_MAX + 1];
	char *fp;

	d = opendir(name);
	if( d ) {
		/* directory returns relative file names, but we need full paths */
		strcpy(fullp, name);
		fp = fullp + strlen(name);
		*fp++ = '/';

		for(sd = readdir(d); sd; sd = readdir(d)) {
			strcpy(fp, sd->d_name);

			if( stat(fullp, &statinfo) == 0 ) {
				switch(statinfo.st_mode & S_IFMT) {
				case S_IFREG:
					input = fopen(fullp, "rb");
					if( input ) {
						inputfile = fullp;
						/* set some initial options */
						reset_xml_character_filter(&xml, xmlRESET);

						if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
							reset_mbox_line_filter(&mbox);
						}
						process_file(input, line_filter, character_filter,
								word_fun, pre_line_fun, post_line_fun);
						fclose(input);
					}
				default:
					/* nothing */
					break;
				}
			}
		}
		closedir(d);
	} else {
		errormsg(E_WARNING, "could not open %s, skipping\n", name);
	}
}

void reset_current_token(char *tokbuf, char **q, token_order_t *how_many) {
	tokbuf[0] = DIAMOND;
	tokbuf[1] = '\0';
	*q = tokbuf + 1;
	*how_many = 0;
}

/* reads a text file as input and applies several filters. */
void process_file(FILE *input,
		int (*line_filter)(MBOX_State *, char *),
		void (*character_filter)(XML_State *, char *),
		void (*word_fun)(char *, token_type_t, regex_count_t),
		char *(*pre_line_fun)(char *),
		void (*post_line_fun)(char *)) {
	char *pptextbuf;
	regex_count_t i;
	char tokbuf[(MAX_TOKEN_LEN+1)*MAX_SUBMATCH+EXTRA_TOKEN_LEN];
	char *q;
	token_order_t how_many;
	long int e;
	int extra_lines = 2;
	e = 0;
	/* initialize the norex state */
	reset_current_token(tokbuf, &q, &how_many);

	set_iobuf_mode(input);

	inputline = 0;

	/* extra lines are used to flush data conversion caches, but not
     needed for plain text */
	if( u_options & (1<<U_OPTION_FILTER) ) { extra_lines = 0; }

	/* now start processing */
	while( fill_textbuf(input, &extra_lines) ) {

		inputline++;
		/* preprocesses textbuf, optionally censors it */
		if( pre_line_fun ) {
			pptextbuf = (*pre_line_fun)(textbuf);
			if( !pptextbuf ) { continue; }
		} else {
			pptextbuf = textbuf;
		}

		/* next we check to see if this line should be skipped */
		if( *pptextbuf && (!line_filter || (*line_filter)(&mbox, pptextbuf)) ) {
			/* now filter some of the characters in the current line */
			if( character_filter ) { (*character_filter)(&xml, pptextbuf); }

			if( (u_options & (1<<U_OPTION_DEBUG)) &&
					(u_options & (1<<U_OPTION_CLASSIFY)) ) {
				fprintf(stdout, "%s", pptextbuf);
				/* 	if( *pptextbuf && !strchr(pptextbuf, '\n') ) { */
				/* 	  fprintf(stdout, "\n"); */
				/* 	} */
			}
			/* repeat for each regular expression:
	 	 	 find all the instances of a matching substring */
			for(i = 0; i < regex_count; i++) {
				regex_tokenizer(pptextbuf, i, word_fun, get_token_type);
			}

			/* default processing: reads tokens and passes them to
	 	 	 the word_fun */
			if( (m_options & (1<<M_OPTION_USE_STDTOK)) ) {
				std_tokenizer(pptextbuf, &q, tokbuf, &how_many, ngram_order,
						word_fun, get_token_type);
			}

		}

		/* now summarize this line if required */
		if( post_line_fun ) { (*post_line_fun)(pptextbuf); }

		if( !(m_options & (1<<M_OPTION_NGRAM_STRADDLE_NL)) ) {
			reset_current_token(tokbuf, &q, &how_many);
		}

		if( cmd & (1<<CMD_RELOAD_CATS) ) {

			reload_all_categories();

			cmd &= ~(1<<CMD_RELOAD_CATS);
		}

	}
	/* since std_tokenizer tokens can straddle lines, we should
     flush the last token fragment - note this has nothing to do with
     the M_OPTION_NGRAM_STRADDLE_NL flag, it's an issue caused by caching
     decoders such as the base64 and qp line filters. */
	if( (m_options & (1<<M_OPTION_USE_STDTOK)) ) {
		std_tokenizer(NULL, &q, tokbuf, &how_many, ngram_order,
				word_fun, get_token_type);
		if( post_line_fun ) { (*post_line_fun)(NULL); }
	}
}


/* reads a text file as input and applies several filters. */
void nvram_process_file(FILE *input,
		int (*line_filter)(MBOX_State *, char *),
		void (*character_filter)(XML_State *, char *),
		void (*word_fun)(char *, token_type_t, regex_count_t),
		char *(*pre_line_fun)(char *),
		void (*post_line_fun)(char *), size_t datasize, char *input_map) {
	char *pptextbuf;
	regex_count_t i;
	char tokbuf[(MAX_TOKEN_LEN+1)*MAX_SUBMATCH+EXTRA_TOKEN_LEN];
	char *q;
	token_order_t how_many;
	long int e;
	int extra_lines = 2;
	e = 0;
	size_t mapped_bytes =0;
	char *map_addr = NULL;

#ifdef DEBUG
	LOG(stderr,"nvram_process_file: Entering  \n");
#endif
	if(!input_map) {
		LOG(stderr,"SCOLDING: nvram_process_file: input map is null \n");
		return;
	}

	map_addr = input_map;

	/* initialize the norex state */
	//reset_current_token(tokbuf, &q, &how_many);
	//set_iobuf_mode(input);
	inputline = 0;

	/* extra lines are used to flush data conversion caches, but not
     needed for plain text */
	if( u_options & (1<<U_OPTION_FILTER) ) { extra_lines = 0; }

	/* now start processing */
	//replacing fill_textbuf with nvram_fill_textbuf
	while( nvram_fill_textbuf(input, &extra_lines,datasize,&mapped_bytes, map_addr ) ) {

		inputline++;
		/* preprocesses textbuf, optionally censors it */
		if( pre_line_fun ) {
			pptextbuf = (*pre_line_fun)(textbuf);
			if( !pptextbuf ) { continue; }
		} else {
			pptextbuf = textbuf;
		}

		/* next we check to see if this line should be skipped */
		if( *pptextbuf && (!line_filter || (*line_filter)(&mbox, pptextbuf)) ) {
			/* now filter some of the characters in the current line */
			if( character_filter ) { (*character_filter)(&xml, pptextbuf); }

			if( (u_options & (1<<U_OPTION_DEBUG)) &&
					(u_options & (1<<U_OPTION_CLASSIFY)) ) {
				fprintf(stdout, "%s", pptextbuf);
				/* 	if( *pptextbuf && !strchr(pptextbuf, '\n') ) { */
				/* 	  fprintf(stdout, "\n"); */
				/* 	} */
			}
			/* repeat for each regular expression:
	 	 	 find all the instances of a matching substring */
			for(i = 0; i < regex_count; i++) {
				regex_tokenizer(pptextbuf, i, word_fun, get_token_type);
			}

			/* default processing: reads tokens and passes them to
	 	 	 the word_fun */
			if( (m_options & (1<<M_OPTION_USE_STDTOK)) ) {
				std_tokenizer(pptextbuf, &q, tokbuf, &how_many, ngram_order,
						word_fun, get_token_type);
			}

		}

		/* now summarize this line if required */
		if( post_line_fun ) { (*post_line_fun)(pptextbuf); }

		if( !(m_options & (1<<M_OPTION_NGRAM_STRADDLE_NL)) ) {
			reset_current_token(tokbuf, &q, &how_many);
		}

		if( cmd & (1<<CMD_RELOAD_CATS) ) {

			reload_all_categories();

			cmd &= ~(1<<CMD_RELOAD_CATS);
		}

	}
	/* since std_tokenizer tokens can straddle lines, we should
     flush the last token fragment - note this has nothing to do with
     the M_OPTION_NGRAM_STRADDLE_NL flag, it's an issue caused by caching
     decoders such as the base64 and qp line filters. */
	if( (m_options & (1<<M_OPTION_USE_STDTOK)) ) {
		std_tokenizer(NULL, &q, tokbuf, &how_many, ngram_order,
				word_fun, get_token_type);
		if( post_line_fun ) { (*post_line_fun)(NULL); }
	}
}


/***********************************************************
 * WIDE CHARACTER FILE HANDLING FUNCTIONS                  *
 * this is needed for any locale whose character set       *
 * encoding can include NUL bytes inside characters        *
 ***********************************************************/
#if defined HAVE_MBRTOWC

void w_process_directory(char *name,
		int (*w_line_filter)(MBOX_State *, wchar_t *),
		void (*w_character_filter)(XML_State *, wchar_t *),
		void (*word_fun)(char *, token_type_t, regex_count_t),
		char *(*pre_line_fun)(char *),
		void (*post_line_fun)(char *)) {
	DIR *d;
	struct dirent *sd;
	FILE *input;
	struct stat statinfo;
	char fullp[_POSIX_PATH_MAX + 1];
	char *fp;

	d = opendir(name);
	if( d ) {
		/* directory returns relative file names, but we need full paths */
		strcpy(fullp, name);
		fp = fullp + strlen(name);
		*fp++ = '/';

		for(sd = readdir(d); sd; sd = readdir(d)) {
			strcpy(fp, sd->d_name);

			if( stat(fullp, &statinfo) == 0 ) {
				switch(statinfo.st_mode & S_IFMT) {
				case S_IFREG:
					input = fopen(fullp, "rb");
					if( input ) {
						inputfile = fullp;
						/* set some initial options */
						reset_xml_character_filter(&xml, xmlRESET);

						if( m_options & (1<<M_OPTION_MBOX_FORMAT) ) {
							reset_mbox_line_filter(&mbox);
						}
						w_process_file(input, w_line_filter, w_character_filter,
								word_fun, pre_line_fun, post_line_fun);
						fclose(input);
					}
				default:
					/* nothing */
					break;
				}
			}
		}
		closedir(d);
	} else {
		errormsg(E_WARNING, "could not open %s, skipping\n", name);
	}
}

/* reads a text file as input, converting each line
into a wide character representation and applies several
filters. */
void w_process_file(FILE *input,
		int (*line_filter)(MBOX_State *,wchar_t *),
		void (*character_filter)(XML_State *,wchar_t *),
		void (*word_fun)(char *, token_type_t, regex_count_t),
		char *(*pre_line_fun)(char *),
		void (*post_line_fun)(char *)) {
	char *pptextbuf;
	regex_count_t i;
	mbstate_t input_shiftstate;
	char *q;
	char tokbuf[(MAX_TOKEN_LEN+1)*MAX_SUBMATCH+EXTRA_TOKEN_LEN];
	token_order_t how_many;
	int extra_lines = 2;
	wchar_t *wcp;
	char wcq[MB_LEN_MAX+1];

	set_iobuf_mode(input);

	/* initialize the norex state */
	reset_current_token(tokbuf, &q, &how_many);

	memset(&input_shiftstate, 0, sizeof(mbstate_t));
	inputline = 0;
	/* extra lines are used to flush data conversion caches, but not
     needed for plain text */
	if( u_options & (1<<U_OPTION_FILTER) ) { extra_lines = 0; }

	while( fill_textbuf(input, &extra_lines) ) {
		inputline++;
		/* preprocesses textbuf, optionally censors it */
		if( pre_line_fun ) {
			pptextbuf = (*pre_line_fun)(textbuf);
			if( !pptextbuf ) { continue; }
		} else {
			pptextbuf = textbuf;
		}

		/* next we check to see if this line should be skipped */
		if( fill_wc_textbuf(pptextbuf, &input_shiftstate) &&
				(!line_filter || (*line_filter)(&mbox, wc_textbuf)) ) {
			/* now filter some of the characters in the current line */
			if( character_filter ) { (*character_filter)(&xml, wc_textbuf); }

			if( (u_options & (1<<U_OPTION_DEBUG)) &&
					(u_options & (1<<U_OPTION_CLASSIFY)) ) {
				for(wcp = wc_textbuf; *wcp; wcp++) {
					*(wcq + wctomb(wcq, *wcp)) = '\0';
					fprintf(stdout, "%s", wcq);
				}
				/* 	if( *wc_textbuf && !wcschr(wc_textbuf, L'\n') ) { */
				/* 	  fprintf(stdout, "\n"); */
				/* 	} */
			}

			/* repeat for each regular expression:
			 find all the instances of a matching substring */
			for(i = 0; i < regex_count; i++) {
				w_regex_tokenizer(wc_textbuf, i, word_fun, get_token_type);
			}

			/* default processing: reads tokens and passes them to
			 the word_fun */
			if( (m_options & (1<<M_OPTION_USE_STDTOK)) ) {
				w_std_tokenizer(wc_textbuf, &q, tokbuf, &how_many, ngram_order,
						word_fun, get_token_type);
			}

		}

		/* now summarize this line if required */
		if( post_line_fun ) { (*post_line_fun)(pptextbuf); }

		if( !(m_options & (1<<M_OPTION_NGRAM_STRADDLE_NL)) ) {
			reset_current_token(tokbuf, &q, &how_many);
		}

		if( cmd & (1<<CMD_RELOAD_CATS) ) {

			reload_all_categories();

			cmd &= ~(1<<CMD_RELOAD_CATS);
		}

	}
	/* since w_std_tokenizer tokens can straddle lines, we should
     flush the last token fragment */
	if( (m_options & (1<<M_OPTION_USE_STDTOK)) ) {
		w_std_tokenizer(NULL, &q, tokbuf, &how_many, ngram_order,
				word_fun, get_token_type);
		if( post_line_fun ) { (*post_line_fun)(NULL); }
	}

}

#endif /* DISABLE_WCHAR */

