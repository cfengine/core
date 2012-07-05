/*****************************************************************************/
/*                                                                           */
/* File: server_globals.c                                                    */
/*                                                                           */
/* Created: Sat Jun 30 19:19:41 2012                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"
#include "cf3.server.h"

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

int CLOCK_DRIFT = 3600;  /* 1hr */
int ACTIVE_THREADS;

int CFD_MAXPROCESSES = 0;
int CFD_INTERVAL = 0;
int DENYBADCLOCKS = true;
int TRIES = 0;
int MAXTRIES = 5;
int LOGCONNS = false;
int LOGENCRYPT = false;
int COLLECT_INTERVAL = 0;

Auth *ROLES = NULL;
Auth *ROLESTOP = NULL;

ServerAccess SV;

Auth *VADMIT = NULL;
Auth *VADMITTOP = NULL;
Auth *VDENY = NULL;
Auth *VDENYTOP = NULL;

Auth *VARADMIT = NULL;
Auth *VARADMITTOP = NULL;
Auth *VARDENY = NULL;
Auth *VARDENYTOP = NULL;

char CFRUNCOMMAND[CF_BUFSIZE];

// These strings should all fit inside 11 characters CF_PROTO_OFFSET - 4 - 1

const char *PROTOCOL[] =
{
    "EXEC",
    "AUTH",                     /* old protocol */
    "GET",
    "OPENDIR",
    "SYNCH",
    "CLASSES",
    "MD5",
    "SMD5",
    "CAUTH",
    "SAUTH",
    "SSYNCH",
    "SGET",
    "VERSION",
    "SOPENDIR",
    "VAR",
    "SVAR",
    "CONTEXT",
    "SCONTEXT",
    "SQUERY",
    "SCALLBACK",
    NULL
};
