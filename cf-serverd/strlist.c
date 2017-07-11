/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
#include "strlist.h"

#include <misc_lib.h>
#include <logging.h>


int memrcmp(const void *p1, const void *p2, size_t n)
{
    const char *s1 = p1;
    const char *s2 = p2;

    while (n > 0)
    {
        if (*s1 != *s2)
        {
            return *s1 - *s2;
        }
        s1--;
        s2--;
        n--;
    }

    return 0;
}


/***************************** STRING ***************************************/


struct string *string_New(const char *s)
{
    size_t s_len = strlen(s);
    struct string *p = malloc(sizeof(*p) + s_len + 1);

    if (p != NULL)
    {
        p->len = s_len;
        /* p->size = s_len + 1; */
        memcpy(p->str, s, s_len + 1);
    }

    return p;
}

/* Compare without ordering, returning only true or false. */
bool string_BoolCompare(const struct string *s1,
                        const struct string *s2)
{
    int result = false;

    if (s1->len == s2->len)
    {
        result = (memcmp(s1->str, s2->str, s1->len) == 0);
    }

    return result;
}

/* Compare and return -1, 0, 1 according to alphabetical order. */
int string_Compare(const struct string **sp1,
                   const struct string **sp2)
{
    const struct string *s1 = *sp1;
    const struct string *s2 = *sp2;
    int result;

    if (s1->len < s2->len)
    {
        if (s1->len == 0)
        {
            return -1;
        }
        else
        {
            result = memcmp(s1->str, s2->str, s1->len);
            return result == 0 ? -1 : result;
        }
    }
    else if (s1->len > s2->len)
    {
        if (s2->len == 0)
        {
            return 1;
        }
        else
        {
            result = memcmp(s1->str, s2->str, s2->len);
            return result == 0 ? 1 : result;
        }
    }
    else                                              /* s1->len == s2->len */
    {
        if (s1->len == 0)                   /* both are zero length strings */
        {
            return 0;
        }
        else
        {
            return memcmp(s1->str, s2->str, s1->len);
        }
    }
}

/**
 * Compare two strings starting from their end.
 */
int string_CompareFromEnd(const struct string **sp1,
                          const struct string **sp2)
{
    const struct string *s1 = *sp1;
    const struct string *s2 = *sp2;
    int result;

    /* Pointer to last char of s1 and s2. */
    const char *last1 = &s1->str[s1->len - 1];
    const char *last2 = &s2->str[s2->len - 1];

    if (s1->len < s2->len)
    {
        result = memrcmp(last1, last2, s1->len);
        return result == 0 ? -1 : result;
    }
    else if (s1->len > s2->len)
    {
        result = memrcmp(last1, last2, s2->len);
        return result == 0 ? 1 : result;
    }
    else                                              /* s1->len == s2->len */
    {
        return memrcmp(last1, last2, s1->len);
    }
}

/**
 * Compare two strings.
 *
 * @return the length of the substring that matches. To find out if the two
 *         strings match exactly you must also check if
 *         s1->len == s2->len == return_value.
 */
size_t string_MatchCount(const struct string *s1,
                         const struct string *s2)
{
    size_t len = MIN(s1->len, s2->len);

    size_t i = 0;
    while (i < len && s1->str[i] == s2->str[i])
    {
        i++;
    }

    return i;
}

/**
 * Compare two strings starting from their end.
 *
 * @return the length of the substring that matches. To find out if the two
 *         strings match exactly you must also check if
 *         s1->len == s2->len == return_value.
 */
size_t string_ReverseMatchCount(const struct string *s1,
                                const struct string *s2)
{
    size_t len = MIN(s1->len, s2->len);
    const char *last1 = &s1->str[s1->len - 1];
    const char *last2 = &s2->str[s2->len - 1];

    size_t i = 0;
    while (i < len && *last1 == *last2)
    {
        i++;
    }

    return i;
}


/***************************** STRLIST **************************************/


size_t StrList_Len(const StrList *sl)
{
    return (sl == NULL) ? 0 : sl->len;
}

char *StrList_At(const StrList *sl, size_t idx)
{
    assert(sl != NULL);
    assert(idx < sl->len);
    return sl->list[idx]->str;
}

/**
 * Insert s at the given index, reallocating if necessary.
 *
 * @return index of the newly inserted string. If -1 (i.e. MAXINT) is returned
 *         then something went horribly wrong.
 */
size_t StrList_Insert(StrList **sl, const char *s, size_t idx)
{
    assert(s != NULL);

    StrList *slp = *sl;                    /* for clarity only */
    size_t new_alloc_len = 0;          /* 0 means no reallocation is needed */

    if ((slp == NULL && idx > 0) ||
        (slp != NULL && idx > slp->len))
    {
        ProgrammingError("StrList_Insert: Out of bounds index %zu", idx);
    }

    if (slp == NULL)
    {
        slp = calloc(1, sizeof(*slp) + sizeof(*slp->list) * 1);
        if (slp == NULL)
        {
            return (size_t) -1;
        }
        slp->alloc_len = 1;
        *sl = slp;
    }
    else if (slp->alloc_len == 0)
    {
        assert (slp->len == 0);
        new_alloc_len = 1;
    }
    else if (slp->len == slp->alloc_len)                 /* need more space */
    {
        new_alloc_len = slp->alloc_len * 2;
    }

    if (new_alloc_len > 0)                        /* reallocation is needed */
    {
        StrList *p =
            realloc(slp, sizeof(*p) + sizeof (*p->list) * new_alloc_len);
        if (p == NULL)
        {
            return (size_t) -1;
        }

        slp = p;
        slp->alloc_len = new_alloc_len;
        *sl = slp;                          /* Change the caller's variable */
    }

    struct string *str = string_New(s);
    if (str == NULL)
    {
        /* Our list has grown but contents are the same, we can exit clean. */
        return (size_t) -1;
    }

    assert(slp->len < slp->alloc_len);

    memmove(&slp->list[idx + 1], &slp->list[idx],              /* Make room */
            (slp->len - idx) * sizeof(slp->list[idx]));

    slp->list[idx] = str;                                 /* Insert element */
    slp->len++;

    return idx;
}

/**
 * Appends a string to strlist sl, reallocating if necessary. It returns NULL
 * if reallocation failed, in which case the initial strlist is still valid.
 * Else it returns a pointer to the newly appended string.
 *
 * @note It is valid for sl to be NULL, which is equivalent to being empty.
 */
size_t StrList_Append(StrList **sl, const char *s)
{
    assert(s != NULL);

    size_t ret;

    if (*sl == NULL)
    {
        ret = StrList_Insert(sl, s, 0);
    }
    else
    {
        ret = StrList_Insert(sl, s, (*sl)->len);
    }

    return ret;
}

/**
 * Trim allocated memory to the minimum needed. Free the whole struct if
 * deemed necessary. This function will never fail. In the unlikely event that
 * realloc() fails to reduce the allocated amount, then we just keep the same
 * memory and log an UnexpectedError.
 *
 * @param **sl in-out param, might become NULL if the struct was freed.
 */
void StrList_Finalise(StrList **sl)
{
    StrList *slp = *sl;                    /* for clarity only */

    if (slp == NULL)
    {
        return;
    }

    assert(slp->len <= slp->alloc_len);

    if (slp->len == 0)
    {
        free(slp);
        slp = NULL;
    }
    else if (slp->len < slp->alloc_len)
    {
        StrList *p =
            realloc(slp, sizeof(*p) + sizeof (*p->list) * slp->len);

        if (p == NULL)
        {
            UnexpectedError("realloc() returned error even though we asked to *reduce* allocated amount: %s",
                            GetErrorStr());
        }
        else
        {
            slp = p;
            slp->alloc_len = slp->len;
        }
    }

    *sl = slp;
}

/**
 * Frees everything and sets the strlist back to NULL, i.e. empty.
 */
void StrList_Free(StrList **sl)
{
    StrList *slp = *sl;                               /* for clarity */

    if (slp == NULL)
    {
        return;
    }

    size_t i;
    for (i = 0; i < slp->len; i++)
    {
        free(slp->list[i]);
    }
    free(slp);

    *sl = NULL;
}


void StrList_Sort(StrList *sl, StringComparatorF comparator)
{
    if (sl != NULL && sl->len > 0)
    {
        qsort(sl->list, sl->len, sizeof(sl->list[0]),
              (int (*)(const void *, const void *)) comparator);
    }
}

/**
 * A different binary search, calls libc's bsearch which also means it's also
 * limited by it: if element is not found only failure can be returned.
 *
 * @return index of match or (size_t) -1 if not found
 */
size_t StrList_bsearch(const StrList *sl,
                       const struct string *s,
                       StringComparatorF comparator)
{
    if (sl != NULL && sl->len > 0)
    {
        struct string **ret =
            bsearch(&s, sl->list, sl->len, sizeof(sl->list[0]),
                    (int (*)(const void *, const void *)) comparator);
        struct string * const *base = &sl->list[0];
        return (ret == NULL) ? (size_t) -1 : (ret - base);
    }
    else
    {
        return (size_t) -1;
    }
}

/**
 * Binary search for the string. Obviously the list must be sorted.
 * If element not found return the position it should be inserted in.
 *
 * @param position returns either the index where it was found, or
 *        the index where it should be inserted in to keep it sorted.
 */
bool StrList_BinarySearchString(const StrList *slp,
                                const struct string *s,
                                size_t *position)
{
    if (slp == NULL)
    {
        *position = 0;
        return false;
    }

    size_t min = 0;
    size_t max = slp->len;

    /* -1 produces "Not found" if we don't iterate at all (empty list). */
    int ret = -1;
    size_t mid = 0;

    while (min < max)
    {
        mid = min + (max - min) / 2;
        const struct string *s_mid = slp->list[mid];

        ret = string_Compare(&s, &s_mid);
        if (ret == -1)                                     /* s < list[mid] */
        {
            max = mid;
        }
        else if (ret == 1)                                 /* s > list[mid] */
        {
            min = mid + 1;
        }
        else                                              /* s == list[mid] */
        {
            break;
        }
    }

    *position = mid;
    return (ret == 0);
}

/**
 * Same as previous, but accepts a raw char* pointer rather than a struct
 * string.
 */
bool StrList_BinarySearch(const StrList *slp, const char *s,
                          size_t *position)
{
    if (slp == NULL)
    {
        *position = 0;
        return false;
    }

    size_t min = 0;
    size_t max = slp->len;

    /* -1 produces "Not found" if we don't iterate at all (empty list). */
    int ret = -1;
    size_t mid = 0;

    while (min < max)
    {
        mid = min + (max - min) / 2;
        ret = strcmp(s, slp->list[mid]->str);
        if (ret < 0)                                       /* s < list[mid] */
        {
            max = mid;
        }
        else if (ret > 0)                                  /* s > list[mid] */
        {
            min = mid + 1;
            /* insert *after* the comparison point, if we exit. */
            mid++;
        }
        else                                              /* s == list[mid] */
        {
            break;
        }
    }

    *position = mid;
    return (ret == 0);
}

/* Search a sorted strlist for string s, of s_len, in forward or backward
 * direction (for the latter the strlist must be sorted with
 * string_CompareFromEnd()). */
static
size_t StrList_BinarySearchExtended(const StrList *sl,
                                    const char *s, size_t s_len,
                                    bool direction_forward,
                                    size_t *min, size_t *max)
{
    size_t found = -1;

    size_t priv_min = *min;
    size_t priv_max = *max;

    assert(s_len > 0);
    assert(priv_min < priv_max);
    assert(priv_max <= sl->len);

    while (priv_min < priv_max)
    {
        size_t pos = (priv_max - priv_min) / 2 + priv_min;
        const char *s2 = sl->list[pos]->str;
        size_t s2_len = sl->list[pos]->len;
        size_t min_len = MIN(s_len, s2_len);
        int match;
        if (direction_forward)
        {
            match = memcmp(s, sl->list[pos]->str, min_len);
        }
        else
        {
            match = memrcmp(s, &s2[s2_len - 1], min_len);
        }
        if (match == 0)
        {
            if (sl->list[pos]->len == s_len)
            {
                /* Exact match, we know the list has no duplicates so it's
                 * the first match. */
                found = pos;
                /* We might as well not search here later, when s_len is
                 * longer, because longer means it won't match this. */
                *min = pos + 1;
                break;
            }
            else
            {
                /* Prefix match, sl[pos] is superstring of s. That means
                 * that the exact match may be before. */
                priv_max = pos;
                /* However we keep this as a different case because we must
                 * not change *max, since that position might match a search
                 * later, with bigger s_len. */
            }
        }
        else if (match < 0)                            /* s < sl[pos] */
        {
            priv_max = pos;
            /* This position will never match, no matter how big s_len grows
             * in later searches. Thus we change *max to avoid searching
             * here. */
            *max = priv_max;
        }
        else                                           /* s > sl[pos] */
        {
            priv_min = pos + 1;
            /* Same here, this position will never match even for longer s. */
            *min = priv_min;
        }
    }

    return found;
}

/**
 * Find the longest string in #sl that is a prefix of #s (of length #s_len and
 * not necessarily '\0'-terminated), delimited by #separator.
 *
 * @param #s_len can be the length of #s, since #s is not necessarily
 *        '\0'-terminated. Is #s_len is 0, then #s is assumed to be
 *        '\0'-terminated and length is computed with strlen().
 *        @note this means that #s can't be 0 bytes...
 *
 * @example if #sl is { "/a/", "/a/b/", "/a/c/" } and we are searching for
 *          #s="/a/d/f" with #separator='/' then this function returns 0, which
 *          is the index of "/a/".
 *
 * @example if #sl is { ".com", ".net", ".cfengine.com" } and we are searching
 *          for #s="cfengine.com" with #separator='.' and
 *          #direction_forward=false, then this function returns 0, which is
 *          the index of ".com".
 *          if we searched for #s="www.cfengine.com" then it would return 2,
 *          which is the index of "www.cfengine.com".
 */
size_t StrList_SearchLongestPrefix(const StrList *sl,
                                   const char *s, size_t s_len,
                                   char separator, bool direction_forward)
{
    /* Remember, NULL strlist is equivalent to empty strlist. */
    if (sl == NULL)
    {
        return (size_t) -1;
    }

    if (s_len == 0)
    {
        s_len = strlen(s);
    }

    size_t found = -1;
    size_t old_found = -1;
    size_t s_prefix_len = 0;
    size_t min = 0;
    size_t max = sl->len;
    bool longer_match_possible = true;

    /* Keep searching until we've searched the whole length, or until there is
     * no reason to keep going. */
    while (longer_match_possible && (s_prefix_len < s_len))
    {
        char *separator_at;
        if (direction_forward)
        {
            /* Find next separator, skipping the previous one. */
            separator_at = memchr(&s[s_prefix_len], separator,
                                  s_len - s_prefix_len);
            s_prefix_len = separator_at - &s[0] + 1;
        }
        else
        {
            /* In this case, SearchLongestPrefix should be SearchLongestSuffix.
             * Find next separator from the end, skipping the previous one. */
            separator_at = memrchr(s, separator,
                                   s_len - s_prefix_len);
            s_prefix_len = &s[s_len - 1] - separator_at + 1;
        }

        if (separator_at == NULL)
        {
            s_prefix_len = s_len;     /* No separator found, use all string */
        }

        /* printf("StrList_SearchLongestPrefix %s: " */
        /*        "'%s' len:%zu prefix_len:%zu\n", */
        /*        direction_forward == true ? "forward" : "backward", */
        /*        s, s_len, s_prefix_len); */
        assert(s_prefix_len <= s_len);

        if (found != (size_t) -1)
        {
            /* Keep the smaller string match in case we don't match again. */
            old_found = found;
        }

        found = StrList_BinarySearchExtended(sl,
            direction_forward == true ? s : &s[s_len - 1],
            s_prefix_len, direction_forward, &min, &max);

        /* If not even a superstring was found then don't keep trying. */
        longer_match_possible = (min < max);
    }

    found = (found == (size_t) -1) ? old_found : found;

    /* printf("StrList_SearchLongestPrefix s:'%s' len:%zu found:'%s'\n", */
    /*        s, s_len, (found == -1) ? "NONE" : sl->list[found]->str); */

    return found;
}

/**
 *  Search within the given strlist for any prefix of the string #s (or exact
 *  match). Not guaranteed it will return the shortest or longest prefix, just
 *  that it will return *fast* once a prefix or exact match is found.
 *
 *  @param #direction_forward if set to false then search is done for suffix,
 *                            not prefix.
 *  @return the index of the found string, (size_t) -1 if not found.
 */
size_t StrList_SearchForPrefix(const StrList *sl,
                               const char *s, size_t s_len,
                               bool direction_forward)
{
    /* Remember, NULL strlist is equivalent to empty strlist. */
    if (sl == NULL)
    {
        return (size_t) -1;
    }

    if (s_len == 0)
    {
        s_len = strlen(s);
    }

    size_t min = 0;
    size_t max = sl->len;
    size_t chr_idx = 0;
    size_t prev_chr_idx = chr_idx;

    while (min < max)
    {
        size_t mid = min + (max - min) / 2;
        const char *s2 = sl->list[mid]->str;
        size_t s2_len  = sl->list[mid]->len;
        size_t min_len = MIN(s_len, s2_len);

        /* We didn't find a string in last iteration so now we are at a
         * different position (mid) in strlist. Nobody guarantees that the
         * first bytes still match, so we'll have to reset
         * chr_idx. Nevertheless, we are sure that the first prev_chr_index
         * bytes match, because they have already matched twice. */
        size_t min_chr_idx = MIN(prev_chr_idx, chr_idx);
        prev_chr_idx = chr_idx;

        /* Count the matching characters. */
        chr_idx = min_chr_idx;
        if (direction_forward)
        {
            while (chr_idx < min_len &&
                   s[chr_idx] == s2[chr_idx])
            {
                chr_idx++;
            }
        }
        else
        {
            while (chr_idx < min_len &&
                   s[s_len - 1 - chr_idx] == s2[s2_len - 1 - chr_idx])
            {
                chr_idx++;
            }
        }

        if (chr_idx == s_len)
        {
            if (s_len == s2_len)
            {
                /* We found an exact match. */
                return mid;
            }
            else
            {
                assert(s_len < s2_len);
                /* We found a superstring of s (i.e. s is a prefix). Don't do
                 * anything, need to keep searching for smaller strings. */
            }
        }
        else if (chr_idx == s2_len)
        {
            /* We found a prefix of s. */
            return mid;
        }

        /* No match, need to keep searching... */
        int compar = s[chr_idx] - sl->list[mid]->str[chr_idx];
        if (compar < 0)
        {
            max = mid;
        }
        else
        {
            assert(compar > 0);
            min = mid + 1;
        }
    }

    return (size_t) -1;
}
