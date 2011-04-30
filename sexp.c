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
#include <string.h>
#include <stdarg.h>

typedef int (*handle_atom_cb)(void *user_data, const char *atom, int len, int depth);

/**
 * \brief Determine whether a character is valid in an unquoted atom
 * \param c character to test
 * \return true if the character is valid in an unquoted atom, false otherwise
 */
static int is_valid_atom(char c) {
	return (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') ||
	       c == '+' || c == '-' ||
	       c == '*' || c == '/';
}

/**
 * \brief Determine whether the given atom is valid unquoted
 * \param atom atom to test
 * \return true if the atom needs quoting, false otherwise
 */
static int str_is_valid_atom(const char *atom) {
	while(*atom) {
		if(!is_valid_atom(*atom))
			return 0;
		++atom;
	}
	return 1;
}

/**
 * \brief Determine whether a character is whitespace
 * \return true if the character is whitespace, false otherwise
 */
static int is_whitespace(char c) {
	return c == ' ' || c == '\n' || c == '\t';
}

/**
 * \brief Update current location in S-expression
 */
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

/**
 * \brief Un-escape atom before passing it to the handle_atom callback
 * \param atom the atom to unescape
 * \param end pointer to the end of the atom
 * \param depth current depth in S-expression
 * \param user_data data supplied in the call to sexp_parse
 * \param handle_atom callback to call with unescaped atom
 */
static int handle_escaped_atom(const char *atom, const char *end, int depth,
                               handle_atom_cb handle_atom, void *user_data) {
	char buffer[end-atom], *opos = buffer;
	while(atom < end) {
		if(*atom == '\\') {
			++atom;
			switch(*atom) {
			case 'n':
				*opos++ = '\n';
				break;
			case 'r':
				*opos++ = '\r';
				break;
			case 't':
				*opos++ = '\t';
				break;
			default:
				*opos++ = *atom;
			}
			++atom;
		} else {
			*opos++ = *atom++;
		}
	}
	return handle_atom(user_data, buffer, opos-buffer, depth);
}

#define HANDLE_BEGIN_LIST() do {                                           \
	if(callbacks && callbacks->begin_list &&                               \
	   callbacks->begin_list(user_data, l, p-l, depth-1))                  \
		goto done;                                                         \
} while(0)

#define HANDLE_END_LIST() do {                                             \
	if(callbacks && callbacks->end_list &&                                 \
	   callbacks->end_list(user_data, depth))                              \
		goto done;                                                         \
} while(0)

/**
 * \brief S-expression parser state
 */
typedef enum {
	/**
	 * \brief Inside a list and past the first atom
	 *
	 * Next valid states: SEXP_LIST_START, SEXP_ATOM, SEXP_QUOTED_ATOM
	 */
	SEXP_LIST,
	/**
	 * \brief Inside a list before the first atom
	 *
	 * Next valid states: SEXP_ATOM
	 */
	SEXP_LIST_START,
	/**
	 * \brief Inside an atom
	 *
	 * Next valid states: SEXP_LIST
	 */
	SEXP_ATOM,
	/**
	 * \brief Inside a quoted atom
	 *
	 * Next valid states: SEXP_ESCAPED_CHAR, SEXP_LIST
	 */
	SEXP_QUOTED_ATOM,
	/**
	 * \brief Inside an escape sequence
	 *
	 * Next valid states: SEXP_QUOTED_ATOM
	 */
	SEXP_ESCAPED_CHAR
} state_t;

int sexp_parse(const char *sexp, struct sexp_callbacks *callbacks, void *user_data) {
	const char *p = sexp; /* Current position in S-expression */
	const char *l = sexp; /* Beginning of the last interesting token */
	state_t state = SEXP_LIST; /* Current parsing state */
	int line = 1; /* Current line in input */
	int column = 1; /* Current column in input */
	int depth = 0; /* Current number of containing lists */
	int first_atom = 0; /* Nonzero when reading the first atom in a list */
	int escaped_atom = 0; /* Nonzero when reading an atom with escaped chars */
	while(*p) {
		update_position(&line, &column, *p);
		switch(state) {
		case SEXP_LIST:
			if(*p == '(') { /* Entering a list */
				++depth;
				state = SEXP_LIST_START;
			} else if(*p == ')') { /* Ending a list */
				if(--depth < 0)
					goto parse_error;
				HANDLE_END_LIST();
			} else if(depth && *p == '"') { /* Entering a quoted atom */
				state = SEXP_QUOTED_ATOM;
				l = p+1;
			} else if(depth && is_valid_atom(*p)) { /* Entering an atom */
				state = SEXP_ATOM;
				l = p;
			} else if(!is_whitespace(*p)) {
				goto parse_error;
			}
			break;
		case SEXP_LIST_START:
			if(*p == ')') { /* Ending a list */
				if(--depth < 0)
					goto parse_error;
				HANDLE_END_LIST();
				state = SEXP_LIST;
			} else if(is_valid_atom(*p)) { /* Entering an atom */
				state = SEXP_ATOM;
				first_atom = 1;
				l = p;
			} else if(!is_whitespace(*p)) {
				goto parse_error;
			}
			break;
		case SEXP_ATOM:
			if(!is_valid_atom(*p)) { /* Ending an atom */
				if(*p == ')') { /* Also ending a list */
					if(--depth < 0)
						goto parse_error;
					HANDLE_END_LIST();
				} else if(!is_whitespace(*p)) {
					goto parse_error;
				}
				if(first_atom) { /* Have first atom of list, so notify caller */
					HANDLE_BEGIN_LIST();
				} else {
					/* Notify caller of additional atom in list */
					if(callbacks && callbacks->handle_atom &&
					   callbacks->handle_atom(user_data, l, p-l, depth))
						goto done;
				}
				first_atom = 0;
				state = SEXP_LIST;
			}
			break;
		case SEXP_QUOTED_ATOM:
			if(*p == '\\') { /* Escape the next character */
				escaped_atom = 1;
				state = SEXP_ESCAPED_CHAR;
			} else if(*p == '"') { /* Ending quoted atom */
				/* Notify caller about this atom */
				if(callbacks && callbacks->handle_atom) {
					if(escaped_atom) { /* Atom needs to be escaped */
						if(handle_escaped_atom(l, p, depth,
						                       callbacks->handle_atom,
						                       user_data))
							goto done;
					} else {
						if(callbacks->handle_atom(user_data, l, p-l, depth))
							goto done;
					}
				}
				state = SEXP_LIST;
				escaped_atom = 0;
				break;
			}
			break;
		case SEXP_ESCAPED_CHAR:
			state = SEXP_QUOTED_ATOM;
			break;
		}
		++p;
	}
	if(depth != 0) { /* S-expression didn't close all lists */
		goto parse_error;
	} else if(state != SEXP_LIST) {
		/* Parser hit end of S-expression in invalid state */
		p = l;
		goto parse_error;
	}
done:
	return 0;

parse_error:
	/* Notify caller of a parse error */
	if(callbacks && callbacks->handle_error)
		callbacks->handle_error(user_data, line, column, *p);
	return -1;
}

void sexp_writer_init(struct sexp_writer *writer, sexp_writer_cb do_write, void *user_data) {
	writer->depth = 0;
	writer->error = 0;
	writer->do_write = do_write;
	writer->user_data = user_data;
}

/**
 * \brief Write whitespace before new list for readability
 * \param writer the S-expression writer to use
 * \return zero if write was successful, nonzero otherwise
 */
static int indent(struct sexp_writer *writer) {
	if(writer->depth) {
		/* Write newline, then indent to current depth */
		int buf_len = writer->depth+1;
		char buffer[buf_len];
		buffer[0] = '\n';
		for(int i=1;i<buf_len;++i)
			buffer[i] = '\t';
		return writer->do_write(writer->user_data, buffer, buf_len);
	}
	return 0;
}

int sexp_writer_start_list(struct sexp_writer *writer, const char *name) {
	if(!writer->error) {
		/* Prepare to write '(name' */
		int buf_len = strlen(name)+2, i;
		char buffer[buf_len];
		buffer[0] = '(';
		strcpy(buffer+1, name);
		if(!str_is_valid_atom(name)) {
			writer->error = 1;
			return -1;
		}
		/* Add whitespace for pretty printing */
		indent(writer);
		/* Now actually write the string we built before */
		if(writer->do_write(writer->user_data, buffer, buf_len-1))
			return -1;
		++writer->depth;
		return 0;
	}
	return -1;
}

int sexp_writer_write_atom(struct sexp_writer *writer, const char *atom) {
	if(!writer->error) {
		if(writer->depth) {
			if(str_is_valid_atom(atom)) { /* Atom doesn't need quoting */
				/* Prepend a space to atom */
				int buf_len = strlen(atom)+2;
				char buffer[buf_len];
				buffer[0] = ' ';
				strcpy(buffer+1, atom);
				if(writer->do_write(writer->user_data, buffer, buf_len-1))
					return -1;
				return 0;
			} else {
				/* Need to quote/escape atom before writing it */
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
			*opos++ = ' '; /* Write a space before the atom */
			*opos++ = '"'; /* Open quotes */
			while(*atom) {
				/* Escape any special chars */
				switch(*atom) {
					case '\r':
						*opos++ = '\\';
						*opos++ = 'r';
						++atom;
						break;
					case '\n':
						*opos++ = '\\';
						*opos++ = 'n';
						++atom;
						break;
					case '\t':
						*opos++ = '\\';
						*opos++ = 't';
						++atom;
						break;
					case '\\':
					case '"':
						*opos++ = '\\';
					default:
						*opos++ = *atom++;
				}
			}
			*opos++ = '"'; /* Close quotes */
			if(writer->do_write(writer->user_data, buffer, opos-buffer))
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
			/* Only need to write a close paren */
			char c = ')';
			if(writer->do_write(writer->user_data, &c, 1))
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
		/* First start the list */
		if(sexp_writer_start_list(writer, name))
			return -1;
		/* Write any additional atoms */
		while((atom = va_arg(ap, const char *)) != NULL) {
			if(sexp_writer_write_atom(writer, atom))
				return -1;
		}
		/* End the list */
		if(sexp_writer_end_list(writer))
			return -1;
		return 0;
	}
	return -1;
}
