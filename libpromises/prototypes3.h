/*
   Copyright 2017 Northern.tech AS

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_PROTOTYPES3_H
#define CFENGINE_PROTOTYPES3_H

#include <cf3.defs.h>
#include <compiler.h>
#include <enterprise_extension.h>

bool BootstrapAllowed(void);

/* Versions */

const char *Version(void);
const char *NameVersion(void);

/* cfparse.y */

void yyerror(const char *s);

/* agent.c */

PromiseResult ScheduleAgentOperations(EvalContext *ctx, const Bundle *bp);

/* Only for agent.c */

void ConnectionsInit(void);
void ConnectionsCleanup(void);

/* client_protocol.c */

void SetSkipIdentify(bool enabled);

/* enterprise_stubs.c */

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, Nova_Initialize, EvalContext *, ctx);
ENTERPRISE_FUNC_1ARG_DECLARE(int, CfSessionKeySize, char, c);
ENTERPRISE_FUNC_0ARG_DECLARE(char, CfEnterpriseOptions);
ENTERPRISE_FUNC_1ARG_DECLARE(const EVP_CIPHER *, CfengineCipher, char, type);
ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, EnterpriseContext, EvalContext *, ctx);
ENTERPRISE_FUNC_0ARG_DECLARE(const char *, GetConsolePrefix);
ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, LoadSlowlyVaryingObservations, EvalContext *, ctx);
ENTERPRISE_FUNC_6ARG_DECLARE(char *, GetRemoteScalar, EvalContext *, ctx, char *, proto, char *, handle, char *, server, int, encrypted, char *, rcv);
ENTERPRISE_FUNC_1ARG_DECLARE(const char *, PromiseID, const Promise *, pp);     /* Not thread-safe */
ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, LogTotalCompliance, const char *, version, int, background_tasks);
#if defined(__MINGW32__)
ENTERPRISE_FUNC_4ARG_DECLARE(int, GetRegistryValue, const char *, key, char *, name, char *, buf, int, bufSz);
#endif
ENTERPRISE_FUNC_6ARG_DECLARE(void *, CfLDAPValue, char *, uri, char *, dn, char *, filter, char *, name, char *, scope, char *, sec);
ENTERPRISE_FUNC_6ARG_DECLARE(void *, CfLDAPList, char *, uri, char *, dn, char *, filter, char *, name, char *, scope, char *, sec);
ENTERPRISE_FUNC_8ARG_DECLARE(void *, CfLDAPArray, EvalContext *, ctx, const Bundle *, caller, char *, array, char *, uri, char *, dn, char *, filter, char *, scope, char *, sec);
ENTERPRISE_FUNC_8ARG_DECLARE(void *, CfRegLDAP, EvalContext *, ctx, char *, uri, char *, dn, char *, filter, char *, name, char *, scope, char *, regex, char *, sec);
ENTERPRISE_VOID_FUNC_3ARG_DECLARE(void, CacheUnreliableValue, char *, caller, char *, handle, char *, buffer);
ENTERPRISE_FUNC_3ARG_DECLARE(int, RetrieveUnreliableValue, char *, caller, char *, handle, char *, buffer);
ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, TranslatePath, char *, new, const char *, old);
ENTERPRISE_VOID_FUNC_4ARG_DECLARE(void, TrackValue, char *, date, double, kept, double, repaired, double, notkept);
ENTERPRISE_FUNC_4ARG_DECLARE(bool, ListHostsWithClass, EvalContext *, ctx, Rlist **, return_list, char *, class_name, char *, return_format);

ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, ShowPromises, const Seq *, bundles, const Seq *, bodies);
ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, ShowPromise, const Promise *, pp);

ENTERPRISE_VOID_FUNC_3ARG_DECLARE(void, GetObservable, int, i, char *, name, char *, desc);
ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, SetMeasurementPromises, Item **, classlist);

ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, CheckAndSetHAState, const char *, workdir, EvalContext *, ctx);
ENTERPRISE_VOID_FUNC_0ARG_DECLARE(void, ReloadHAConfig);

/* manual.c */

void TexinfoManual(EvalContext *ctx, const char *source_dir, const char *output_file);

/* modes.c */

int ParseModeString(const char *modestring, mode_t *plusmask, mode_t *minusmask);

/* patches.c */

int IsPrivileged(void);
char *cf_strtimestamp_local(const time_t time, char *buf);
char *cf_strtimestamp_utc(const time_t time, char *buf);
int cf_closesocket(int sd);

#if !defined(__MINGW32__)
#define OpenNetwork() /* noop */
#define CloseNetwork() /* noop */
#else
void OpenNetwork(void);
void CloseNetwork(void);
#endif

int LinkOrCopy(const char *from, const char *to, int sym);
int ExclusiveLockFile(int fd);
int ExclusiveUnlockFile(int fd);

/* storage_tools.c */

off_t GetDiskUsage(char *file, CfSize type);

/* verify_reports.c */

PromiseResult VerifyReportPromise(EvalContext *ctx, const Promise *pp);

/* cf-key */

ENTERPRISE_FUNC_1ARG_DECLARE(bool, LicenseInstall, char *, path_source);

/* cf-serverd */

ENTERPRISE_FUNC_0ARG_DECLARE(size_t, EnterpriseGetMaxCfHubProcesses);

#endif
