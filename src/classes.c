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

/*******************************************************************/
/*                                                                 */
/*  GLOBAL class default variables for cfengine                    */
/*  These variables are what needs to be modified if you add or    */
/*  modify class definitions... remember also to change clsattr    */
/*  and search for the os types in cfengine.c (mount stuff)        */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************************/

                      /* See also "enum classes" in cf.defs.h        */
char *CLASSTEXT[] =             /* If you change here change enum classes too! */
{
    "",
    "<soft>",
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
    "unused1",
    "unused2",
    "unused3",
    NULL
};

/*********************************************************************/

  /* remember to change cf.defs.h !!  */

char *CLASSATTRIBUTES[CF_CLASSATTR][CF_ATTRDIM] =
{
    {"-", "-", "-"},            /* as appear here are matched. The     */
    {"-", "-", "-"},            /* fields are sysname and machine */
    {"hp-ux", ".*", ".*"},      /* hpux */
    {"aix", ".*", ".*"},        /* aix */
    {"linux", ".*", ".*"},      /* linux */
    {"sunos", ".*", "5.*"},     /* solaris */
    {"freebsd", ".*", ".*"},    /* freebsd */
    {"netbsd", ".*", ".*"},     /* NetBSD */
    {"sn.*", "cray*", ".*"},    /* cray */
    {"cygwin_nt.*", ".*", ".*"},        /* NT (cygwin) */
    {"unix_sv", ".*", ".*"},    /* Unixware */
    {"openbsd", ".*", ".*"},    /* OpenBSD */
    {"sco_sv", ".*", ".*"},     /* SCO */
    {"darwin", ".*", ".*"},     /* Darwin, aka MacOS X */
    {"qnx", ".*", ".*"},        /* qnx  */
    {"dragonfly", ".*", ".*"},  /* dragonfly */
    {"windows_nt.*", ".*", ".*"},       /* NT (native) */
    {"vmkernel", ".*", ".*"},   /* VMWARE / ESX */
    {"unused1", "blah", "blah"},
    {"unused2", "blah", "blah"},
    {"unused3", "blah", "blah"},
    {NULL, NULL, NULL}
};

/*********************************************************************/

char *VPSCOMM[CF_CLASSATTR] =
{
    "",
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
    "/bin/ps",
    "/bin/ps",
    "/bin/ps",
    NULL
};

/*********************************************************************/

// linux after rhel 3: ps -eo user,pid,ppid,pgid,%cpu,%mem,vsize,ni,rss,stat,nlwp,stime,time,args
// solaris: ps -eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,nlwp,stime,time,args

char *VPSOPTS[CF_CLASSATTR] =
{
    "",
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
    "-",
    "-",
    "-",
    NULL
};

/*********************************************************************/

char *VMOUNTCOMM[CF_CLASSATTR] =
{
    "",                         /* see cf.defs.h */
    "",
    "/sbin/mount -ea",          /* hpux */
    "/usr/sbin/mount -t nfs",   /* aix */
    "/bin/mount -va",           /* linux */
    "/usr/sbin/mount -a",       /* solaris */
    "/sbin/mount -va",          /* freebsd */
    "/sbin/mount -a",           /* netbsd */
    "/etc/mount -va",           /* cray */
    "/bin/sh /etc/fstab",       /* NT - possible security issue */
    "/sbin/mountall",           /* Unixware */
    "/sbin/mount",              /* openbsd */
    "/etc/mountall",            /* sco */
    "/sbin/mount -va",          /* darwin */
    "/bin/mount -v",            /* qnx */
    "/sbin/mount -va",          /* dragonfly */
    "mingw-invalid",            /* mingw */
    "/bin/mount -a",            /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VUNMOUNTCOMM[CF_CLASSATTR] =
{
    "",                         /* see cf.defs.h */
    "",
    "/sbin/umount",             /* hpux */
    "/usr/sbin/umount",         /* aix */
    "/bin/umount",              /* linux */
    "/etc/umount",              /* solaris */
    "/sbin/umount",             /* freebsd */
    "/sbin/umount",             /* netbsd */
    "/etc/umount",              /* cray */
    "/bin/umount",              /* NT */
    "/sbin/umount",             /* Unixware */
    "/sbin/umount",             /* openbsd */
    "/etc/umount",              /* sco */
    "/sbin/umount",             /* darwin */
    "/bin/umount",              /* qnx */
    "/sbin/umount",             /* dragonfly */
    "mingw-invalid",            /* mingw */
    "/bin/umount",              /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VMOUNTOPTS[CF_CLASSATTR] =
{
    "",                         /* see cf.defs.h */
    "",
    "bg,hard,intr",             /* hpux */
    "bg,hard,intr",             /* aix */
    "defaults",                 /* linux */
    "bg,hard,intr",             /* solaris */
    "bg,intr",                  /* freebsd */
    "-i,-b",                    /* netbsd */
    "bg,hard,intr",             /* cray */
    "",                         /* NT */
    "bg,hard,intr",             /* Unixware */
    "-i,-b",                    /* openbsd */
    "bg,hard,intr",             /* sco */
    "-i,-b",                    /* darwin */
    "bg,hard,intr",             /* qnx */
    "bg,intr",                  /* dragonfly */
    "mingw-invalid",            /* mingw */
    "defaults",                 /* vmstate */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VRESOLVCONF[CF_CLASSATTR] =
{
    "-",
    "-",                        /* see cf.defs.h */
    "/etc/resolv.conf",         /* hpux */
    "/etc/resolv.conf",         /* aix */
    "/etc/resolv.conf",         /* linux */
    "/etc/resolv.conf",         /* solaris */
    "/etc/resolv.conf",         /* freebsd */
    "/etc/resolv.conf",         /* netbsd */
    "/etc/resolv.conf",         /* cray */
    "/etc/resolv.conf",         /* NT */
    "/etc/resolv.conf",         /* Unixware */
    "/etc/resolv.conf",         /* openbsd */
    "/etc/resolv.conf",         /* sco */
    "/etc/resolv.conf",         /* darwin */
    "/etc/resolv.conf",         /* qnx */
    "/etc/resolv.conf",         /* dragonfly */
    "",                         /* mingw */
    "/etc/resolv.conf",         /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VFSTAB[CF_CLASSATTR] =
{
    "-",
    "-",                        /* see cf.defs.h */
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
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VMAILDIR[CF_CLASSATTR] =
{
    "-",
    "-",                        /* see cf.defs.h */
    "/var/mail",                /* hpux */
    "/var/spool/mail",          /* aix */
    "/var/spool/mail",          /* linux */
    "/var/mail",                /* solaris */
    "/var/mail",                /* freebsd */
    "/var/mail",                /* netbsd */
    "/usr/mail",                /* cray */
    "N/A",                      /* NT */
    "/var/mail",                /* Unixware */
    "/var/mail",                /* openbsd */
    "/var/spool/mail",          /* sco */
    "/var/mail",                /* darwin */
    "/var/spool/mail",          /* qnx */
    "/var/mail",                /* dragonfly */
    "",                         /* mingw */
    "/var/spool/mail",          /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VNETSTAT[CF_CLASSATTR] =
{
    "-",
    "-",
    "/usr/bin/netstat -rn",     /* hpux */
    "/usr/bin/netstat -rn",     /* aix */
    "/bin/netstat -rn",         /* linux */
    "/usr/bin/netstat -rn",     /* solaris */
    "/usr/bin/netstat -rn",     /* freebsd */
    "/usr/bin/netstat -rn",     /* netbsd */
    "/usr/ucb/netstat -rn",     /* cray */
    "/cygdrive/c/WINNT/System32/netstat",       /* NT */
    "/usr/bin/netstat -rn",     /* Unixware */
    "/usr/bin/netstat -rn",     /* openbsd */
    "/usr/bin/netstat -rn",     /* sco */
    "/usr/sbin/netstat -rn",    /* darwin */
    "/usr/bin/netstat -rn",     /* qnx */
    "/usr/bin/netstat -rn",     /* dragonfly */
    "mingw-invalid",            /* mingw */
    "/usr/bin/netstat",         /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VEXPORTS[CF_CLASSATTR] =
{
    "-",
    "-",
    "/etc/exports",             /* hpux */
    "/etc/exports",             /* aix */
    "/etc/exports",             /* linux */
    "/etc/dfs/dfstab",          /* solaris */
    "/etc/exports",             /* freebsd */
    "/etc/exports",             /* netbsd */
    "/etc/exports",             /* cray */
    "/etc/exports",             /* NT */
    "/etc/dfs/dfstab",          /* Unixware */
    "/etc/exports",             /* openbsd */
    "/etc/dfs/dfstab",          /* sco */
    "/etc/exports",             /* darwin */
    "/etc/exports",             /* qnx */
    "/etc/exports",             /* dragonfly */
    "",                         /* mingw */
    "none",                     /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VROUTE[CF_CLASSATTR] =
{
    "-",
    "-",
    "-",                        /* hpux */
    "-",                        /* aix */
    "/sbin/route",              /* linux */
    "/usr/sbin/route",          /* solaris */
    "/sbin/route",              /* freebsd */
    "-",                        /* netbsd */
    "-",                        /* cray */
    "-",                        /* NT */
    "-",                        /* Unixware */
    "/sbin/route",              /* openbsd */
    "-",                        /* sco */
    "/sbin/route",              /* darwin */
    "-",                        /* qnx */
    "/sbin/route",              /* dragonfly */
    "-",                        /* mingw */
    "-",                        /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VROUTEADDFMT[CF_CLASSATTR] =
{
    "-",
    "-",
    "-",                        /* hpux */
    "-",                        /* aix */
    "add %s gw %s",             /* linux */
    "add %s %s",                /* solaris */
    "add %s %s",                /* freebsd */
    "-",                        /* netbsd */
    "-",                        /* cray */
    "-",                        /* NT */
    "-",                        /* Unixware */
    "add %s %s",                /* openbsd */
    "-",                        /* sco */
    "add %s %s",                /* darwin */
    "-",                        /* qnx */
    "add %s %s",                /* dragonfly */
    "-",                        /* mingw */
    "-",                        /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};

/*********************************************************************/

char *VROUTEDELFMT[CF_CLASSATTR] =
{
    "-",
    "-",
    "-",                        /* hpux */
    "-",                        /* aix */
    "del %s",                   /* linux */
    "delete %s",                /* solaris */
    "delete %s",                /* freebsd */
    "-",                        /* netbsd */
    "-",                        /* cray */
    "-",                        /* NT */
    "-",                        /* Unixware */
    "delete %s",                /* openbsd */
    "-",                        /* sco */
    "delete %s",                /* darwin */
    "-",                        /* qnx */
    "delete %s",                /* dragonfly */
    "-",                        /* mingw */
    "-",                        /* vmware */
    "unused-blah",
    "unused-blah",
    "unused-blah",
    NULL
};
