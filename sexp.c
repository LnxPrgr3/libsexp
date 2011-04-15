/*
 * Copyright (c) 2011 Jeremy Pepper
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of libsexp nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "sexp.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef int (*handle_atom_cb)(const char *atom, int len, int depth);

static int is_valid_atom(char c) {
	return (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') ||
	       c == '+' || c == '-' ||
	       c == '*' || c == '/';
}

static int str_is_valid_atom(const char *atom) {
	while(*atom) {
		if(!is_valid_atom(*atom))
			return 0;
		++atom;
	}
	return 1;
}

static int is_whitespace(char c) {
	return c == ' ' || c == '\n' || c == '\t';
}

static void update_position(int *line, int *column, char c) {
	switch(c) {
	case '\n':
		++*line;
		*column = 1;
		break;
	case '\t':
		*column += 8;
	default:
		++*column;
	}
}

static int handle_escaped_atom(const char *atom, const char *end, int depth,
                               handle_atom_cb handle_atom) {
	char buffer[end-atom], *opos = buffer;
	while(atom < end) {
		if(*atom == '\\')
			++atom;
		*opos++ = *atom++;
	}
	return handle_atom(buffer, opos-buffer, depth);
}

#define HANDLE_BEGIN_LIST() do {                                           \
	if(callbacks && callbacks->begin_list &&                               \
	   callbacks->begin_list(l, p-l, depth-1))                             \
		goto done;                                                         \
} while(0)

#define HANDLE_END_LIST() do {                                             \
	if(callbacks && callbacks->end_list && callbacks->end_list(depth))     \
		goto done;                                                         \
} while(0)

int sexp_parse(const char *sexp, struct sexp_callbacks *callbacks) {
	const char *p = sexp, *l = sexp;
	enum {SEXP_LIST, SEXP_LIST_START, SEXP_ATOM, SEXP_QUOTED_ATOM, SEXP_ESCAPED_CHAR, SEXP_POST_ATOM} state = SEXP_LIST;
	int line = 1, column = 1;
	int depth = 0;
	int first_atom = 0;
	int escaped_atom = 0;
	while(*p) {
		update_position(&line, &column, *p);
		switch(state) {
		case SEXP_LIST:
			if(*p == '(') {
				++depth;
				state = SEXP_LIST_START;
			} else if(*p == ')') {
				if(--depth < 0)
					goto parse_error;
				HANDLE_END_LIST();
			} else if(depth && *p == '"') {
				state = SEXP_QUOTED_ATOM;
				l = p+1;
			} else if(depth && is_valid_atom(*p)) {
				state = SEXP_ATOM;
				l = p;
			} else if(!is_whitespace(*p)) {
				goto parse_error;
			}
			break;
		case SEXP_LIST_START:
			if(*p == ')') {
				if(--depth < 0)
					goto parse_error;
				HANDLE_END_LIST();
			} else if(is_valid_atom(*p)) {
				state = SEXP_ATOM;
				first_atom = 1;
				l = p;
			} else if(!is_whitespace(*p)) {
				goto parse_error;
			}
			break;
		case SEXP_ATOM:
			if(!is_valid_atom(*p)) {
				if(*p == ')') {
					if(--depth < 0)
						goto parse_error;
					HANDLE_END_LIST();
				} else if(!is_whitespace(*p)) {
					goto parse_error;
				}
				if(first_atom) {
					HANDLE_BEGIN_LIST();
				} else {
					if(callbacks && callbacks->handle_atom &&
					   callbacks->handle_atom(l, p-l, depth))
						goto done;
				}
				first_atom = 0;
				state = SEXP_LIST;
			}
			break;
		case SEXP_QUOTED_ATOM:
			if(*p == '\\') {
				escaped_atom = 1;
				state = SEXP_ESCAPED_CHAR;
			} else if(*p == '"') {
				if(callbacks && callbacks->handle_atom) {
					if(escaped_atom) {
						if(handle_escaped_atom(l, p, depth,
						                       callbacks->handle_atom))
							goto done;
					} else {
						if(callbacks->handle_atom(l, p-l, depth))
							goto done;
					}
				}
				state = SEXP_POST_ATOM;
				escaped_atom = 0;
				break;
			}
			break;
		case SEXP_ESCAPED_CHAR:
			state = SEXP_QUOTED_ATOM;
			break;
		case SEXP_POST_ATOM:
			if(*p == ')') {
				if(--depth < 0)
					goto parse_error;
				HANDLE_END_LIST();
			} else if(!is_whitespace(*p)) {
				goto parse_error;
			} else {
				state = SEXP_LIST;
			}
			break;
		}
		++p;
	}
	if(depth != 0) {
		goto parse_error;
	} else if(state != SEXP_LIST && state != SEXP_POST_ATOM) {
		p = l;
		goto parse_error;
	}
done:
	return 0;

parse_error:
	if(callbacks && callbacks->handle_error)
		callbacks->handle_error(line, column, *p);
	return -1;
}

void sexp_writer_init(struct sexp_writer *writer, sexp_writer_cb do_write) {
	writer->depth = 0;
	writer->error = 0;
	writer->do_write = do_write; 
}

static int indent(struct sexp_writer *writer) {
	if(writer->depth) {
		int buf_len = writer->depth+1;
		char buffer[buf_len];
		buffer[0] = '\n';
		for(int i=1;i<buf_len;++i)
			buffer[i] = '\t';
		return writer->do_write(buffer, buf_len);
	}
	return 0;
}

int sexp_writer_start_list(struct sexp_writer *writer, const char *name) {
	if(!writer->error) {
		int buf_len = strlen(name)+2, i;
		char buffer[buf_len];
		buffer[0] = '(';
		strcpy(buffer+1, name);
		if(!str_is_valid_atom(name)) {
			writer->error = 1;
			return -1;
		}
		indent(writer);
		if(writer->do_write(buffer, buf_len-1))
			return -1;
		++writer->depth;
		return 0;
	}
	return -1;
}

int sexp_writer_write_atom(struct sexp_writer *writer, const char *atom) {
	if(!writer->error) {
		if(writer->depth) {
			if(str_is_valid_atom(atom)) {
				int buf_len = strlen(atom)+2;
				char buffer[buf_len];
				buffer[0] = ' ';
				strcpy(buffer+1, atom);
				if(writer->do_write(buffer, buf_len-1))
					return -1;
				return 0;
			} else {
				return sexp_writer_write_quoted_atom(writer, atom);
			}
		}
		writer->error = 1;
	}
	return -1;
}

int sexp_writer_write_quoted_atom(struct sexp_writer *writer, const char *atom) {
	if(!writer->error) {
		if(writer->depth) {
			int buf_len = strlen(atom)*2+3;
			char buffer[buf_len], *opos = buffer;
			*opos++ = ' ';
			*opos++ = '"';
			while(*atom) {
				if(*atom == '\\' || *atom == '"') {
					*opos++ = '\\';
				}
				*opos++ = *atom++;
			}
			*opos++ = '"';
			if(writer->do_write(buffer, opos-buffer))
				return -1;
			return 0;
		}
		writer->error = 1;
	}
	return -1;
}

int sexp_writer_end_list(struct sexp_writer *writer) {
	if(!writer->error) {
		if(writer->depth) {
			char c = ')';
			if(writer->do_write(&c, 1))
				return -1;
			--writer->depth;
			return 0;
		}
		writer->error = 1;
	}
	return -1;
}

int sexp_writer_write_list(struct sexp_writer *writer, const char *name, ...) {
	if(!writer->error) {
		va_list ap;
		const char *atom;
		va_start(ap, name);
		if(sexp_writer_start_list(writer, name))
			return -1;
		while((atom = va_arg(ap, const char *)) != NULL) {
			if(sexp_writer_write_atom(writer, atom))
				return -1;
		}
		if(sexp_writer_end_list(writer))
			return -1;
		return 0;
	}
	return -1;
}
