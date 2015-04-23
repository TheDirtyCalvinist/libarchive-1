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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_entry.c 201096 2009-12-28 02:41:27Z kientzle $");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#define HAVE_MAJOR
#elif MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#define HAVE_MAJOR
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>	/* for Linux file flags */
#endif
/*
 * Some Linux distributions have both linux/ext2_fs.h and ext2fs/ext2_fs.h.
 * As the include guards don't agree, the order of include is important.
 */
#ifdef HAVE_LINUX_EXT2_FS_H
#include <linux/ext2_fs.h>	/* for Linux file flags */
#endif
#if defined(HAVE_EXT2FS_EXT2_FS_H) && !defined(__CYGWIN__)
#include <ext2fs/ext2_fs.h>	/* for Linux file flags */
#endif
#include <stddef.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#include "archive.h"
#include "archive_acl_private.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_entry_private.h"

#if !defined(HAVE_MAJOR) && !defined(major)
/* Replacement for major/minor/makedev. */
#define	major(x) ((int)(0x00ff & ((x) >> 8)))
#define	minor(x) ((int)(0xffff00ff & (x)))
#define	makedev(maj,min) ((0xff00 & ((maj)<<8)) | (0xffff00ff & (min)))
#endif

/* Play games to come up with a suitable makedev() definition. */
#ifdef __QNXNTO__
/* QNX.  <sigh> */
#include <sys/netmgr.h>
#define ae_makedev(maj, min) makedev(ND_LOCAL_NODE, (maj), (min))
#elif defined makedev
/* There's a "makedev" macro. */
#define ae_makedev(maj, min) makedev((maj), (min))
#elif defined mkdev || ((defined _WIN32 || defined __WIN32__) && !defined(__CYGWIN__))
/* Windows. <sigh> */
#define ae_makedev(maj, min) mkdev((maj), (min))
#else
/* There's a "makedev" function. */
#define ae_makedev(maj, min) makedev((maj), (min))
#endif

/*
 * This adjustment is needed to support the following idiom for adding
 * 1000ns to the stored time:
 * archive_entry_set_atime(archive_entry_atime(),
 *                         archive_entry_atime_nsec() + 1000)
 * The additional if() here compensates for ambiguity in the C standard,
 * which permits two possible interpretations of a % b when a is negative.
 */
#define FIX_NS(t,ns) \
	do {	\
		t += ns / 1000000000; \
		ns %= 1000000000; \
		if (ns < 0) { --t; ns += 1000000000; } \
	} while (0)

static char *	 ae_fflagstostr(unsigned long bitset, unsigned long bitclear);
static const wchar_t	*ae_wcstofflags(const wchar_t *stringp,
		    unsigned long *setp, unsigned long *clrp);
static const char	*ae_strtofflags(const char *stringp,
		    unsigned long *setp, unsigned long *clrp);

#ifndef HAVE_WCSCPY
static wchar_t * wcscpy(wchar_t *s1, const wchar_t *s2)
{
	wchar_t *dest = s1;
	while ((*s1 = *s2) != L'\0')
		++s1, ++s2;
	return dest;
}
#endif
#ifndef HAVE_WCSLEN
static size_t wcslen(const wchar_t *s)
{
	const wchar_t *p = s;
	while (*p != L'\0')
		++p;
	return p - s;
}
#endif
#ifndef HAVE_WMEMCMP
/* Good enough for simple equality testing, but not for sorting. */
#define wmemcmp(a,b,i)  memcmp((a), (b), (i) * sizeof(wchar_t))
#endif

/****************************************************************************
 *
 * Public Interface
 *
 ****************************************************************************/

struct tk_archive_entry *
tk_archive_entry_clear(struct tk_archive_entry *entry)
{
	if (entry == NULL)
		return (NULL);
	tk_archive_mstring_clean(&entry->ae_fflags_text);
	tk_archive_mstring_clean(&entry->ae_gname);
	tk_archive_mstring_clean(&entry->ae_hardlink);
	tk_archive_mstring_clean(&entry->ae_pathname);
	tk_archive_mstring_clean(&entry->ae_sourcepath);
	tk_archive_mstring_clean(&entry->ae_symlink);
	tk_archive_mstring_clean(&entry->ae_uname);
	tk_archive_entry_copy_mac_metadata(entry, NULL, 0);
	tk_archive_acl_clear(&entry->acl);
	tk_archive_entry_xattr_clear(entry);
	tk_archive_entry_sparse_clear(entry);
	free(entry->stat);
	memset(entry, 0, sizeof(*entry));
	return entry;
}

struct tk_archive_entry *
tk_archive_entry_clone(struct tk_archive_entry *entry)
{
	struct tk_archive_entry *entry2;
	struct ae_xattr *xp;
	struct ae_sparse *sp;
	size_t s;
	const void *p;

	/* Allocate new structure and copy over all of the fields. */
	/* TODO: Should we copy the archive over?  Or require a new archive
	 * as an argument? */
	entry2 = tk_archive_entry_new2(entry->archive);
	if (entry2 == NULL)
		return (NULL);
	entry2->ae_stat = entry->ae_stat;
	entry2->ae_fflags_set = entry->ae_fflags_set;
	entry2->ae_fflags_clear = entry->ae_fflags_clear;

	/* TODO: XXX If clone can have a different archive, what do we do here if
	 * character sets are different? XXX */
	tk_archive_mstring_copy(&entry2->ae_fflags_text, &entry->ae_fflags_text);
	tk_archive_mstring_copy(&entry2->ae_gname, &entry->ae_gname);
	tk_archive_mstring_copy(&entry2->ae_hardlink, &entry->ae_hardlink);
	tk_archive_mstring_copy(&entry2->ae_pathname, &entry->ae_pathname);
	tk_archive_mstring_copy(&entry2->ae_sourcepath, &entry->ae_sourcepath);
	tk_archive_mstring_copy(&entry2->ae_symlink, &entry->ae_symlink);
	entry2->ae_set = entry->ae_set;
	tk_archive_mstring_copy(&entry2->ae_uname, &entry->ae_uname);

	/* Copy ACL data over. */
	tk_archive_acl_copy(&entry2->acl, &entry->acl);

	/* Copy Mac OS metadata. */
	p = tk_archive_entry_mac_metadata(entry, &s);
	tk_archive_entry_copy_mac_metadata(entry2, p, s);

	/* Copy xattr data over. */
	xp = entry->xattr_head;
	while (xp != NULL) {
		tk_archive_entry_xattr_add_entry(entry2,
		    xp->name, xp->value, xp->size);
		xp = xp->next;
	}

	/* Copy sparse data over. */
	sp = entry->sparse_head;
	while (sp != NULL) {
		tk_archive_entry_sparse_add_entry(entry2,
		    sp->offset, sp->length);
		sp = sp->next;
	}

	return (entry2);
}

void
tk_archive_entry_free(struct tk_archive_entry *entry)
{
	tk_archive_entry_clear(entry);
	free(entry);
}

struct tk_archive_entry *
tk_archive_entry_new(void)
{
	return tk_archive_entry_new2(NULL);
}

struct tk_archive_entry *
tk_archive_entry_new2(struct archive *a)
{
	struct tk_archive_entry *entry;

	entry = (struct tk_archive_entry *)malloc(sizeof(*entry));
	if (entry == NULL)
		return (NULL);
	memset(entry, 0, sizeof(*entry));
	entry->archive = a;
	return (entry);
}

/*
 * Functions for reading fields from an archive_entry.
 */

time_t
tk_archive_entry_atime(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_atime);
}

long
tk_archive_entry_atime_nsec(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_atime_nsec);
}

int
tk_archive_entry_atime_is_set(struct tk_archive_entry *entry)
{
	return (entry->ae_set & AE_SET_ATIME);
}

time_t
tk_archive_entry_birthtime(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_birthtime);
}

long
tk_archive_entry_birthtime_nsec(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_birthtime_nsec);
}

int
tk_archive_entry_birthtime_is_set(struct tk_archive_entry *entry)
{
	return (entry->ae_set & AE_SET_BIRTHTIME);
}

time_t
tk_archive_entry_ctime(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_ctime);
}

int
tk_archive_entry_ctime_is_set(struct tk_archive_entry *entry)
{
	return (entry->ae_set & AE_SET_CTIME);
}

long
tk_archive_entry_ctime_nsec(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_ctime_nsec);
}

dev_t
tk_archive_entry_dev(struct tk_archive_entry *entry)
{
	if (entry->ae_stat.aest_dev_is_broken_down)
		return ae_makedev(entry->ae_stat.aest_devmajor,
		    entry->ae_stat.aest_devminor);
	else
		return (entry->ae_stat.aest_dev);
}

int
tk_archive_entry_dev_is_set(struct tk_archive_entry *entry)
{
	return (entry->ae_set & AE_SET_DEV);
}

dev_t
tk_archive_entry_devmajor(struct tk_archive_entry *entry)
{
	if (entry->ae_stat.aest_dev_is_broken_down)
		return (entry->ae_stat.aest_devmajor);
	else
		return major(entry->ae_stat.aest_dev);
}

dev_t
tk_archive_entry_devminor(struct tk_archive_entry *entry)
{
	if (entry->ae_stat.aest_dev_is_broken_down)
		return (entry->ae_stat.aest_devminor);
	else
		return minor(entry->ae_stat.aest_dev);
}

mode_t
tk_archive_entry_filetype(struct tk_archive_entry *entry)
{
	return (AE_IFMT & entry->acl.mode);
}

void
tk_archive_entry_fflags(struct tk_archive_entry *entry,
    unsigned long *set, unsigned long *clear)
{
	*set = entry->ae_fflags_set;
	*clear = entry->ae_fflags_clear;
}

/*
 * Note: if text was provided, this just returns that text.  If you
 * really need the text to be rebuilt in a canonical form, set the
 * text, ask for the bitmaps, then set the bitmaps.  (Setting the
 * bitmaps clears any stored text.)  This design is deliberate: if
 * we're editing archives, we don't want to discard flags just because
 * they aren't supported on the current system.  The bitmap<->text
 * conversions are platform-specific (see below).
 */
const char *
tk_archive_entry_fflags_text(struct tk_archive_entry *entry)
{
	const char *f;
	char *p;

	if (tk_archive_mstring_get_mbs(entry->archive,
	    &entry->ae_fflags_text, &f) == 0) {
		if (f != NULL)
			return (f);
	} else if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");

	if (entry->ae_fflags_set == 0  &&  entry->ae_fflags_clear == 0)
		return (NULL);

	p = ae_fflagstostr(entry->ae_fflags_set, entry->ae_fflags_clear);
	if (p == NULL)
		return (NULL);

	tk_archive_mstring_copy_mbs(&entry->ae_fflags_text, p);
	free(p);
	if (tk_archive_mstring_get_mbs(entry->archive,
	    &entry->ae_fflags_text, &f) == 0)
		return (f);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

int64_t
tk_archive_entry_gid(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_gid);
}

const char *
tk_archive_entry_gname(struct tk_archive_entry *entry)
{
	const char *p;
	if (tk_archive_mstring_get_mbs(entry->archive, &entry->ae_gname, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

const wchar_t *
tk_archive_entry_gname_w(struct tk_archive_entry *entry)
{
	const wchar_t *p;
	if (tk_archive_mstring_get_wcs(entry->archive, &entry->ae_gname, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

int
_tk_archive_entry_gname_l(struct tk_archive_entry *entry,
    const char **p, size_t *len, struct tk_archive_string_conv *sc)
{
	return (tk_archive_mstring_get_mbs_l(&entry->ae_gname, p, len, sc));
}

const char *
tk_archive_entry_hardlink(struct tk_archive_entry *entry)
{
	const char *p;
	if ((entry->ae_set & AE_SET_HARDLINK) == 0)
		return (NULL);
	if (tk_archive_mstring_get_mbs(
	    entry->archive, &entry->ae_hardlink, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

const wchar_t *
tk_archive_entry_hardlink_w(struct tk_archive_entry *entry)
{
	const wchar_t *p;
	if ((entry->ae_set & AE_SET_HARDLINK) == 0)
		return (NULL);
	if (tk_archive_mstring_get_wcs(
	    entry->archive, &entry->ae_hardlink, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

int
_tk_archive_entry_hardlink_l(struct tk_archive_entry *entry,
    const char **p, size_t *len, struct tk_archive_string_conv *sc)
{
	if ((entry->ae_set & AE_SET_HARDLINK) == 0) {
		*p = NULL;
		*len = 0;
		return (0);
	}
	return (tk_archive_mstring_get_mbs_l(&entry->ae_hardlink, p, len, sc));
}

int64_t
tk_archive_entry_ino(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_ino);
}

int
tk_archive_entry_ino_is_set(struct tk_archive_entry *entry)
{
	return (entry->ae_set & AE_SET_INO);
}

int64_t
tk_archive_entry_ino64(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_ino);
}

mode_t
tk_archive_entry_mode(struct tk_archive_entry *entry)
{
	return (entry->acl.mode);
}

time_t
tk_archive_entry_mtime(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_mtime);
}

long
tk_archive_entry_mtime_nsec(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_mtime_nsec);
}

int
tk_archive_entry_mtime_is_set(struct tk_archive_entry *entry)
{
	return (entry->ae_set & AE_SET_MTIME);
}

unsigned int
tk_archive_entry_nlink(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_nlink);
}

const char *
tk_archive_entry_pathname(struct tk_archive_entry *entry)
{
	const char *p;
	if (tk_archive_mstring_get_mbs(
	    entry->archive, &entry->ae_pathname, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

const wchar_t *
tk_archive_entry_pathname_w(struct tk_archive_entry *entry)
{
	const wchar_t *p;
	if (tk_archive_mstring_get_wcs(
	    entry->archive, &entry->ae_pathname, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

int
_tk_archive_entry_pathname_l(struct tk_archive_entry *entry,
    const char **p, size_t *len, struct tk_archive_string_conv *sc)
{
	return (tk_archive_mstring_get_mbs_l(&entry->ae_pathname, p, len, sc));
}

mode_t
tk_archive_entry_perm(struct tk_archive_entry *entry)
{
	return (~AE_IFMT & entry->acl.mode);
}

dev_t
tk_archive_entry_rdev(struct tk_archive_entry *entry)
{
	if (entry->ae_stat.aest_rdev_is_broken_down)
		return ae_makedev(entry->ae_stat.aest_rdevmajor,
		    entry->ae_stat.aest_rdevminor);
	else
		return (entry->ae_stat.aest_rdev);
}

dev_t
tk_archive_entry_rdevmajor(struct tk_archive_entry *entry)
{
	if (entry->ae_stat.aest_rdev_is_broken_down)
		return (entry->ae_stat.aest_rdevmajor);
	else
		return major(entry->ae_stat.aest_rdev);
}

dev_t
tk_archive_entry_rdevminor(struct tk_archive_entry *entry)
{
	if (entry->ae_stat.aest_rdev_is_broken_down)
		return (entry->ae_stat.aest_rdevminor);
	else
		return minor(entry->ae_stat.aest_rdev);
}

int64_t
tk_archive_entry_size(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_size);
}

int
tk_archive_entry_size_is_set(struct tk_archive_entry *entry)
{
	return (entry->ae_set & AE_SET_SIZE);
}

const char *
tk_archive_entry_sourcepath(struct tk_archive_entry *entry)
{
	const char *p;
	if (tk_archive_mstring_get_mbs(
	    entry->archive, &entry->ae_sourcepath, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

const wchar_t *
tk_archive_entry_sourcepath_w(struct tk_archive_entry *entry)
{
	const wchar_t *p;
	if (tk_archive_mstring_get_wcs(
	    entry->archive, &entry->ae_sourcepath, &p) == 0)
		return (p);
	return (NULL);
}

const char *
tk_archive_entry_symlink(struct tk_archive_entry *entry)
{
	const char *p;
	if ((entry->ae_set & AE_SET_SYMLINK) == 0)
		return (NULL);
	if (tk_archive_mstring_get_mbs(
	    entry->archive, &entry->ae_symlink, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

const wchar_t *
tk_archive_entry_symlink_w(struct tk_archive_entry *entry)
{
	const wchar_t *p;
	if ((entry->ae_set & AE_SET_SYMLINK) == 0)
		return (NULL);
	if (tk_archive_mstring_get_wcs(
	    entry->archive, &entry->ae_symlink, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

int
_tk_archive_entry_symlink_l(struct tk_archive_entry *entry,
    const char **p, size_t *len, struct tk_archive_string_conv *sc)
{
	if ((entry->ae_set & AE_SET_SYMLINK) == 0) {
		*p = NULL;
		*len = 0;
		return (0);
	}
	return (tk_archive_mstring_get_mbs_l( &entry->ae_symlink, p, len, sc));
}

int64_t
tk_archive_entry_uid(struct tk_archive_entry *entry)
{
	return (entry->ae_stat.aest_uid);
}

const char *
tk_archive_entry_uname(struct tk_archive_entry *entry)
{
	const char *p;
	if (tk_archive_mstring_get_mbs(entry->archive, &entry->ae_uname, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

const wchar_t *
tk_archive_entry_uname_w(struct tk_archive_entry *entry)
{
	const wchar_t *p;
	if (tk_archive_mstring_get_wcs(entry->archive, &entry->ae_uname, &p) == 0)
		return (p);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (NULL);
}

int
_tk_archive_entry_uname_l(struct tk_archive_entry *entry,
    const char **p, size_t *len, struct tk_archive_string_conv *sc)
{
	return (tk_archive_mstring_get_mbs_l(&entry->ae_uname, p, len, sc));
}

/*
 * Functions to set archive_entry properties.
 */

void
tk_archive_entry_set_filetype(struct tk_archive_entry *entry, unsigned int type)
{
	entry->stat_valid = 0;
	entry->acl.mode &= ~AE_IFMT;
	entry->acl.mode |= AE_IFMT & type;
}

void
tk_archive_entry_set_fflags(struct tk_archive_entry *entry,
    unsigned long set, unsigned long clear)
{
	tk_archive_mstring_clean(&entry->ae_fflags_text);
	entry->ae_fflags_set = set;
	entry->ae_fflags_clear = clear;
}

const char *
tk_archive_entry_copy_fflags_text(struct tk_archive_entry *entry,
    const char *flags)
{
	tk_archive_mstring_copy_mbs(&entry->ae_fflags_text, flags);
	return (ae_strtofflags(flags,
		    &entry->ae_fflags_set, &entry->ae_fflags_clear));
}

const wchar_t *
tk_archive_entry_copy_fflags_text_w(struct tk_archive_entry *entry,
    const wchar_t *flags)
{
	tk_archive_mstring_copy_wcs(&entry->ae_fflags_text, flags);
	return (ae_wcstofflags(flags,
		    &entry->ae_fflags_set, &entry->ae_fflags_clear));
}

void
tk_archive_entry_set_gid(struct tk_archive_entry *entry, int64_t g)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_gid = g;
}

void
tk_archive_entry_set_gname(struct tk_archive_entry *entry, const char *name)
{
	tk_archive_mstring_copy_mbs(&entry->ae_gname, name);
}

void
tk_archive_entry_copy_gname(struct tk_archive_entry *entry, const char *name)
{
	tk_archive_mstring_copy_mbs(&entry->ae_gname, name);
}

void
tk_archive_entry_copy_gname_w(struct tk_archive_entry *entry, const wchar_t *name)
{
	tk_archive_mstring_copy_wcs(&entry->ae_gname, name);
}

int
tk_archive_entry_update_gname_utf8(struct tk_archive_entry *entry, const char *name)
{
	if (tk_archive_mstring_update_utf8(entry->archive,
	    &entry->ae_gname, name) == 0)
		return (1);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (0);
}

int
_tk_archive_entry_copy_gname_l(struct tk_archive_entry *entry,
    const char *name, size_t len, struct tk_archive_string_conv *sc)
{
	return (tk_archive_mstring_copy_mbs_len_l(&entry->ae_gname, name, len, sc));
}

void
tk_archive_entry_set_ino(struct tk_archive_entry *entry, int64_t ino)
{
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_INO;
	entry->ae_stat.aest_ino = ino;
}

void
tk_archive_entry_set_ino64(struct tk_archive_entry *entry, int64_t ino)
{
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_INO;
	entry->ae_stat.aest_ino = ino;
}

void
tk_archive_entry_set_hardlink(struct tk_archive_entry *entry, const char *target)
{
	tk_archive_mstring_copy_mbs(&entry->ae_hardlink, target);
	if (target != NULL)
		entry->ae_set |= AE_SET_HARDLINK;
	else
		entry->ae_set &= ~AE_SET_HARDLINK;
}

void
tk_archive_entry_copy_hardlink(struct tk_archive_entry *entry, const char *target)
{
	tk_archive_mstring_copy_mbs(&entry->ae_hardlink, target);
	if (target != NULL)
		entry->ae_set |= AE_SET_HARDLINK;
	else
		entry->ae_set &= ~AE_SET_HARDLINK;
}

void
tk_archive_entry_copy_hardlink_w(struct tk_archive_entry *entry, const wchar_t *target)
{
	tk_archive_mstring_copy_wcs(&entry->ae_hardlink, target);
	if (target != NULL)
		entry->ae_set |= AE_SET_HARDLINK;
	else
		entry->ae_set &= ~AE_SET_HARDLINK;
}

int
tk_archive_entry_update_hardlink_utf8(struct tk_archive_entry *entry, const char *target)
{
	if (target != NULL)
		entry->ae_set |= AE_SET_HARDLINK;
	else
		entry->ae_set &= ~AE_SET_HARDLINK;
	if (tk_archive_mstring_update_utf8(entry->archive,
	    &entry->ae_hardlink, target) == 0)
		return (1);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (0);
}

int
_tk_archive_entry_copy_hardlink_l(struct tk_archive_entry *entry,
    const char *target, size_t len, struct tk_archive_string_conv *sc)
{
	int r;

	r = tk_archive_mstring_copy_mbs_len_l(&entry->ae_hardlink,
	    target, len, sc);
	if (target != NULL && r == 0)
		entry->ae_set |= AE_SET_HARDLINK;
	else
		entry->ae_set &= ~AE_SET_HARDLINK;
	return (r);
}

void
tk_archive_entry_set_atime(struct tk_archive_entry *entry, time_t t, long ns)
{
	FIX_NS(t, ns);
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_ATIME;
	entry->ae_stat.aest_atime = t;
	entry->ae_stat.aest_atime_nsec = ns;
}

void
tk_archive_entry_unset_atime(struct tk_archive_entry *entry)
{
	tk_archive_entry_set_atime(entry, 0, 0);
	entry->ae_set &= ~AE_SET_ATIME;
}

void
tk_archive_entry_set_birthtime(struct tk_archive_entry *entry, time_t t, long ns)
{
	FIX_NS(t, ns);
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_BIRTHTIME;
	entry->ae_stat.aest_birthtime = t;
	entry->ae_stat.aest_birthtime_nsec = ns;
}

void
tk_archive_entry_unset_birthtime(struct tk_archive_entry *entry)
{
	tk_archive_entry_set_birthtime(entry, 0, 0);
	entry->ae_set &= ~AE_SET_BIRTHTIME;
}

void
tk_archive_entry_set_ctime(struct tk_archive_entry *entry, time_t t, long ns)
{
	FIX_NS(t, ns);
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_CTIME;
	entry->ae_stat.aest_ctime = t;
	entry->ae_stat.aest_ctime_nsec = ns;
}

void
tk_archive_entry_unset_ctime(struct tk_archive_entry *entry)
{
	tk_archive_entry_set_ctime(entry, 0, 0);
	entry->ae_set &= ~AE_SET_CTIME;
}

void
tk_archive_entry_set_dev(struct tk_archive_entry *entry, dev_t d)
{
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_DEV;
	entry->ae_stat.aest_dev_is_broken_down = 0;
	entry->ae_stat.aest_dev = d;
}

void
tk_archive_entry_set_devmajor(struct tk_archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_DEV;
	entry->ae_stat.aest_dev_is_broken_down = 1;
	entry->ae_stat.aest_devmajor = m;
}

void
tk_archive_entry_set_devminor(struct tk_archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_DEV;
	entry->ae_stat.aest_dev_is_broken_down = 1;
	entry->ae_stat.aest_devminor = m;
}

/* Set symlink if symlink is already set, else set hardlink. */
void
tk_archive_entry_set_link(struct tk_archive_entry *entry, const char *target)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		tk_archive_mstring_copy_mbs(&entry->ae_symlink, target);
	else
		tk_archive_mstring_copy_mbs(&entry->ae_hardlink, target);
}

/* Set symlink if symlink is already set, else set hardlink. */
void
tk_archive_entry_copy_link(struct tk_archive_entry *entry, const char *target)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		tk_archive_mstring_copy_mbs(&entry->ae_symlink, target);
	else
		tk_archive_mstring_copy_mbs(&entry->ae_hardlink, target);
}

/* Set symlink if symlink is already set, else set hardlink. */
void
tk_archive_entry_copy_link_w(struct tk_archive_entry *entry, const wchar_t *target)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		tk_archive_mstring_copy_wcs(&entry->ae_symlink, target);
	else
		tk_archive_mstring_copy_wcs(&entry->ae_hardlink, target);
}

int
tk_archive_entry_update_link_utf8(struct tk_archive_entry *entry, const char *target)
{
	int r;
	if (entry->ae_set & AE_SET_SYMLINK)
		r = tk_archive_mstring_update_utf8(entry->archive,
		    &entry->ae_symlink, target);
	else
		r = tk_archive_mstring_update_utf8(entry->archive,
		    &entry->ae_hardlink, target);
	if (r == 0)
		return (1);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (0);
}

int
_tk_archive_entry_copy_link_l(struct tk_archive_entry *entry,
    const char *target, size_t len, struct tk_archive_string_conv *sc)
{
	int r;

	if (entry->ae_set & AE_SET_SYMLINK)
		r = tk_archive_mstring_copy_mbs_len_l(&entry->ae_symlink,
		    target, len, sc);
	else
		r = tk_archive_mstring_copy_mbs_len_l(&entry->ae_hardlink,
		    target, len, sc);
	return (r);
}

void
tk_archive_entry_set_mode(struct tk_archive_entry *entry, mode_t m)
{
	entry->stat_valid = 0;
	entry->acl.mode = m;
}

void
tk_archive_entry_set_mtime(struct tk_archive_entry *entry, time_t t, long ns)
{
	FIX_NS(t, ns);
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_MTIME;
	entry->ae_stat.aest_mtime = t;
	entry->ae_stat.aest_mtime_nsec = ns;
}

void
tk_archive_entry_unset_mtime(struct tk_archive_entry *entry)
{
	tk_archive_entry_set_mtime(entry, 0, 0);
	entry->ae_set &= ~AE_SET_MTIME;
}

void
tk_archive_entry_set_nlink(struct tk_archive_entry *entry, unsigned int nlink)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_nlink = nlink;
}

void
tk_archive_entry_set_pathname(struct tk_archive_entry *entry, const char *name)
{
	tk_archive_mstring_copy_mbs(&entry->ae_pathname, name);
}

void
tk_archive_entry_copy_pathname(struct tk_archive_entry *entry, const char *name)
{
	tk_archive_mstring_copy_mbs(&entry->ae_pathname, name);
}

void
tk_archive_entry_copy_pathname_w(struct tk_archive_entry *entry, const wchar_t *name)
{
	tk_archive_mstring_copy_wcs(&entry->ae_pathname, name);
}

int
tk_archive_entry_update_pathname_utf8(struct tk_archive_entry *entry, const char *name)
{
	if (tk_archive_mstring_update_utf8(entry->archive,
	    &entry->ae_pathname, name) == 0)
		return (1);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (0);
}

int
_tk_archive_entry_copy_pathname_l(struct tk_archive_entry *entry,
    const char *name, size_t len, struct tk_archive_string_conv *sc)
{
	return (tk_archive_mstring_copy_mbs_len_l(&entry->ae_pathname,
	    name, len, sc));
}

void
tk_archive_entry_set_perm(struct tk_archive_entry *entry, mode_t p)
{
	entry->stat_valid = 0;
	entry->acl.mode &= AE_IFMT;
	entry->acl.mode |= ~AE_IFMT & p;
}

void
tk_archive_entry_set_rdev(struct tk_archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_rdev = m;
	entry->ae_stat.aest_rdev_is_broken_down = 0;
}

void
tk_archive_entry_set_rdevmajor(struct tk_archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_rdev_is_broken_down = 1;
	entry->ae_stat.aest_rdevmajor = m;
}

void
tk_archive_entry_set_rdevminor(struct tk_archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_rdev_is_broken_down = 1;
	entry->ae_stat.aest_rdevminor = m;
}

void
tk_archive_entry_set_size(struct tk_archive_entry *entry, int64_t s)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_size = s;
	entry->ae_set |= AE_SET_SIZE;
}

void
tk_archive_entry_unset_size(struct tk_archive_entry *entry)
{
	tk_archive_entry_set_size(entry, 0);
	entry->ae_set &= ~AE_SET_SIZE;
}

void
tk_archive_entry_copy_sourcepath(struct tk_archive_entry *entry, const char *path)
{
	tk_archive_mstring_copy_mbs(&entry->ae_sourcepath, path);
}

void
tk_archive_entry_copy_sourcepath_w(struct tk_archive_entry *entry, const wchar_t *path)
{
	tk_archive_mstring_copy_wcs(&entry->ae_sourcepath, path);
}

void
tk_archive_entry_set_symlink(struct tk_archive_entry *entry, const char *linkname)
{
	tk_archive_mstring_copy_mbs(&entry->ae_symlink, linkname);
	if (linkname != NULL)
		entry->ae_set |= AE_SET_SYMLINK;
	else
		entry->ae_set &= ~AE_SET_SYMLINK;
}

void
tk_archive_entry_copy_symlink(struct tk_archive_entry *entry, const char *linkname)
{
	tk_archive_mstring_copy_mbs(&entry->ae_symlink, linkname);
	if (linkname != NULL)
		entry->ae_set |= AE_SET_SYMLINK;
	else
		entry->ae_set &= ~AE_SET_SYMLINK;
}

void
tk_archive_entry_copy_symlink_w(struct tk_archive_entry *entry, const wchar_t *linkname)
{
	tk_archive_mstring_copy_wcs(&entry->ae_symlink, linkname);
	if (linkname != NULL)
		entry->ae_set |= AE_SET_SYMLINK;
	else
		entry->ae_set &= ~AE_SET_SYMLINK;
}

int
tk_archive_entry_update_symlink_utf8(struct tk_archive_entry *entry, const char *linkname)
{
	if (linkname != NULL)
		entry->ae_set |= AE_SET_SYMLINK;
	else
		entry->ae_set &= ~AE_SET_SYMLINK;
	if (tk_archive_mstring_update_utf8(entry->archive,
	    &entry->ae_symlink, linkname) == 0)
		return (1);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (0);
}

int
_tk_archive_entry_copy_symlink_l(struct tk_archive_entry *entry,
    const char *linkname, size_t len, struct tk_archive_string_conv *sc)
{
	int r;

	r = tk_archive_mstring_copy_mbs_len_l(&entry->ae_symlink,
	    linkname, len, sc);
	if (linkname != NULL && r == 0)
		entry->ae_set |= AE_SET_SYMLINK;
	else
		entry->ae_set &= ~AE_SET_SYMLINK;
	return (r);
}

void
tk_archive_entry_set_uid(struct tk_archive_entry *entry, int64_t u)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_uid = u;
}

void
tk_archive_entry_set_uname(struct tk_archive_entry *entry, const char *name)
{
	tk_archive_mstring_copy_mbs(&entry->ae_uname, name);
}

void
tk_archive_entry_copy_uname(struct tk_archive_entry *entry, const char *name)
{
	tk_archive_mstring_copy_mbs(&entry->ae_uname, name);
}

void
tk_archive_entry_copy_uname_w(struct tk_archive_entry *entry, const wchar_t *name)
{
	tk_archive_mstring_copy_wcs(&entry->ae_uname, name);
}

int
tk_archive_entry_update_uname_utf8(struct tk_archive_entry *entry, const char *name)
{
	if (tk_archive_mstring_update_utf8(entry->archive,
	    &entry->ae_uname, name) == 0)
		return (1);
	if (errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (0);
}

int
_tk_archive_entry_copy_uname_l(struct tk_archive_entry *entry,
    const char *name, size_t len, struct tk_archive_string_conv *sc)
{
	return (tk_archive_mstring_copy_mbs_len_l(&entry->ae_uname,
	    name, len, sc));
}

const void *
tk_archive_entry_mac_metadata(struct tk_archive_entry *entry, size_t *s)
{
  *s = entry->mac_metadata_size;
  return entry->mac_metadata;
}

void
tk_archive_entry_copy_mac_metadata(struct tk_archive_entry *entry,
    const void *p, size_t s)
{
  free(entry->mac_metadata);
  if (p == NULL || s == 0) {
    entry->mac_metadata = NULL;
    entry->mac_metadata_size = 0;
  } else {
    entry->mac_metadata_size = s;
    entry->mac_metadata = malloc(s);
    if (entry->mac_metadata == NULL)
      abort();
    memcpy(entry->mac_metadata, p, s);
  }
}

/*
 * ACL management.  The following would, of course, be a lot simpler
 * if: 1) the last draft of POSIX.1e were a really thorough and
 * complete standard that addressed the needs of ACL archiving and 2)
 * everyone followed it faithfully.  Alas, neither is true, so the
 * following is a lot more complex than might seem necessary to the
 * uninitiated.
 */

struct tk_archive_acl *
tk_archive_entry_acl(struct tk_archive_entry *entry)
{
	return &entry->acl;
}

void
tk_archive_entry_acl_clear(struct tk_archive_entry *entry)
{
	tk_archive_acl_clear(&entry->acl);
}

/*
 * Add a single ACL entry to the internal list of ACL data.
 */
int
tk_archive_entry_acl_add_entry(struct tk_archive_entry *entry,
    int type, int permset, int tag, int id, const char *name)
{
	return tk_archive_acl_add_entry(&entry->acl, type, permset, tag, id, name);
}

/*
 * As above, but with a wide-character name.
 */
int
tk_archive_entry_acl_add_entry_w(struct tk_archive_entry *entry,
    int type, int permset, int tag, int id, const wchar_t *name)
{
	return tk_archive_acl_add_entry_w_len(&entry->acl,
	    type, permset, tag, id, name, wcslen(name));
}

/*
 * Return a count of entries matching "want_type".
 */
int
tk_archive_entry_acl_count(struct tk_archive_entry *entry, int want_type)
{
	return tk_archive_acl_count(&entry->acl, want_type);
}

/*
 * Prepare for reading entries from the ACL data.  Returns a count
 * of entries matching "want_type", or zero if there are no
 * non-extended ACL entries of that type.
 */
int
tk_archive_entry_acl_reset(struct tk_archive_entry *entry, int want_type)
{
	return tk_archive_acl_reset(&entry->acl, want_type);
}

/*
 * Return the next ACL entry in the list.  Fake entries for the
 * standard permissions and include them in the returned list.
 */
int
tk_archive_entry_acl_next(struct tk_archive_entry *entry, int want_type, int *type,
    int *permset, int *tag, int *id, const char **name)
{
	int r;
	r = tk_archive_acl_next(entry->archive, &entry->acl, want_type, type,
		permset, tag, id, name);
	if (r == ARCHIVE_FATAL && errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (r);
}

/*
 * Generate a text version of the ACL.  The flags parameter controls
 * the style of the generated ACL.
 */
const wchar_t *
tk_archive_entry_acl_text_w(struct tk_archive_entry *entry, int flags)
{
	const wchar_t *r;
	r = tk_archive_acl_text_w(entry->archive, &entry->acl, flags);
	if (r == NULL && errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (r);
}

const char *
tk_archive_entry_acl_text(struct tk_archive_entry *entry, int flags)
{
	const char *p;
	if (tk_archive_acl_text_l(&entry->acl, flags, &p, NULL, NULL) != 0
	    && errno == ENOMEM)
		__tk_archive_errx(1, "No memory");
	return (p);
}

int
_tk_archive_entry_acl_text_l(struct tk_archive_entry *entry, int flags,
    const char **acl_text, size_t *len, struct tk_archive_string_conv *sc)
{
	return (tk_archive_acl_text_l(&entry->acl, flags, acl_text, len, sc));
}

/*
 * Following code is modified from UC Berkeley sources, and
 * is subject to the following copyright notice.
 */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static struct flag {
	const char	*name;
	const wchar_t	*wname;
	unsigned long	 set;
	unsigned long	 clear;
} flags[] = {
	/* Preferred (shorter) names per flag first, all prefixed by "no" */
#ifdef SF_APPEND
	{ "nosappnd",	L"nosappnd",		SF_APPEND,	0 },
	{ "nosappend",	L"nosappend",		SF_APPEND,	0 },
#endif
#ifdef  EXT2_APPEND_FL				/* 'a' */
	{ "nosappnd",	L"nosappnd",		EXT2_APPEND_FL,	0 },
	{ "nosappend",	L"nosappend",		EXT2_APPEND_FL,	0 },
#endif
#ifdef SF_ARCHIVED
	{ "noarch",	L"noarch",		SF_ARCHIVED,	0 },
	{ "noarchived",	L"noarchived",       	SF_ARCHIVED,	0 },
#endif
#ifdef SF_IMMUTABLE
	{ "noschg",	L"noschg",		SF_IMMUTABLE,	0 },
	{ "noschange",	L"noschange",		SF_IMMUTABLE,	0 },
	{ "nosimmutable",	L"nosimmutable",	SF_IMMUTABLE,	0 },
#endif
#ifdef EXT2_IMMUTABLE_FL			/* 'i' */
	{ "noschg",	L"noschg",		EXT2_IMMUTABLE_FL,	0 },
	{ "noschange",	L"noschange",		EXT2_IMMUTABLE_FL,	0 },
	{ "nosimmutable",	L"nosimmutable",	EXT2_IMMUTABLE_FL,	0 },
#endif
#ifdef SF_NOUNLINK
	{ "nosunlnk",	L"nosunlnk",		SF_NOUNLINK,	0 },
	{ "nosunlink",	L"nosunlink",		SF_NOUNLINK,	0 },
#endif
#ifdef SF_SNAPSHOT
	{ "nosnapshot",	L"nosnapshot",	SF_SNAPSHOT,	0 },
#endif
#ifdef UF_APPEND
	{ "nouappnd",	L"nouappnd",		UF_APPEND,	0 },
	{ "nouappend",	L"nouappend",		UF_APPEND,	0 },
#endif
#ifdef UF_IMMUTABLE
	{ "nouchg",	L"nouchg",		UF_IMMUTABLE,	0 },
	{ "nouchange",	L"nouchange",		UF_IMMUTABLE,	0 },
	{ "nouimmutable",	L"nouimmutable",	UF_IMMUTABLE,	0 },
#endif
#ifdef UF_NODUMP
	{ "nodump",	L"nodump",		0,		UF_NODUMP},
#endif
#ifdef EXT2_NODUMP_FL				/* 'd' */
	{ "nodump",	L"nodump",		0,		EXT2_NODUMP_FL},
#endif
#ifdef UF_OPAQUE
	{ "noopaque",	L"noopaque",		UF_OPAQUE,	0 },
#endif
#ifdef UF_NOUNLINK
	{ "nouunlnk",	L"nouunlnk",		UF_NOUNLINK,	0 },
	{ "nouunlink",	L"nouunlink",		UF_NOUNLINK,	0 },
#endif
#ifdef UF_COMPRESSED
	{ "nocompressed",L"nocompressed",	UF_COMPRESSED,	0 },
#endif
#ifdef EXT2_UNRM_FL
        { "nouunlink",	L"nouunlink",		EXT2_UNRM_FL,	0},
#endif

#ifdef EXT2_BTREE_FL
        { "nobtree",	L"nobtree",       	EXT2_BTREE_FL,	0 },
#endif

#ifdef EXT2_ECOMPR_FL
        { "nocomperr",	L"nocomperr",       	EXT2_ECOMPR_FL,	0 },
#endif

#ifdef EXT2_COMPR_FL				/* 'c' */
        { "nocompress",	L"nocompress",       	EXT2_COMPR_FL,	0 },
#endif

#ifdef EXT2_NOATIME_FL				/* 'A' */
        { "noatime",	L"noatime",		0,		EXT2_NOATIME_FL},
#endif

#ifdef EXT2_DIRTY_FL
        { "nocompdirty",L"nocompdirty",		EXT2_DIRTY_FL,		0},
#endif

#ifdef EXT2_COMPRBLK_FL
#ifdef EXT2_NOCOMPR_FL
        { "nocomprblk",	L"nocomprblk",		EXT2_COMPRBLK_FL, EXT2_NOCOMPR_FL},
#else
        { "nocomprblk",	L"nocomprblk",		EXT2_COMPRBLK_FL,	0},
#endif
#endif
#ifdef EXT2_DIRSYNC_FL
        { "nodirsync",	L"nodirsync",		EXT2_DIRSYNC_FL,	0},
#endif
#ifdef EXT2_INDEX_FL
        { "nohashidx",	L"nohashidx",		EXT2_INDEX_FL,		0},
#endif
#ifdef EXT2_IMAGIC_FL
        { "noimagic",	L"noimagic",		EXT2_IMAGIC_FL,		0},
#endif
#ifdef EXT3_JOURNAL_DATA_FL
        { "nojournal",	L"nojournal",		EXT3_JOURNAL_DATA_FL,	0},
#endif
#ifdef EXT2_SECRM_FL
        { "nosecuredeletion",L"nosecuredeletion",EXT2_SECRM_FL,		0},
#endif
#ifdef EXT2_SYNC_FL
        { "nosync",	L"nosync",		EXT2_SYNC_FL,		0},
#endif
#ifdef EXT2_NOTAIL_FL
        { "notail",	L"notail",		0,		EXT2_NOTAIL_FL},
#endif
#ifdef EXT2_TOPDIR_FL
        { "notopdir",	L"notopdir",		EXT2_TOPDIR_FL,		0},
#endif
#ifdef EXT2_RESERVED_FL
        { "noreserved",	L"noreserved",		EXT2_RESERVED_FL,	0},
#endif

	{ NULL,		NULL,			0,		0 }
};

/*
 * fflagstostr --
 *	Convert file flags to a comma-separated string.  If no flags
 *	are set, return the empty string.
 */
static char *
ae_fflagstostr(unsigned long bitset, unsigned long bitclear)
{
	char *string, *dp;
	const char *sp;
	unsigned long bits;
	struct flag *flag;
	size_t	length;

	bits = bitset | bitclear;
	length = 0;
	for (flag = flags; flag->name != NULL; flag++)
		if (bits & (flag->set | flag->clear)) {
			length += strlen(flag->name) + 1;
			bits &= ~(flag->set | flag->clear);
		}

	if (length == 0)
		return (NULL);
	string = (char *)malloc(length);
	if (string == NULL)
		return (NULL);

	dp = string;
	for (flag = flags; flag->name != NULL; flag++) {
		if (bitset & flag->set || bitclear & flag->clear) {
			sp = flag->name + 2;
		} else if (bitset & flag->clear  ||  bitclear & flag->set) {
			sp = flag->name;
		} else
			continue;
		bitset &= ~(flag->set | flag->clear);
		bitclear &= ~(flag->set | flag->clear);
		if (dp > string)
			*dp++ = ',';
		while ((*dp++ = *sp++) != '\0')
			;
		dp--;
	}

	*dp = '\0';
	return (string);
}

/*
 * strtofflags --
 *	Take string of arguments and return file flags.  This
 *	version works a little differently than strtofflags(3).
 *	In particular, it always tests every token, skipping any
 *	unrecognized tokens.  It returns a pointer to the first
 *	unrecognized token, or NULL if every token was recognized.
 *	This version is also const-correct and does not modify the
 *	provided string.
 */
static const char *
ae_strtofflags(const char *s, unsigned long *setp, unsigned long *clrp)
{
	const char *start, *end;
	struct flag *flag;
	unsigned long set, clear;
	const char *failed;

	set = clear = 0;
	start = s;
	failed = NULL;
	/* Find start of first token. */
	while (*start == '\t'  ||  *start == ' '  ||  *start == ',')
		start++;
	while (*start != '\0') {
		/* Locate end of token. */
		end = start;
		while (*end != '\0'  &&  *end != '\t'  &&
		    *end != ' '  &&  *end != ',')
			end++;
		for (flag = flags; flag->name != NULL; flag++) {
			if (memcmp(start, flag->name, end - start) == 0) {
				/* Matched "noXXXX", so reverse the sense. */
				clear |= flag->set;
				set |= flag->clear;
				break;
			} else if (memcmp(start, flag->name + 2, end - start)
			    == 0) {
				/* Matched "XXXX", so don't reverse. */
				set |= flag->set;
				clear |= flag->clear;
				break;
			}
		}
		/* Ignore unknown flag names. */
		if (flag->name == NULL  &&  failed == NULL)
			failed = start;

		/* Find start of next token. */
		start = end;
		while (*start == '\t'  ||  *start == ' '  ||  *start == ',')
			start++;

	}

	if (setp)
		*setp = set;
	if (clrp)
		*clrp = clear;

	/* Return location of first failure. */
	return (failed);
}

/*
 * wcstofflags --
 *	Take string of arguments and return file flags.  This
 *	version works a little differently than strtofflags(3).
 *	In particular, it always tests every token, skipping any
 *	unrecognized tokens.  It returns a pointer to the first
 *	unrecognized token, or NULL if every token was recognized.
 *	This version is also const-correct and does not modify the
 *	provided string.
 */
static const wchar_t *
ae_wcstofflags(const wchar_t *s, unsigned long *setp, unsigned long *clrp)
{
	const wchar_t *start, *end;
	struct flag *flag;
	unsigned long set, clear;
	const wchar_t *failed;

	set = clear = 0;
	start = s;
	failed = NULL;
	/* Find start of first token. */
	while (*start == L'\t'  ||  *start == L' '  ||  *start == L',')
		start++;
	while (*start != L'\0') {
		/* Locate end of token. */
		end = start;
		while (*end != L'\0'  &&  *end != L'\t'  &&
		    *end != L' '  &&  *end != L',')
			end++;
		for (flag = flags; flag->wname != NULL; flag++) {
			if (wmemcmp(start, flag->wname, end - start) == 0) {
				/* Matched "noXXXX", so reverse the sense. */
				clear |= flag->set;
				set |= flag->clear;
				break;
			} else if (wmemcmp(start, flag->wname + 2, end - start)
			    == 0) {
				/* Matched "XXXX", so don't reverse. */
				set |= flag->set;
				clear |= flag->clear;
				break;
			}
		}
		/* Ignore unknown flag names. */
		if (flag->wname == NULL  &&  failed == NULL)
			failed = start;

		/* Find start of next token. */
		start = end;
		while (*start == L'\t'  ||  *start == L' '  ||  *start == L',')
			start++;

	}

	if (setp)
		*setp = set;
	if (clrp)
		*clrp = clear;

	/* Return location of first failure. */
	return (failed);
}


#ifdef TEST
#include <stdio.h>
int
main(int argc, char **argv)
{
	struct tk_archive_entry *entry = tk_archive_entry_new();
	unsigned long set, clear;
	const wchar_t *remainder;

	remainder = tk_archive_entry_copy_fflags_text_w(entry, L"nosappnd dump archive,,,,,,,");
	tk_archive_entry_fflags(entry, &set, &clear);

	wprintf(L"set=0x%lX clear=0x%lX remainder='%ls'\n", set, clear, remainder);

	wprintf(L"new flags='%s'\n", tk_archive_entry_fflags_text(entry));
	return (0);
}
#endif
