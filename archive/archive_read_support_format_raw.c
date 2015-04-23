/*-
 * Copyright (c) 2003-2009 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_support_format_raw.c 201107 2009-12-28 03:25:33Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"

struct raw_info {
	int64_t offset; /* Current position in the file. */
	int64_t unconsumed;
	int     end_of_file;
};

static int	tk_archive_read_format_raw_bid(struct tk_archive_read *, int);
static int	tk_archive_read_format_raw_cleanup(struct tk_archive_read *);
static int	tk_archive_read_format_raw_read_data(struct tk_archive_read *,
		    const void **, size_t *, int64_t *);
static int	tk_archive_read_format_raw_read_data_skip(struct tk_archive_read *);
static int	tk_archive_read_format_raw_read_header(struct tk_archive_read *,
		    struct tk_archive_entry *);

int
tk_archive_read_support_format_raw(struct archive *_a)
{
	struct raw_info *info;
	struct tk_archive_read *a = (struct tk_archive_read *)_a;
	int r;

	tk_archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "tk_archive_read_support_format_raw");

	info = (struct raw_info *)calloc(1, sizeof(*info));
	if (info == NULL) {
		tk_archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate raw_info data");
		return (ARCHIVE_FATAL);
	}

	r = __tk_archive_read_register_format(a,
	    info,
	    "raw",
	    tk_archive_read_format_raw_bid,
	    NULL,
	    tk_archive_read_format_raw_read_header,
	    tk_archive_read_format_raw_read_data,
	    tk_archive_read_format_raw_read_data_skip,
	    NULL,
	    tk_archive_read_format_raw_cleanup);
	if (r != ARCHIVE_OK)
		free(info);
	return (r);
}

/*
 * Bid 1 if this is a non-empty file.  Anyone who can really support
 * this should outbid us, so it should generally be safe to use "raw"
 * in conjunction with other formats.  But, this could really confuse
 * folks if there are bid errors or minor file damage, so we don't
 * include "raw" as part of support_format_all().
 */
static int
tk_archive_read_format_raw_bid(struct tk_archive_read *a, int best_bid)
{
	if (best_bid < 1 && __tk_archive_read_ahead(a, 1, NULL) != NULL)
		return (1);
	return (-1);
}

/*
 * Mock up a fake header.
 */
static int
tk_archive_read_format_raw_read_header(struct tk_archive_read *a,
    struct tk_archive_entry *entry)
{
	struct raw_info *info;

	info = (struct raw_info *)(a->format->data);
	if (info->end_of_file)
		return (ARCHIVE_EOF);

	a->archive.tk_archive_format = ARCHIVE_FORMAT_RAW;
	a->archive.tk_archive_format_name = "raw";
	tk_archive_entry_set_pathname(entry, "data");
	tk_archive_entry_set_filetype(entry, AE_IFREG);
	tk_archive_entry_set_perm(entry, 0644);
	/* I'm deliberately leaving most fields unset here. */
	return (ARCHIVE_OK);
}

static int
tk_archive_read_format_raw_read_data(struct tk_archive_read *a,
    const void **buff, size_t *size, int64_t *offset)
{
	struct raw_info *info;
	ssize_t avail;

	info = (struct raw_info *)(a->format->data);

	/* Consume the bytes we read last time. */
	if (info->unconsumed) {
		__tk_archive_read_consume(a, info->unconsumed);
		info->unconsumed = 0;
	}

	if (info->end_of_file)
		return (ARCHIVE_EOF);

	/* Get whatever bytes are immediately available. */
	*buff = __tk_archive_read_ahead(a, 1, &avail);
	if (avail > 0) {
		/* Return the bytes we just read */
		*size = avail;
		*offset = info->offset;
		info->offset += *size;
		info->unconsumed = avail;
		return (ARCHIVE_OK);
	} else if (0 == avail) {
		/* Record and return end-of-file. */
		info->end_of_file = 1;
		*size = 0;
		*offset = info->offset;
		return (ARCHIVE_EOF);
	} else {
		/* Record and return an error. */
		*size = 0;
		*offset = info->offset;
		return ((int)avail);
	}
}

static int
tk_archive_read_format_raw_read_data_skip(struct tk_archive_read *a)
{
	struct raw_info *info = (struct raw_info *)(a->format->data);

	/* Consume the bytes we read last time. */
	if (info->unconsumed) {
		__tk_archive_read_consume(a, info->unconsumed);
		info->unconsumed = 0;
	}
	info->end_of_file = 1;
	return (ARCHIVE_OK);
}

static int
tk_archive_read_format_raw_cleanup(struct tk_archive_read *a)
{
	struct raw_info *info;

	info = (struct raw_info *)(a->format->data);
	free(info);
	a->format->data = NULL;
	return (ARCHIVE_OK);
}
