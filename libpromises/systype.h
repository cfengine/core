/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_SYSTYPE_H
#define CFENGINE_SYSTYPE_H

/*******************************************************************/

typedef enum
{
    PLATFORM_CONTEXT_UNKNOWN,
    PLATFORM_CONTEXT_OPENVZ, /* VZ Host with vzps installed */
    PLATFORM_CONTEXT_HP,
    PLATFORM_CONTEXT_AIX,
    PLATFORM_CONTEXT_LINUX,
    PLATFORM_CONTEXT_BUSYBOX, /* Linux-based Busybox toolset */
    PLATFORM_CONTEXT_SOLARIS, /* > 5.10, BSD-compatible system tools */
    PLATFORM_CONTEXT_SUN_SOLARIS, /* < 5.11, BSD tools in /usr/ucb */
    PLATFORM_CONTEXT_FREEBSD,
    PLATFORM_CONTEXT_NETBSD,
    PLATFORM_CONTEXT_CRAYOS,
    PLATFORM_CONTEXT_WINDOWS_NT, /* MS-Win CygWin */
    PLATFORM_CONTEXT_SYSTEMV,
    PLATFORM_CONTEXT_OPENBSD,
    PLATFORM_CONTEXT_CFSCO,
    PLATFORM_CONTEXT_DARWIN, /* MacOS X */
    PLATFORM_CONTEXT_QNX,
    PLATFORM_CONTEXT_DRAGONFLY,
    PLATFORM_CONTEXT_MINGW, /* MS-Win native */
    PLATFORM_CONTEXT_VMWARE,
    PLATFORM_CONTEXT_ANDROID,

    PLATFORM_CONTEXT_MAX /* Not an actual platform: must be last */
} PlatformContext;

/*******************************************************************/

extern PlatformContext VSYSTEMHARDCLASS;
extern PlatformContext VPSHARDCLASS; /* used to define which ps command to use*/
extern const char *const CLASSTEXT[PLATFORM_CONTEXT_MAX];
extern const char *const VPSCOMM[PLATFORM_CONTEXT_MAX];
extern const char *const VPSOPTS[PLATFORM_CONTEXT_MAX];
extern const char *const VFSTAB[PLATFORM_CONTEXT_MAX];

/*******************************************************************/

#endif /* CFENGINE_SYSTYPE_H */
