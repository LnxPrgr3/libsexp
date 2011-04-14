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

typedef int (*handle_atom_cb)(const char *atom, int len, int depth);

static int is_valid_atom(char c) {
	return (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') ||
	       c == '+' || c == '-' ||
	       c == '*' || c == '/';
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
			} else if(*p == '"') {
				state = SEXP_QUOTED_ATOM;
				l = p+1;
			} else if(is_valid_atom(*p)) {
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
done:
	return 0;

parse_error:
	if(callbacks && callbacks->handle_error)
		callbacks->handle_error(line, column, *p);
	return -1;
}
