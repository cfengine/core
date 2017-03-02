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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_WINDOWS_FUNCTIONS_H
#define CFENGINE_WINDOWS_FUNCTIONS_H

#include <fncall.h>

#ifdef __MINGW32__
/* win_api.c */

int NovaWin_chmod(const char *path, mode_t mode);

/* win_file.c */

int NovaWin_rename(const char *oldpath, const char *newpath);
int NovaWin_FileExists(const char *fileName);
int NovaWin_IsDir(char *fileName);
int NovaWin_TakeFileOwnership(char *path);
int NovaWin_SetFileOwnership(char *path, SID *sid);
off_t NovaWin_GetDiskUsage(char *file, CfSize type);

/* win_log.c */

void OpenLog(int facility);
void CloseLog(void);

void LogToSystemLog(const char *msg, LogLevel level);

/* win_proc.c */

int NovaWin_IsProcessRunning(pid_t pid);
int NovaWin_GetCurrentProcessOwner(SID *sid, int sidSz);
int NovaWin_SetTokenPrivilege(HANDLE token, char *privilegeName, int enablePriv);

/* win_ps.c */

int NovaWin_GetProcessSnapshot(Item **procdata);
int GatherProcessUsers(Item **userList, int *userListSz, int *numRootProcs, int *numOtherProcs);

/* win_service_exec.c */

void NovaWin_StartExecService(void);

/* win_sysinfo.c */

int NovaWin_GetWinDir(char *winDir, int winDirSz);
int NovaWin_GetSysDir(char *sysDir, int sysDirSz);
int NovaWin_GetProgDir(char *progDir, int progDirSz);
int NovaWin_GetEnv(char *varName, char *varContents, int varContentsSz);

const char *GetDefaultWorkDir(void);
const char *GetDefaultLogDir(void);
const char *GetDefaultPidDir(void);
const char *GetDefaultMasterDir(void);
const char *GetDefaultInputDir(void);

/* win_user.c */

int NovaWin_UserNameToSid(char *userName, SID *sid, DWORD sidSz, int shouldExist);
int NovaWin_GroupNameToSid(char *groupName, SID *sid, DWORD sidSz, int shouldExist);
int NovaWin_NameToSid(char *name, SID *sid, DWORD sidSz);
int NovaWin_SidToName(SID *sid, char *name, int nameSz);
int NovaWin_StringToSid(char *stringSid, SID *sid, int sidSz);

FnCallResult FnCallUserExists(EvalContext *ctx, const Policy *policy, const FnCall *fp, const Rlist *finalargs);
FnCallResult FnCallGroupExists(EvalContext *ctx, const Policy *policy, const FnCall *fp, const Rlist *finalargs);

/* win_wmi.c */

int NovaWin_PackageListInstalledFromAPI(EvalContext *ctx, PackageItem ** pkgList, Attributes a, Promise *pp);

/* win_execd_pipe.c */

bool IsReadReady(int fd, int timeout_sec);

/* win_common.c */

void InitializeWindows(void);

#endif /* __MINGW32__ */

#endif // CFENGINE_WINDOWS_FUNCTIONS_H
