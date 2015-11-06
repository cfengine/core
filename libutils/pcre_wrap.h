#ifndef PCRE_WRAP_H_INCLUDED
#define PCRE_WRAP_H_INCLUDED



#include <platform.h>
#include <regex.h>

/*
 * Constants:
 */

#define FALSE 0
#define TRUE 1

/* Capacity */
#define PCRE_WRAP_MAX_SUBMATCHES  33     /* Maximum number of capturing subpatterns allowed. MUST be <= 99! FIXME: Should be dynamic */
#define PCRE_WRAP_MAX_MATCH_INIT  40     /* Initial amount of matches that can be stored in global searches */
#define PCRE_WRAP_MAX_MATCH_GROW  1.6    /* Factor by which storage for matches is extended if exhausted */

/* Error codes */
#define PCRE_WRAP_ERR_NOMEM     -10      /* Failed to acquire memory. */
#define PCRE_WRAP_ERR_CMDSYNTAX -11      /* Syntax of s///-command */
#define PCRE_WRAP_ERR_STUDY     -12      /* pcre error while studying the pattern */
#define PCRE_WRAP_ERR_BADJOB    -13      /* NULL job pointer, pattern or substitute */
#define PCRE_WRAP_WARN_BADREF   -14      /* Backreference out of range */

/* Flags */
#define PCRE_WRAP_GLOBAL          1      /* Job should be applied globally, as with perl's g option */
#define PCRE_WRAP_TRIVIAL         2      /* Backreferences in the substitute are ignored */
#define PCRE_WRAP_SUCCESS         4      /* Job did previously match */


/*
 * Data types:
 */

/* A compiled substitute */

typedef struct {
    char  *text;                                   /* The plaintext part of the substitute, with all backreferences stripped */
    int    backrefs;                               /* The number of backreferences */
    int    block_offset[PCRE_WRAP_MAX_SUBMATCHES];      /* Array with the offsets of all plaintext blocks in text */
    size_t block_length[PCRE_WRAP_MAX_SUBMATCHES];      /* Array with the lengths of all plaintext blocks in text */
    int    backref[PCRE_WRAP_MAX_SUBMATCHES];           /* Array with the backref number for all plaintext block borders */
    int    backref_count[PCRE_WRAP_MAX_SUBMATCHES + 2]; /* Array with the number of references to each backref index */
} pcre_wrap_substitute;


/*
 * A match, including all captured subpatterns (submatches)
 * Note: The zeroth is the whole match, the PCRE_WRAP_MAX_SUBMATCHES + 0th
 * is the range before the match, the PCRE_WRAP_MAX_SUBMATCHES + 1th is the
 * range after the match.
 */

typedef struct {
    int    submatches;                               /* Number of captured subpatterns */
    int    submatch_offset[PCRE_WRAP_MAX_SUBMATCHES + 2]; /* Offset for each submatch in the subject */
    size_t submatch_length[PCRE_WRAP_MAX_SUBMATCHES + 2]; /* Length of each submatch in the subject */
} pcre_wrap_match;


/* A PCRE_WRAP job */

typedef struct PCRE_WRAP_JOB {
    pcre *pattern;                            /* The compiled pcre pattern */
    pcre_extra *hints;                        /* The pcre hints for the pattern */
    int options;                              /* The pcre options (numeric) */
    int flags;                                /* The pcre_wrap and user flags (see "Flags" above) */
    pcre_wrap_substitute *substitute;              /* The compiled pcre_wrap substitute */
    struct PCRE_WRAP_JOB *next;                    /* Pointer for chaining jobs to joblists */
} pcre_wrap_job;


/*
 * Prototypes:
 */

/* Main usage */
extern pcre_wrap_job        *pcre_wrap_compile(const char *pattern, const char *substitute, const char *options, int *errptr);
extern int              pcre_wrap_execute(pcre_wrap_job *job, char *subject, size_t subject_length, char **result, size_t *result_length);

/* Freeing jobs */
extern pcre_wrap_job        *pcre_wrap_free_job(pcre_wrap_job *job);

/* Info on errors: */
extern const char *pcre_wrap_strerror(const int error);

#endif /* ndef PCRE_WRAP_H_INCLUDED */

/*
  Local Variables:
  tab-width: 3
  end:
*/
