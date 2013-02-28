/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "cf3.defs.h"

const char *CLASSTEXT[PLATFORM_CONTEXT_MAX] =
{
    "<unknown>",
    "hpux",
    "aix",
    "linux",
    "solaris",
    "freebsd",
    "netbsd",
    "cray",
    "windows",
    "unix_sv",
    "openbsd",
    "sco",
    "darwin",
    "qnx",
    "dragonfly",
    "windows",
    "vmware",
};

const char *VPSCOMM[PLATFORM_CONTEXT_MAX] =
{
    "",
    "/bin/ps",                  /* hpux */
    "/bin/ps",                  /* aix */
    "/bin/ps",                  /* linux */
    "/bin/ps",                  /* solaris */
    "/bin/ps",                  /* freebsd */
    "/bin/ps",                  /* netbsd */
    "/bin/ps",                  /* cray */
    "/bin/ps",                  /* NT - cygnus */
    "/bin/ps",                  /* unixware */
    "/bin/ps",                  /* openbsd */
    "/bin/ps",                  /* sco */
    "/bin/ps",                  /* darwin */
    "/bin/ps",                  /* qnx  */
    "/bin/ps",                  /* dragonfly */
    "mingw-invalid",            /* mingw */
    "/bin/ps",                  /* vmware */
};

// linux after rhel 3: ps -eo user,pid,ppid,pgid,%cpu,%mem,vsize,ni,rss,stat,nlwp,stime,time,args
// solaris: ps -eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,nlwp,stime,time,args

const char *VPSOPTS[PLATFORM_CONTEXT_MAX] =
{
    "",
    "-ef",                      /* hpux */
    "-N -eo user,pid,ppid,pgid,pcpu,pmem,vsz,ni,stat,st=STIME,time,args",  /* aix */
    "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,nlwp,stime,time,args",        /* linux */
    "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,nlwp,stime,time,args",        /* solaris */
    "auxw",                     /* freebsd */
    "auxw",                     /* netbsd */
    "-elyf",                    /* cray */
    "-aW",                      /* NT */
    "-ef",                      /* Unixware */
    "auxw",                     /* openbsd */
    "-ef",                      /* sco */
    "auxw",                     /* darwin */
    "-elyf",                    /* qnx */
    "auxw",                     /* dragonfly */
    "mingw-invalid",            /* mingw */
    "?",                        /* vmware */
};

const char *VFSTAB[PLATFORM_CONTEXT_MAX] =
{
    "-",
    "/etc/fstab",               /* hpux */
    "/etc/filesystems",         /* aix */
    "/etc/fstab",               /* linux */
    "/etc/vfstab",              /* solaris */
    "/etc/fstab",               /* freebsd */
    "/etc/fstab",               /* netbsd */
    "/etc/fstab",               /* cray */
    "/etc/fstab",               /* NT */
    "/etc/vfstab",              /* Unixware */
    "/etc/fstab",               /* openbsd */
    "/etc/default/filesys",     /* sco */
    "/etc/fstab",               /* darwin */
    "/etc/fstab",               /* qnx */
    "/etc/fstab",               /* dragonfly */
    "",                         /* mingw */
    "/etc/fstab",               /* vmware */
};

