/*
 * Obtain the extended args column for the process number given as a
 * string as process.
 *
 * Under Solaris /bin/ps the COMMAND column is limited to 80 characters.
 * This function overcomes that limitation by using code taken from
 * UCB ps.
 *
 *
 * @return 0 on success or -1 otherwise.
 */
# ifndef PS_SOLARIS_H
# define PS_SOLARIS_H
int GetPsArgs(const char *process, char *args);
#endif
