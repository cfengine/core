/*****************************************************************************/
/*                                                                           */
/* File: server_globals.h                                                    */
/*                                                                           */
/* Created: Sat Jun 30 19:20:15 2012                                         */
/*                                                                           */
/*****************************************************************************/

#ifndef SERVER_GLOBALS
#define SERVER_GLOBALS 1

extern int CLOCK_DRIFT;
extern int ACTIVE_THREADS;

extern int CFD_MAXPROCESSES;
extern int CFD_INTERVAL;
extern int DENYBADCLOCKS;
extern int TRIES;
extern int MAXTRIES;
extern int LOGCONNS;
extern int LOGENCRYPT;
extern int COLLECT_INTERVAL;

extern Auth *ROLES;
extern Auth *ROLESTOP;

extern ServerAccess SV;

extern Auth *VADMIT;
extern Auth *VADMITTOP;
extern Auth *VDENY;
extern Auth *VDENYTOP;

extern Auth *VARADMIT;
extern Auth *VARADMITTOP;
extern Auth *VARDENY;
extern Auth *VARDENYTOP;

extern char CFRUNCOMMAND[];
extern const char *PROTOCOL[];

#endif
