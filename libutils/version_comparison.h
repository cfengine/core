#ifndef CF_VERSION_COMPARISON_H
#define CF_VERSION_COMPARISON_H

typedef enum VersionComparison
{
    VERSION_SMALLER,
    VERSION_EQUAL,
    VERSION_GREATER,
    VERSION_ERROR,
} VersionComparison;

VersionComparison CompareVersion(const char *a, const char *b);

#endif
