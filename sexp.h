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

#ifndef SEXP_H
#define SEXP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief List of callbacks to handle parsing events
 */
struct sexp_callbacks {
	/**
	 * \brief Function called for each new list
	 * \param name the first atom in the list
	 * \param len the length of the first atom
	 * \param depth the nesting depth at which this list was encountered
	 * \return 0 to continue parsing or nonzero to stop parsing
	 */
	int (*begin_list)(const char *name, int len, int depth);
	/**
	 * \brief Function called for each atom after the first in a list
	 * \param atom the contents of the atom
	 * \param len the length of the atom
	 * \param depth the nesting depth at which this atom was encountered
	 * \return 0 to continue parsing or nonzero to stop parsing
	 */
	int (*handle_atom)(const char *atom, int len, int depth);
	/**
	 * \brief Function called at the end of each list
	 * \param depth the nesting depth at which this list was encountered
	 * \return 0 to continue parsing or nonzero to stop parsing
	 */
	int (*end_list)(int depth);
	/**
	 * \brief Function called when a parsing error occurs
	 * \param line the line number on which the error occured
	 * \param column the column on which the error occured
	 * \param c the character that triggered the error
	 */
	void (*handle_error)(int line, int column, char c);
};

/**
 * \brief Function called to pass rendered data back to the caller
 * \param data the character data to write
 * \param len the number of characters of data to write
 * \return 0 if the write was successful, nonzero otherwise
 */
typedef int (*sexp_writer_cb)(const char *data, int len);

/**
 * \brief S-expression writer state
 */
struct sexp_writer {
	int depth;
	int error;
	sexp_writer_cb do_write;
};

/**
 * \brief Parse the given S-expression
 * \param sexp the S-expression to parse
 * \param callbacks callbacks to call for parsing events
 * \return 0 if parsing was successful, nonzero otherwise
 */
int sexp_parse(const char *sexp, struct sexp_callbacks *callbacks);
/**
 * \brief Initialize an S-expression writer structure
 * \param writer the S-expression writer to initialize
 * \param do_write callback to call with rendered character data
 */
void sexp_writer_init(struct sexp_writer *writer, sexp_writer_cb do_write);
/**
 * \brief Begin writing a new list
 * \param writer the S-expression writer to use
 * \param name the atom naming this list
 * \return 0 if the write was successful, nonzero otherwise
 */
int sexp_writer_start_list(struct sexp_writer *writer, const char *name);
/**
 * \brief Write an atom
 * \param writer the S-expression writer to use
 * \param atom the atom to write
 * \return 0 if the write was successful, nonzero otherwise
 */
int sexp_writer_write_atom(struct sexp_writer *writer, const char *atom);
/**
 * \brief Write a quoted atom
 * \param writer the S-expression writer to use
 * \param atom the atom to write
 * \return 0 if the write was successful, nonzero otherwise
 */
int sexp_writer_write_quoted_atom(struct sexp_writer *writer, const char *atom);
/**
 * \brief Finish writing a list started with sexp_writer_start_list
 * \return 0 if the write was successful, nonzero otherwise
 */
int sexp_writer_end_list(struct sexp_writer *writer);
/**
 * \brief Write a list with the given atoms as contents
 * \param writer the S-expression writer to use
 * \param name the atom naming this list
 * \param ... the atoms to add to this list, terminated by NULL
 * \return 0 if the write was successful, nonzero otherwise
 */
int sexp_writer_write_list(struct sexp_writer *writer, const char *name, ...);

#ifdef __cplusplus
}
#endif

#endif
