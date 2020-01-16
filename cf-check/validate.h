#ifndef CF_CHECK_VALIDATE_H
#define CF_CHECK_VALIDATE_H

// Performs validation on single database file
// Returns 0 for success, errno code otherwise
int CFCheck_Validate(const char *path);

#endif
