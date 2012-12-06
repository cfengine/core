#ifndef CFENGINE_MATCHING_H
#define CFENGINE_MATCHING_H

#include "cf3.defs.h"

bool ValidateRegEx(const char *regex);
int FullTextMatch(const char *regptr, const char *cmpptr);
char *ExtractFirstReference(const char *regexp, const char *teststring);        /* Not thread-safe */
int BlockTextMatch(const char *regexp, const char *teststring, int *s, int *e);
int IsRegexItemIn(Item *list, char *regex);
int IsPathRegex(char *str);
int IsRegex(char *str);
int MatchRlistItem(Rlist *listofregex, const char *teststring);
void EscapeRegexChars(char *str, char *strEsc, int strEscSz);
void EscapeSpecialChars(char *str, char *strEsc, int strEscSz, char *noEscseq, char *noEsclist);
char *EscapeChar(char *str, int strSz, char esc);
void AnchorRegex(const char *regex, char *out, int outSz);
int MatchPolicy(const char *needle, const char *haystack, Attributes a, const Promise *pp);

#endif // MATCHING_H
