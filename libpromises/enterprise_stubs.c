/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "cf3.defs.h"

#include "prototypes3.h"
#include "syntax.h"

/*
 * This module contains numeruous functions which don't use all their parameters
 *
 * Temporarily, in order to avoid cluttering output with thousands of warnings,
 * this module is excempted from producing warnings about unused function
 * parameters.
 *
 * Please remove this #pragma ASAP and provide ARG_UNUSED declarations for
 * unused parameters.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

/* all agents: generic_agent.c */

const char *GetConsolePrefix(void)
{
    return "cf3";
}


/* all agents: sysinfo.c */

void EnterpriseContext(ARG_UNUSED EvalContext *ctx)
{
}

void LoadSlowlyVaryingObservations(EvalContext *ctx)
{
    Log(LOG_LEVEL_VERBOSE, "# Extended system discovery is only available in CFEngine Enterprise");
}

/* all agents: cfstream.c, expand.c, generic_agent.c */


const char *PromiseID(ARG_UNUSED const Promise *pp)
{
    return "";
}


/* all agents: logging.c */


void NotePromiseCompliance(ARG_UNUSED const Promise *pp, ARG_UNUSED PromiseState state, ARG_UNUSED const char *reason)
{
}

void TrackValue(char *date, double kept, double repaired, double notkept)
{
}

void LogTotalCompliance(const char *version, int background_tasks)
{
    double total = (double) (PR_KEPT + PR_NOTKEPT + PR_REPAIRED) / 100.0;

    char string[CF_BUFSIZE] = { 0 };

    snprintf(string, CF_BUFSIZE,
             "Outcome of version %s (" CF_AGENTC "-%d): Promises observed to be kept %.0f%%, Promises repaired %.0f%%, Promises not repaired %.0f\%%",
             version, background_tasks,
             (double) PR_KEPT / total,
             (double) PR_REPAIRED / total,
             (double) PR_NOTKEPT / total);

    Log(LOG_LEVEL_VERBOSE, "Total: %s", string);

    char filename[CF_BUFSIZE];
    snprintf(filename, CF_BUFSIZE, "%s/%s", CFWORKDIR, CF_PROMISE_LOG);
    MapName(filename);

    FILE *fout = fopen(filename, "a");
    if (fout == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not open '%s'. (fopen: %s)", filename, GetErrorStr());
    }
    else
    {
        fprintf(fout, "%" PRIdMAX ",%" PRIdMAX ": %s\n", (intmax_t)CFSTARTTIME, (intmax_t)time(NULL), string);
        fclose(fout);
    }
}


/* network communication: cf-serverd.c, client_protocol.c, client_code.c, crypto.c */


int CfSessionKeySize(char type)
{
    return CF_BLOWFISHSIZE;
}

char CfEnterpriseOptions(void)
{
    return 'c';
}

const EVP_CIPHER *CfengineCipher(char type)
{
    return EVP_bf_cbc();
}

/* cf-agent: evalfunction.c */

char *GetRemoteScalar(EvalContext *ctx, char *proto, char *handle, char *server, int encrypted, char *rcv)
{
    Log(LOG_LEVEL_VERBOSE, "# Access to server literals is only available in CFEngine Enterprise");
    return "";
}

void CacheUnreliableValue(char *caller, char *handle, char *buffer)
{
    Log(LOG_LEVEL_VERBOSE, "# Value fault-tolerance only available in CFEngine Enterprise");
}

int RetrieveUnreliableValue(char *caller, char *handle, char *buffer)
{
    Log(LOG_LEVEL_VERBOSE, "# Value fault-tolerance only available in CFEngine Enterprise");
    return false;
}

#if defined(__MINGW32__)
int GetRegistryValue(char *key, char *name, char *buf, int bufSz)
{
    return 0;
}
#endif

void *CfLDAPValue(char *uri, char *dn, char *filter, char *name, char *scope, char *sec)
{
    Log(LOG_LEVEL_ERR, "LDAP support only available in CFEngine Enterprise");
    return NULL;
}

void *CfLDAPList(char *uri, char *dn, char *filter, char *name, char *scope, char *sec)
{
    Log(LOG_LEVEL_ERR, "LDAP support only available in CFEngine Enterprise");
    return NULL;
}

void *CfLDAPArray(EvalContext *ctx, const Bundle *caller, char *array, char *uri, char *dn, char *filter, char *scope, char *sec)
{
    Log(LOG_LEVEL_ERR, "LDAP support only available in CFEngine Enterprise");
    return NULL;
}

void *CfRegLDAP(char *uri, char *dn, char *filter, char *name, char *scope, char *regex, char *sec)
{
    Log(LOG_LEVEL_ERR, "LDAP support only available in CFEngine Enterprise");
    return NULL;
}

bool CFDB_HostsWithClass(EvalContext *ctx, Rlist **return_list, char *class_name, char *return_format)
{
    Log(LOG_LEVEL_ERR, "Host class counting is only available in CFEngine Enterprise");
    return false;
}

/* cf-serverd: server_transform.c, cf-serverd.c */

void TranslatePath(char *new, const char *old)
{
    strncpy(new, old, CF_BUFSIZE - 1);
}


void ShowPromises(ARG_UNUSED const Seq *bundles, ARG_UNUSED const Seq *bodies)
{
}

void ShowPromise(ARG_UNUSED const Promise *pp)
{
}
