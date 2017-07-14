/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/
#ifndef CFENGINE_STRLIST_H
#define CFENGINE_STRLIST_H


#include <platform.h>


/**
 * Simple length-aware string type. Allocate with string_New. Free
 * simply with free().
 */
struct string
{
    size_t len;                 /* str length not counting terminating-'\0' */
//    size_t size;                /* str allocated size */
    char str[];
};


struct string *string_New(const char *s);
bool string_BoolCompare(const struct string *s1, const struct string *s2);
size_t string_MatchCount(const struct string *s1, const struct string *s2);
size_t string_ReverseMatchCount(const struct string *s1, const struct string *s2);

typedef int (*StringComparatorF)(const struct string **,
                                 const struct string **);
int string_Compare(const struct string **s1,
                   const struct string **s2);
int string_CompareFromEnd(const struct string **s1,
                          const struct string **s2);


/**
 * strlist is a list of #len strings.
 *
 * @note strlist can be NULL, which is equivalent to having 0 elements. In
 *       fact, NULL is the properly initialised strlist, it will be
 *       automatically allocated after using StrList_Insert() or
 *       StrList_Append().
 *
 * @note strlist can be a container for *any* kind of data, not only strings,
 *       as long as it is a one-piece memory block, and a struct with first
 *       field being "size_t len". '\0' termination is not needed at any
 *       point, so just insert/append your custom buffers.
 *       To use it like that use the Raw family of functions.
 */
typedef struct strlist
{
    size_t len;
    size_t alloc_len;                              /* for realloc() economy */
    struct string *list[];
} StrList;

size_t StrList_Len(const StrList *sl);
char *StrList_At(const StrList *sl, size_t idx);
size_t StrList_Insert(StrList **sl, const char *s, size_t idx);
size_t StrList_Append(StrList **sl, const char *s);
void StrList_Finalise(StrList **sl);
void StrList_Free(StrList **sl);
void StrList_Sort(StrList *sl, StringComparatorF f);
bool StrList_BinarySearchString(const StrList *slp,
                                const struct string *s,
                                size_t *position);
bool StrList_BinarySearch(const StrList *slp, const char *s,
                          size_t *position);
size_t StrList_SearchLongestPrefix(const StrList *sl,
                                  const char *s, size_t s_len,
                                  char separator, bool direction_forward);
size_t StrList_SearchForPrefix(const StrList *sl,
                               const char *s, size_t s_len,
                               bool direction_forward);


#endif
