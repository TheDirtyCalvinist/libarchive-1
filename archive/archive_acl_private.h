/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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
 *
 * $FreeBSD$
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifndef ARCHIVE_ACL_PRIVATE_H_INCLUDED
#define	ARCHIVE_ACL_PRIVATE_H_INCLUDED

#include "archive_string.h"

struct tk_archive_acl_entry {
	struct tk_archive_acl_entry *next;
	int	type;			/* E.g., access or default */
	int	tag;			/* E.g., user/group/other/mask */
	int	permset;		/* r/w/x bits */
	int	id;			/* uid/gid for user/group */
	struct tk_archive_mstring name;		/* uname/gname */
};

struct tk_archive_acl {
	mode_t		mode;
	struct tk_archive_acl_entry	*acl_head;
	struct tk_archive_acl_entry	*acl_p;
	int		 acl_state;	/* See acl_next for details. */
	wchar_t		*acl_text_w;
	char		*acl_text;
	int		 acl_types;
};

void tk_archive_acl_clear(struct tk_archive_acl *);
void tk_archive_acl_copy(struct tk_archive_acl *, struct tk_archive_acl *);
int tk_archive_acl_count(struct tk_archive_acl *, int);
int tk_archive_acl_reset(struct tk_archive_acl *, int);
int tk_archive_acl_next(struct archive *, struct tk_archive_acl *, int,
    int *, int *, int *, int *, const char **);

int tk_archive_acl_add_entry(struct tk_archive_acl *, int, int, int, int, const char *);
int tk_archive_acl_add_entry_w_len(struct tk_archive_acl *,
    int, int, int, int, const wchar_t *, size_t);
int tk_archive_acl_add_entry_len(struct tk_archive_acl *,
    int, int, int, int, const char *, size_t);

const wchar_t *tk_archive_acl_text_w(struct archive *, struct tk_archive_acl *, int);
int tk_archive_acl_text_l(struct tk_archive_acl *, int, const char **, size_t *,
    struct tk_archive_string_conv *);

/*
 * Private ACL parser.  This is private because it handles some
 * very weird formats that clients should not be messing with.
 * Clients should only deal with their platform-native formats.
 * Because of the need to support many formats cleanly, new arguments
 * are likely to get added on a regular basis.  Clients who try to use
 * this interface are likely to be surprised when it changes.
 */
int tk_archive_acl_parse_w(struct tk_archive_acl *,
		    const wchar_t *, int /* type */);
int tk_archive_acl_parse_l(struct tk_archive_acl *,
		    const char *, int /* type */,
		    struct tk_archive_string_conv *);

#endif /* ARCHIVE_ENTRY_PRIVATE_H_INCLUDED */
