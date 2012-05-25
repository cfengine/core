#ifndef CFENGINE_FILES_NAMES_H
#define CFENGINE_FILES_NAMES_H

#include "cf.defs.h"

int IsNewerFileTree(char *dir, time_t reftime);
int DeEscapeQuotedString(const char *in, char *out);
int CompareCSVName(const char *s1, const char *s2);
int IsDir(char *path);
int EmptyString(char *s);
char *JoinPath(char *path, const char *leaf);
char *JoinSuffix(char *path, char *leaf);
int JoinMargin(char *path, const char *leaf, char **nextFree, int bufsize, int margin);
int StartJoin(char *path, char *leaf, int bufsize);
int Join(char *path, const char *leaf, int bufsize);
int JoinSilent(char *path, const char *leaf, int bufsize);
int EndJoin(char *path, char *leaf, int bufsize);
int IsAbsPath(char *path);
void AddSlash(char *str);
void DeleteSlash(char *str);
const char *LastFileSeparator(const char *str);
int ChopLastNode(char *str);
char *CanonifyName(const char *str);
void CanonifyNameInPlace(char *str);
char *CanonifyChar(const char *str, char ch);
const char *ReadLastNode(const char *str);
int CompressPath(char *dest, char *src);
void Chop(char *str);
void StripTrailingNewline(char *str);
char *ScanPastChars(char *scanpast, char *input);
int IsAbsoluteFileName(const char *f);
bool IsFileOutsideDefaultRepository(const char *f);
int RootDirLength(char *f);
const char *GetSoftwareCacheFilename(char *buffer);

#endif
