/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "archive.h"
#include "archive_write_private.h"

struct write_grzip {
	struct tk_archive_write_program_data *pdata;
};

static int tk_archive_write_grzip_open(struct tk_archive_write_filter *);
static int tk_archive_write_grzip_options(struct tk_archive_write_filter *,
		    const char *, const char *);
static int tk_archive_write_grzip_write(struct tk_archive_write_filter *,
		    const void *, size_t);
static int tk_archive_write_grzip_close(struct tk_archive_write_filter *);
static int tk_archive_write_grzip_free(struct tk_archive_write_filter *);

int
tk_archive_write_add_filter_grzip(struct archive *_a)
{
	struct tk_archive_write_filter *f = __tk_archive_write_allocate_filter(_a);
	struct write_grzip *data;

	tk_archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "tk_archive_write_add_filter_grzip");

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		tk_archive_set_error(_a, ENOMEM, "Can't allocate memory");
		return (ARCHIVE_FATAL);
	}
	data->pdata = __tk_archive_write_program_allocate();
	if (data->pdata == NULL) {
		free(data);
		tk_archive_set_error(_a, ENOMEM, "Can't allocate memory");
		return (ARCHIVE_FATAL);
	}

	f->name = "grzip";
	f->code = ARCHIVE_FILTER_GRZIP;
	f->data = data;
	f->open = tk_archive_write_grzip_open;
	f->options = tk_archive_write_grzip_options;
	f->write = tk_archive_write_grzip_write;
	f->close = tk_archive_write_grzip_close;
	f->free = tk_archive_write_grzip_free;

	/* Note: This filter always uses an external program, so we
	 * return "warn" to inform of the fact. */
	tk_archive_set_error(_a, ARCHIVE_ERRNO_MISC,
	    "Using external grzip program for grzip compression");
	return (ARCHIVE_WARN);
}

static int
tk_archive_write_grzip_options(struct tk_archive_write_filter *f, const char *key,
    const char *value)
{
	(void)f; /* UNUSED */
	(void)key; /* UNUSED */
	(void)value; /* UNUSED */
	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

static int
tk_archive_write_grzip_open(struct tk_archive_write_filter *f)
{
	struct write_grzip *data = (struct write_grzip *)f->data;

	return __tk_archive_write_program_open(f, data->pdata, "grzip");
}

static int
tk_archive_write_grzip_write(struct tk_archive_write_filter *f,
    const void *buff, size_t length)
{
	struct write_grzip *data = (struct write_grzip *)f->data;

	return __tk_archive_write_program_write(f, data->pdata, buff, length);
}

static int
tk_archive_write_grzip_close(struct tk_archive_write_filter *f)
{
	struct write_grzip *data = (struct write_grzip *)f->data;

	return __tk_archive_write_program_close(f, data->pdata);
}

static int
tk_archive_write_grzip_free(struct tk_archive_write_filter *f)
{
	struct write_grzip *data = (struct write_grzip *)f->data;

	__tk_archive_write_program_free(data->pdata);
	free(data);
	return (ARCHIVE_OK);
}
