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

#include <systype.h>
#if defined __FreeBSD__
#include <sys/param.h>
#endif

/* Set in libenv/sysinfo.c::DetectEnvironment (called every time environment
   reload is performed).

   Utilized all over the place, usually to look up OS-specific command/option to
   call external utility
*/
PlatformContext VSYSTEMHARDCLASS; /* GLOBAL_E?, initialized_later */
PlatformContext VPSHARDCLASS; /* used to define which ps command to use*/


/* Configure system name and system-specific details. */

const char *const CLASSTEXT[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = "<unknown>",
    [PLATFORM_CONTEXT_OPENVZ] = "virt_host_vz_vzps",
    [PLATFORM_CONTEXT_HP] = "hpux",
    [PLATFORM_CONTEXT_AIX] = "aix",
    [PLATFORM_CONTEXT_LINUX] = "linux",
    [PLATFORM_CONTEXT_BUSYBOX] = "linux",
    [PLATFORM_CONTEXT_SOLARIS] = "solaris",
    [PLATFORM_CONTEXT_SUN_SOLARIS] = "solaris",
    [PLATFORM_CONTEXT_FREEBSD] = "freebsd",
    [PLATFORM_CONTEXT_NETBSD] = "netbsd",
    [PLATFORM_CONTEXT_CRAYOS] = "cray",
    [PLATFORM_CONTEXT_WINDOWS_NT] = "windows",
    [PLATFORM_CONTEXT_SYSTEMV] = "unix_sv",
    [PLATFORM_CONTEXT_OPENBSD] = "openbsd",
    [PLATFORM_CONTEXT_CFSCO] = "sco",
    [PLATFORM_CONTEXT_DARWIN] = "darwin",
    [PLATFORM_CONTEXT_QNX] = "qnx",
    [PLATFORM_CONTEXT_DRAGONFLY] = "dragonfly",
    [PLATFORM_CONTEXT_MINGW] = "windows",
    [PLATFORM_CONTEXT_VMWARE] = "vmware",
    [PLATFORM_CONTEXT_ANDROID] = "android",
};

const char *const VPSCOMM[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = "",
    [PLATFORM_CONTEXT_OPENVZ] = "/bin/vzps",                /* virt_host_vz_vzps */
    [PLATFORM_CONTEXT_HP] = "/bin/ps",                      /* hpux */
    [PLATFORM_CONTEXT_AIX] = "/bin/ps",                     /* aix */
    [PLATFORM_CONTEXT_LINUX] = "/bin/ps",                   /* linux */
    [PLATFORM_CONTEXT_BUSYBOX] = "/bin/ps",                 /* linux */
    [PLATFORM_CONTEXT_SOLARIS] = "/bin/ps",                 /* solaris >= 11 */
    [PLATFORM_CONTEXT_SUN_SOLARIS] = "/usr/ucb/ps",         /* solaris  < 11 */
    [PLATFORM_CONTEXT_FREEBSD] = "/bin/ps",                 /* freebsd */
    [PLATFORM_CONTEXT_NETBSD] = "/bin/ps",                  /* netbsd */
    [PLATFORM_CONTEXT_CRAYOS] = "/bin/ps",                  /* cray */
    [PLATFORM_CONTEXT_WINDOWS_NT] = "/bin/ps",              /* NT - cygnus */
    [PLATFORM_CONTEXT_SYSTEMV] = "/bin/ps",                 /* unixware */
    [PLATFORM_CONTEXT_OPENBSD] = "/bin/ps",                 /* openbsd */
    [PLATFORM_CONTEXT_CFSCO] = "/bin/ps",                   /* sco */
    [PLATFORM_CONTEXT_DARWIN] = "/bin/ps",                  /* darwin */
    [PLATFORM_CONTEXT_QNX] = "/bin/ps",                     /* qnx  */
    [PLATFORM_CONTEXT_DRAGONFLY] = "/bin/ps",               /* dragonfly */
    [PLATFORM_CONTEXT_MINGW] = "mingw-invalid",             /* mingw */
    [PLATFORM_CONTEXT_VMWARE] = "/bin/ps",                  /* vmware */
    [PLATFORM_CONTEXT_ANDROID] = "/system/xbin/busybox ps", /* android */
};

// linux after rhel 3: ps -eo user,pid,ppid,pgid,%cpu,%mem,vsize,ni,rss,stat,nlwp,stime,time,args
// solaris: ps -eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,nlwp,stime,time,args

const char *const VPSOPTS[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = "",
    [PLATFORM_CONTEXT_OPENVZ] = "-E 0 -o user,pid,ppid,pgid,pcpu,pmem,vsz,ni,rss,thcount,stime,time,args",   /* virt_host_vz_vzps (with vzps, the -E 0 replace the -e) */
    [PLATFORM_CONTEXT_HP] = "-ef",                    /* hpux */
    [PLATFORM_CONTEXT_AIX] =  "-N -eo user,pid,ppid,pgid,pcpu,pmem,vsz,ni,stat,st=STIME,time,args",       /* aix */
    /* Note: keep in sync with GetProcessOptions()'s hack for Linux 2.4 */
    [PLATFORM_CONTEXT_LINUX] = "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,ni,rss:9,nlwp,stime,etime,time,args",/* linux */
    [PLATFORM_CONTEXT_BUSYBOX] = "",                  /* linux / busybox */
    [PLATFORM_CONTEXT_SOLARIS] = "auxww",     /* solaris >= 11 */
    [PLATFORM_CONTEXT_SUN_SOLARIS] = "auxww", /* solaris < 11 */
#if __FreeBSD_version >= 903000
    [PLATFORM_CONTEXT_FREEBSD] = "auxw -J 0",              /* freebsd 9.3 and newer */
#else
    [PLATFORM_CONTEXT_FREEBSD] = "auxw",              /* freebsd 9.2 and older*/
#endif
    [PLATFORM_CONTEXT_NETBSD] = "-axo user,pid,ppid,pgid,pcpu,pmem,vsz,ni,rss,nlwp,start,time,args",   /* netbsd */
    [PLATFORM_CONTEXT_CRAYOS] = "-elyf",              /* cray */
    [PLATFORM_CONTEXT_WINDOWS_NT] = "-aW",            /* NT */
    [PLATFORM_CONTEXT_SYSTEMV] = "-ef",               /* Unixware */
    [PLATFORM_CONTEXT_OPENBSD] = "-axo user,pid,ppid,pgid,pcpu,pmem,vsz,ni,rss,start,time,args",       /* openbsd */
    [PLATFORM_CONTEXT_CFSCO] = "-ef",                 /* sco */
    [PLATFORM_CONTEXT_DARWIN] = "auxw",               /* darwin */
    [PLATFORM_CONTEXT_QNX] = "-elyf",                 /* qnx */
    [PLATFORM_CONTEXT_DRAGONFLY] = "auxw",            /* dragonfly */
    [PLATFORM_CONTEXT_MINGW] = "mingw-invalid",       /* mingw */
    [PLATFORM_CONTEXT_VMWARE] = "?",                  /* vmware */
    [PLATFORM_CONTEXT_ANDROID] = "",                  /* android */
};

const char *const VFSTAB[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = "-",
    [PLATFORM_CONTEXT_OPENVZ] = "/etc/fstab",         /* virt_host_vz_vzps */
    [PLATFORM_CONTEXT_HP] = "/etc/fstab",             /* hpux */
    [PLATFORM_CONTEXT_AIX] = "/etc/filesystems",      /* aix */
    [PLATFORM_CONTEXT_LINUX] = "/etc/fstab",          /* linux */
    [PLATFORM_CONTEXT_BUSYBOX] = "/etc/fstab",        /* linux */
    [PLATFORM_CONTEXT_SOLARIS] = "/etc/vfstab",       /* solaris */
    [PLATFORM_CONTEXT_SUN_SOLARIS] = "/etc/vfstab",   /* solaris */
    [PLATFORM_CONTEXT_FREEBSD] = "/etc/fstab",        /* freebsd */
    [PLATFORM_CONTEXT_NETBSD] = "/etc/fstab",         /* netbsd */
    [PLATFORM_CONTEXT_CRAYOS] = "/etc/fstab",         /* cray */
    [PLATFORM_CONTEXT_WINDOWS_NT] = "/etc/fstab",     /* NT */
    [PLATFORM_CONTEXT_SYSTEMV] = "/etc/vfstab",       /* Unixware */
    [PLATFORM_CONTEXT_OPENBSD] = "/etc/fstab",        /* openbsd */
    [PLATFORM_CONTEXT_CFSCO] = "/etc/default/filesys",/* sco */
    [PLATFORM_CONTEXT_DARWIN] = "/etc/fstab",         /* darwin */
    [PLATFORM_CONTEXT_QNX] = "/etc/fstab",            /* qnx */
    [PLATFORM_CONTEXT_DRAGONFLY] = "/etc/fstab",      /* dragonfly */
    [PLATFORM_CONTEXT_MINGW] = "",                    /* mingw */
    [PLATFORM_CONTEXT_VMWARE] = "/etc/fstab",         /* vmware */
    [PLATFORM_CONTEXT_ANDROID] = "",                  /* android */
};
