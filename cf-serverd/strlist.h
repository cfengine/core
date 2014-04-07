


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
