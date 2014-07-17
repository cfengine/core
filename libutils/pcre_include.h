/*
 * PCRE in RHEL4 is within a subdirectory itself. We do the inclusion here so
 * that you can just do #include "pcre_include.h" instead of ifdef'ing all the
 * time.
 */


#ifdef HAVE_PCRE_H
# include <pcre.h>
#endif

#ifdef HAVE_PCRE_PCRE_H
# include <pcre/pcre.h>
#endif
