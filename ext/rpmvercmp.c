/*

rpmvercmp.c contains code from RPM project, licensed as the following:

RPM and it's source code are covered under two separate licenses.

The entire code base may be distributed under the terms of the GNU General
Public License (GPL), which appears immediately below.  Alternatively,
all of the source code in the lib subdirectory of the RPM source code
distribution as well as any code derived from that code may instead be
distributed under the GNU Library General Public License (LGPL), at the
choice of the distributor. The complete text of the LGPL appears
at the bottom of this file.

This alternatively is allowed to enable applications to be linked against
the RPM library (commonly called librpm) without forcing such applications
to be distributed under the GPL.

Any questions regarding the licensing of RPM should be addressed to
marc@redhat.com and ewt@redhat.com.
*/

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

static inline int rstreq(const char *s1, const char *s2)
{
    return (strcmp(s1, s2) == 0);
}

static inline int rislower(int c)  {
    return (c >= 'a' && c <= 'z');
}

static inline int risupper(int c)  {
    return (c >= 'A' && c <= 'Z');
}

static inline int risalpha(int c)  {
    return (rislower(c) || risupper(c));
}

static inline int risdigit(int c)  {
    return (c >= '0' && c <= '9');
}

static inline int risalnum(int c)  {
    return (risalpha(c) || risdigit(c));
}

static int rpmvercmp(const char * a, const char * b)
{
    /* easy comparison to see if versions are identical */
    if (rstreq(a, b)) return 0;

    char oldch1, oldch2;
    char abuf[strlen(a)+1], bbuf[strlen(b)+1];
    char *str1 = abuf, *str2 = bbuf;
    char * one, * two;
    int rc;
    int isnum;

    strcpy(str1, a);
    strcpy(str2, b);

    one = str1;
    two = str2;

    /* loop through each version segment of str1 and str2 and compare them */
    while (*one || *two) {
	while (*one && !risalnum(*one) && *one != '~') one++;
	while (*two && !risalnum(*two) && *two != '~') two++;

	/* handle the tilde separator, it sorts before everthing else */
	if (*one == '~' || *two == '~') {
	    if (*one != '~') return 1;
	    if (*two != '~') return -1;
	    one++;
	    two++;
	    continue;
	}

	/* If we ran to the end of either, we are finished with the loop */
	if (!(*one && *two)) break;

	str1 = one;
	str2 = two;

	/* grab first completely alpha or completely numeric segment */
	/* leave one and two pointing to the start of the alpha or numeric */
	/* segment and walk str1 and str2 to end of segment */
	if (risdigit(*str1)) {
	    while (*str1 && risdigit(*str1)) str1++;
	    while (*str2 && risdigit(*str2)) str2++;
	    isnum = 1;
	} else {
	    while (*str1 && risalpha(*str1)) str1++;
	    while (*str2 && risalpha(*str2)) str2++;
	    isnum = 0;
	}

	/* save character at the end of the alpha or numeric segment */
	/* so that they can be restored after the comparison */
	oldch1 = *str1;
	*str1 = '\0';
	oldch2 = *str2;
	*str2 = '\0';

	/* this cannot happen, as we previously tested to make sure that */
	/* the first string has a non-null segment */
	if (one == str1) return -1;	/* arbitrary */

	/* take care of the case where the two version segments are */
	/* different types: one numeric, the other alpha (i.e. empty) */
	/* numeric segments are always newer than alpha segments */
	/* XXX See patch #60884 (and details) from bugzilla #50977. */
	if (two == str2) return (isnum ? 1 : -1);

	if (isnum) {
	    size_t onelen, twolen;
	    /* this used to be done by converting the digit segments */
	    /* to ints using atoi() - it's changed because long  */
	    /* digit segments can overflow an int - this should fix that. */

	    /* throw away any leading zeros - it's a number, right? */
	    while (*one == '0') one++;
	    while (*two == '0') two++;

	    /* whichever number has more digits wins */
	    onelen = strlen(one);
	    twolen = strlen(two);
	    if (onelen > twolen) return 1;
	    if (twolen > onelen) return -1;
	}

	/* strcmp will return which one is greater - even if the two */
	/* segments are alpha or if they are numeric.  don't return  */
	/* if they are equal because there might be more segments to */
	/* compare */
	rc = strcmp(one, two);
	if (rc) return (rc < 1 ? -1 : 1);

	/* restore character that was replaced by null above */
	*str1 = oldch1;
	one = str1;
	*str2 = oldch2;
	two = str2;
    }

    /* this catches the case where all numeric and alpha segments have */
    /* compared identically but the segment sepparating characters were */
    /* different */
    if ((!*one) && (!*two)) return 0;

    /* whichever version still has characters left over wins */
    if (!*one) return -1; else return 1;
}

typedef struct
{
    char *epoch;
    char *version;
    char *release;
} EVR;

static void parseEVR(char * evr, EVR *evr_parsed)
{
    char *s, *se;

    s = evr;
    while (*s && risdigit(*s)) s++;     /* s points to epoch terminator */
    se = strrchr(s, '-');               /* se points to version terminator */

    if (*s == ':') {
        evr_parsed->epoch = evr;
        *s++ = '\0';
        evr_parsed->version = s;
        if (*(evr_parsed->epoch) == '\0') evr_parsed->epoch = "0";
    } else {
        evr_parsed->epoch = NULL;   /* XXX disable epoch compare if missing */
        evr_parsed->version = evr;
    }
    if (se) {
        *se++ = '\0';
        evr_parsed->release = se;
    } else {
        evr_parsed->release = NULL;
    }
}

static int rpmVersionCompare(EVR *first, EVR *second)
{
    uint32_t epochOne = first->epoch ? atoi(first->epoch) : 0;
    uint32_t epochTwo = second->epoch ? atoi(second->epoch) : 0;

    int rc;

    if (epochOne < epochTwo)
	return -1;
    else if (epochOne > epochTwo)
	return 1;

    rc = rpmvercmp(first->version, second->version);
    if (rc)
	return rc;

    return rpmvercmp(first->release ? first->release : "",
                     second->release ? second->release : "");
}

static void usage(void)
{
    fprintf(stderr, "Usage: rpmvercmp <ver1> lt <ver2>\n");
    fprintf(stderr, "       rpmvercmp <ver1> eq <ver2>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Returns 0 if requested comparison holds, 1 otherwise\n");
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        usage();
        exit(255);
    }

    if (!rstreq(argv[2], "lt") && !rstreq(argv[2], "eq"))
    {
        usage();
        exit(255);
    }

    EVR first, second;

    parseEVR(argv[1], &first);
    parseEVR(argv[3], &second);

    int rc = rpmVersionCompare(&first, &second);

    if (rstreq(argv[2], "lt"))
    {
        exit(rc == -1 ? 0 : 1);
    }
    else
    {
        exit(rc == 0 ? 0 : 1);
    }
}

