/*
  Copyright 2024 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <cf3.defs.h>
#include <known_dirs.h>

#include <prototypes3.h>
#include <syntax.h>
#include <eval_context.h>
#include <file_lib.h>
#include <string_lib.h> // StringCopy()

#include <enterprise_extension.h>

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

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, Nova_Initialize, EvalContext *, ctx)
{
}

/* all agents: generic_agent.c */

ENTERPRISE_FUNC_0ARG_DEFINE_STUB(const char *, GetConsolePrefix)
{
    return "cf3";
}


/* all agents: sysinfo.c */

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, EnterpriseContext, ARG_UNUSED EvalContext *, ctx)
{
}


/* all agents: logging.c */


ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, LogTotalCompliance, const char *, version, int, background_tasks)
{
    double total = (double) (PR_KEPT + PR_NOTKEPT + PR_REPAIRED) / 100.0;

    char string[CF_BUFSIZE] = { 0 };

    snprintf(string, CF_BUFSIZE,
             "Outcome of version %s (" CF_AGENTC "-%d): Promises observed to be kept %.2f%%, Promises repaired %.2f%%, Promises not repaired %.2f%%",
             version, background_tasks,
             (double) PR_KEPT / total,
             (double) PR_REPAIRED / total,
             (double) PR_NOTKEPT / total);

    Log(LOG_LEVEL_VERBOSE, "Logging total compliance, total '%s'", string);

    char filename[CF_BUFSIZE];
    snprintf(filename, CF_BUFSIZE, "%s/%s", GetLogDir(), CF_PROMISE_LOG);
    MapName(filename);

    FILE *fout = safe_fopen(filename, "a");
    if (fout == NULL)
    {
        Log(LOG_LEVEL_ERR, "In total compliance logging, could not open file '%s'. (fopen: %s)", filename, GetErrorStr());
    }
    else
    {
        fprintf(fout, "%jd,%jd: %s\n", (intmax_t)CFSTARTTIME, (intmax_t)time(NULL), string);
        fclose(fout);
    }
}


/* network communication: cf-serverd.c, client_protocol.c, client_code.c, crypto.c */


ENTERPRISE_FUNC_1ARG_DEFINE_STUB(int, CfSessionKeySize, char, type)
{
    return CF_BLOWFISHSIZE;
}

ENTERPRISE_FUNC_0ARG_DEFINE_STUB(char, CfEnterpriseOptions)
{
    return 'c';
}

ENTERPRISE_FUNC_1ARG_DEFINE_STUB(const EVP_CIPHER *, CfengineCipher, char, type)
{
    return EVP_bf_cbc();
}

/* cf-agent: evalfunction.c */

ENTERPRISE_FUNC_6ARG_DEFINE_STUB(char *, GetRemoteScalar, EvalContext *, ctx, char *, proto, char *, handle,
                                 const char *, server, int, encrypted, char *, rcv)
{
    Log(LOG_LEVEL_VERBOSE, "Access to server literals is only available in CFEngine Enterprise");
    return "";
}

ENTERPRISE_VOID_FUNC_3ARG_DEFINE_STUB(void, CacheUnreliableValue, char *, caller, char *, handle, char *, buffer)
{
    Log(LOG_LEVEL_VERBOSE, "Value fault-tolerance only available in CFEngine Enterprise");
}

ENTERPRISE_FUNC_3ARG_DEFINE_STUB(int, RetrieveUnreliableValue, char *, caller, char *, handle, char *, buffer)
{
    Log(LOG_LEVEL_VERBOSE, "Value fault-tolerance only available in CFEngine Enterprise");
    return 0; // enterprise version returns strlen(buffer) or 0 for error
}

#if defined(__MINGW32__)
ENTERPRISE_FUNC_4ARG_DEFINE_STUB(bool, GetRegistryValue, char *, key, char *, name, char *, buf, int, bufSz)
{
    return 0;
}
#endif

ENTERPRISE_FUNC_6ARG_DEFINE_STUB(void *, CfLDAPValue, char *, uri, char *, dn, char *, filter, char *, name, char *, scope, char *, sec)
{
    Log(LOG_LEVEL_ERR, "LDAP support only available in CFEngine Enterprise");
    return NULL;
}

ENTERPRISE_FUNC_6ARG_DEFINE_STUB(void *, CfLDAPList, char *, uri, char *, dn, char *, filter, char *, name, char *, scope, char *, sec)
{
    Log(LOG_LEVEL_ERR, "LDAP support only available in CFEngine Enterprise");
    return NULL;
}

ENTERPRISE_FUNC_8ARG_DEFINE_STUB(void *, CfLDAPArray, EvalContext *, ctx, const Bundle *, caller, char *, array, char *, uri, char *, dn,
                                 char *, filter, char *, scope, char *, sec)
{
    Log(LOG_LEVEL_ERR, "LDAP support only available in CFEngine Enterprise");
    return NULL;
}

ENTERPRISE_FUNC_8ARG_DEFINE_STUB(void *, CfRegLDAP, EvalContext *, ctx, char *, uri, char *, dn, char *, filter, char *, name, char *, scope, char *, regex, char *, sec)
{
    Log(LOG_LEVEL_ERR, "LDAP support only available in CFEngine Enterprise");
    return NULL;
}

ENTERPRISE_FUNC_5ARG_DEFINE_STUB(bool, ListHostsWithField, EvalContext *, ctx, Rlist **, return_list, char *, field_value, char *, return_format, int, function_type)
{
    Log(LOG_LEVEL_ERR, "Host class counting is only available in CFEngine Enterprise");
    return false;
}
/* cf-serverd: server_transform.c, cf-serverd.c */

ENTERPRISE_FUNC_3ARG_DEFINE_STUB(bool, TranslatePath, const char *, from, char *, to, size_t, to_size)
{
    const size_t length = StringCopy(from, to, to_size);
    if (length >= to_size)
    {
        Log(LOG_LEVEL_ERR,
            "File name was too long and got truncated: '%s'",
            to);
        return false;
    }
    return true;
}


ENTERPRISE_VOID_FUNC_3ARG_DEFINE_STUB(void, EvalContextLogPromiseIterationOutcome,
                                      ARG_UNUSED EvalContext *, ctx,
                                      ARG_UNUSED const Promise *, pp,
                                      ARG_UNUSED PromiseResult, result)
{
}

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, CheckAndSetHAState, ARG_UNUSED const char *, workdir, ARG_UNUSED EvalContext *, ctx)
{
}

ENTERPRISE_VOID_FUNC_0ARG_DEFINE_STUB(void, ReloadHAConfig)
{
}

ENTERPRISE_FUNC_0ARG_DEFINE_STUB(size_t, EnterpriseGetMaxCfHubProcesses)
{
    return 0;
}

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, Nova_ClassHistoryAddContextName,
                                      ARG_UNUSED const StringSet *, list,
                                      ARG_UNUSED const char *, context_name)
{
}

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, Nova_ClassHistoryEnable,
                                      ARG_UNUSED StringSet **, list,
                                      ARG_UNUSED bool, enable)
{
}
