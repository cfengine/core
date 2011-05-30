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
char *CLASSTEXT[] =   /* If you change here change enum classes too! */
   {
   "",
   "<soft>",
   "sun4",
   "ultrix",
   "hpux",
   "aix",
   "linux",
   "solaris",
   "osf",
   "digital",
   "sun3",
   "irix4",
   "irix",
   "irix64",
   "freebsd",
   "solarisx86",
   "bsd4_3",
   "newsos",
   "netbsd",
   "aos",
   "bsdos",
   "nextstep",
   "cray",
   "gnu",
   "windows",
   "unix_sv",
   "openbsd",
   "sco",
   "darwin",
   "ux4800",
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
   {"-","-","-"},                  /* as appear here are matched. The     */
   {"-","-","-"},                  /* fields are sysname and machine */
   {"sunos",".*","4.*"},           /* sun 4  */
   {"ultrix","risc","4.*"},        /* ultrix */
   {"hp-ux",".*",".*"},            /* hpux */
   {"aix",".*",".*"},              /* aix */
   {"linux",".*",".*"},            /* linux */
   {"sunos","sun4.*","5.*"},       /* solaris */
   {"osf1","alpha",".*"},          /* osf1 */
   {"osf1","alpha","4.*"},         /* digital */   
   {"sunos","sun3","4.*"},         /* sun3 */
   {"irix4","ip.*","4.*"},         /* irix4 */
   {"irix", "ip.*",".*"},          /* irix */
   {"irix64","ip.*",".*"},         /* irix64 */
   {"freebsd",".*",".*"},          /* freebsd */
   {"sunos","i86pc","5.*"},        /* solarisx86 */
   {"bsd",".*",".*"},              /* bsd 4.3 */
   {"newsos",".*",".*"},           /* newsos4 */
   {"netbsd",".*",".*"},           /* NetBSD */
   {"aos",".*",".*"},              /* AOS */
   {"bsd/os",".*",".*"},           /* BSDI */
   {"nextstep",".*",".*"},         /* nextstep */
   {"sn.*","cray*",".*"},           /* cray */
   {"gnu.*",".*",".*"},             /* gnu */
   {"cygwin_nt.*",".*",".*"},       /* NT (cygwin) */
   {"unix_sv",".*",".*"},          /* Unixware */
   {"openbsd",".*",".*"},          /* OpenBSD */
   {"sco_sv",".*",".*"},           /* SCO */
   {"darwin",".*",".*"},           /* Darwin, aka MacOS X */
   {"ux4800",".*",".*"},           /* UX/4800 */
   {"qnx",".*",".*"},              /* qnx  */
   {"dragonfly",".*",".*"},        /* dragonfly */
   {"windows_nt.*",".*",".*"},     /* NT (native) */
   {"vmkernel",".*",".*"},            /* VMWARE / ESX */
   {"unused1","blah","blah"},
   {"unused2","blah","blah"},
   {"unused3","blah","blah"},
   {NULL,NULL,NULL}
   };

/*********************************************************************/

char *VPSCOMM[CF_CLASSATTR] =
   {
   "",
   "",
   "/bin/ps",       /* sun 4  */
   "/bin/ps",       /* ultrix */
   "/bin/ps",       /* hpux */
   "/bin/ps",       /* aix */
   "/bin/ps",       /* linux */
   "/bin/ps",       /* solaris */
   "/bin/ps",       /* osf1 */
   "/bin/ps",       /* digital */   
   "/bin/ps",       /* sun3 */
   "/bin/ps",       /* irix4 */
   "/bin/ps",       /* irix */
   "/bin/ps",       /* irix64 */
   "/bin/ps",       /* freebsd */
   "/bin/ps",       /* solarisx86 */
   "/bin/ps",       /* bsd 4.3 */
   "/bin/ps",       /* newos4 */
   "/bin/ps",       /* netbsd */
   "/bin/ps",       /* AOS */
   "/bin/ps",       /* BSDI */
   "/bin/ps",       /* nextstep */
   "/bin/ps",       /* cray */
   "/bin/ps",       /* gnu */
   "/bin/ps",       /* NT - cygnus */
   "/bin/ps",       /* unixware */
   "/bin/ps",       /* openbsd */
   "/bin/ps",       /* sco */
   "/bin/ps",       /* darwin */
   "/bin/ps",       /* ux4800 */
   "/bin/ps",       /* qnx  */
   "/bin/ps",       /* dragonfly */
   "mingw-invalid", /* mingw */
   "/bin/ps",       /* vmware */
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
   "auxw",   /* sun4 */
   "auxw",   /* ultrix */
   "-ef",    /* hpux */
   "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,ni,stat,stime,time,args",    /* aix */
   "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,nlwp,stime,time,args",   /* linux */
   "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,nlwp,stime,time,args",   /* solaris */
   "-ef",    /* osf1 */
   "auxw",   /* digital */   
   "auxw",   /* sun3 */
   "-ef",    /* irix4 */
   "-ef",    /* irix */
   "-ef",    /* irix64 */
   "auxw",   /* freebsd */
   "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,pri,rss,nlwp,stime,time,args",   /* solarisx86 */
   "-ax",    /* bsd 4.3 */
   "auxw",   /* newsos4 */
   "auxw",   /* netbsd */
   "auxw",   /* AOS */
   "auxw",   /* BSDI */
   "auxw",   /* nextstep */
   "-elyf",    /* cray */
   "auxw",   /* gnu */
   "-aW",    /* NT */
   "-ef",    /* Unixware */
   "auxw",   /* openbsd */
   "-ef",    /* sco */
   "auxw",   /* darwin */
   "-ef",    /* ux4800 */
   "-elyf",    /* qnx */
   "auxw",   /* dragonfly */
   "mingw-invalid", /* mingw */
   "?",      /* vmware*/
   "-",
   "-",
   "-",
   NULL
   };

/*********************************************************************/

char *VMOUNTCOMM[CF_CLASSATTR] =
   {
   "",                                              /* see cf.defs.h */
   "",
   "/etc/mount -va",      /* sun4 */
   "/etc/mount -va",      /* ultrix */
   "/sbin/mount -ea",      /* hpux */
   /*"/etc/mount -t nfs",*/   /* aix */
   "/usr/sbin/mount -t nfs",   /* aix */
   "/bin/mount -va",      /* linux */
   "/usr/sbin/mount -a",  /* solaris */
   "/usr/sbin/mount -va", /* osf1 */
   "/usr/sbin/mount -va", /* digital */   
   "/etc/mount -va",      /* sun3 */
   "/sbin/mount -va",     /* irix4 */
   "/sbin/mount -va",     /* irix */
   "/sbin/mount -va",     /* irix64 */
   "/sbin/mount -va",     /* freebsd */
   "/usr/sbin/mount -a",  /* solarisx86 */
   "/etc/mount -a",       /* bsd 4.3 */
   "/etc/mount -a",       /* newsos4 */
   "/sbin/mount -a",      /* netbsd */
   "/etc/mount -a",       /* AOS */
   "/sbin/mount -a",      /* BSDI */
   "/usr/etc/mount -a",   /* nextstep */
   "/etc/mount -va",      /* cray */
   "/bin/mount -va",      /* gnu */
   "/bin/sh /etc/fstab",  /* NT - possible security issue */
   "/sbin/mountall",      /* Unixware */
   "/sbin/mount",         /* openbsd */
   "/etc/mountall",         /* sco */
   "/sbin/mount -va",     /* darwin */
   "/sbin/mount -v",      /* ux4800 */
   "/bin/mount -v",       /* qnx */
   "/sbin/mount -va",     /* dragonfly */
   "mingw-invalid",       /* mingw */
   "/bin/mount -a",       /* vmware */
   "unused-blah",
   "unused-blah",
   "unused-blah",
   NULL
   };

/*********************************************************************/

char *VUNMOUNTCOMM[CF_CLASSATTR] =
   {
   "",                                              /* see cf.defs.h */
   "",
   "/etc/umount",      /* sun4 */
   "/etc/umount",      /* ultrix */
   "/sbin/umount",     /* hpux */
   "/usr/sbin/umount", /* aix */
   "/bin/umount",      /* linux */
   "/etc/umount",      /* solaris */
   "/usr/sbin/umount", /* osf1 */
   "/usr/sbin/umount", /* digital */   
   "/etc/umount",      /* sun3 */
   "/sbin/umount",     /* irix4 */
   "/sbin/umount",     /* irix */
   "/sbin/umount",     /* irix64 */
   "/sbin/umount",     /* freebsd */
   "/etc/umount",      /* solarisx86 */
   "/etc/umount",      /* bsd4.3 */
   "/etc/umount",      /* newsos4 */
   "/sbin/umount",     /* netbsd */
   "/etc/umount",      /* AOS */
   "/sbin/umount",     /* BSDI */
   "/usr/etc/umount",  /* nextstep */
   "/etc/umount",      /* cray */
   "/sbin/umount",     /* gnu */
   "/bin/umount",      /* NT */
   "/sbin/umount",     /* Unixware */
   "/sbin/umount",     /* openbsd */
   "/etc/umount",     /* sco */
   "/sbin/umount",     /* darwin */
   "/sbin/umount",     /* ux4800 */
   "/bin/umount",      /* qnx */
   "/sbin/umount",     /* dragonfly */
   "mingw-invalid",     /* mingw */
   "/bin/umount",      /* vmware */
   "unused-blah",
   "unused-blah",
   "unused-blah",
   NULL
   };



/*********************************************************************/

char *VMOUNTOPTS[CF_CLASSATTR] =
   {
   "",                                              /* see cf.defs.h */
   "",
   "bg,hard,intr",    /* sun4 */
   "bg,hard,intr",    /* ultrix */
   "bg,hard,intr",    /* hpux */
   "bg,hard,intr",    /* aix */
   "defaults",        /* linux */
   "bg,hard,intr",    /* solaris */
   "bg,hard,intr",    /* osf1 */
   "bg,hard,intr",    /* digital */   
   "bg,hard,intr",    /* sun3 */
   "bg,hard,intr",    /* irix4 */
   "bg,hard,intr",    /* irix */
   "bg,hard,intr",    /* irix64 */
   "bg,intr",         /* freebsd */
   "bg,hard,intr",    /* solarisx86 */
   "bg,hard,intr",    /* bsd4.3 */
   "bg,hard,intr",    /* newsos4 */
   "-i,-b",           /* netbsd */
   "bg,hard,intr",    /* AOS */
   "bg,intr",         /* BSDI */
   "bg,hard,intr",    /* nextstep */
   "bg,hard,intr",    /* cray */
   "defaults",        /* gnu */
   "",                /* NT */
   "bg,hard,intr",    /* Unixware */
   "-i,-b",           /* openbsd */
   "bg,hard,intr",    /* sco */
   "-i,-b",           /* darwin */
   "bg,hard,intr",    /* ux4800 */
   "bg,hard,intr",    /* qnx */
   "bg,intr",         /* dragonfly */
   "mingw-invalid",   /* mingw */
   "defaults",        /* vmstate */
   "unused-blah",
   "unused-blah",
   "unused-blah",
   NULL
   };

/*********************************************************************/

char *VRESOLVCONF[CF_CLASSATTR] =
   {
   "-",
   "-",                                              /* see cf.defs.h */
   "/etc/resolv.conf",     /* sun4 */
   "/etc/resolv.conf",     /* ultrix */
   "/etc/resolv.conf",     /* hpux */
   "/etc/resolv.conf",     /* aix */
   "/etc/resolv.conf",     /* linux */   
   "/etc/resolv.conf",     /* solaris */
   "/etc/resolv.conf",     /* osf1 */
   "/etc/resolv.conf",     /* digital */   
   "/etc/resolv.conf",     /* sun3 */
   "/usr/etc/resolv.conf", /* irix4 */
   "/etc/resolv.conf",     /* irix */
   "/etc/resolv.conf",     /* irix64 */
   "/etc/resolv.conf",     /* freebsd */
   "/etc/resolv.conf",     /* solarisx86 */
   "/etc/resolv.conf",     /* bsd4.3 */
   "/etc/resolv.conf",     /* newsos4 */
   "/etc/resolv.conf",     /* netbsd */
   "/etc/resolv.conf",     /* AOS */
   "/etc/resolv.conf",     /* BSDI */
   "/etc/resolv.conf",     /* nextstep */
   "/etc/resolv.conf",     /* cray */
   "/etc/resolv.conf",     /* gnu */
   "/etc/resolv.conf",     /* NT */
   "/etc/resolv.conf",     /* Unixware */
   "/etc/resolv.conf",     /* openbsd */
   "/etc/resolv.conf",     /* sco */
   "/etc/resolv.conf",     /* darwin */
   "/etc/resolv.conf",     /* ux4800 */
   "/etc/resolv.conf",     /* qnx */
   "/etc/resolv.conf",     /* dragonfly */
   "",                     /* mingw */
   "/etc/resolv.conf",     /* vmware */
   "unused-blah",
   "unused-blah",
   "unused-blah",
   NULL
   };



/*********************************************************************/

char *VFSTAB[CF_CLASSATTR] =
   {
   "-",
   "-",                                              /* see cf.defs.h */
   "/etc/fstab",       /* sun4 */
   "/etc/fstab",       /* ultrix */
   "/etc/fstab",       /* hpux */
   "/etc/filesystems", /* aix */
   "/etc/fstab",       /* linux */
   "/etc/vfstab",      /* solaris */
   "/etc/fstab",       /* osf1 */
   "/etc/fstab",       /* digital */   
   "/etc/fstab",       /* sun3 */
   "/etc/fstab",       /* irix4 */
   "/etc/fstab",       /* irix */
   "/etc/fstab",       /* irix64 */
   "/etc/fstab",       /* freebsd */
   "/etc/vfstab",      /* solarisx86 */
   "/etc/fstab",       /* bsd4.3 */
   "/etc/fstab",       /* newsos4 */
   "/etc/fstab",       /* netbsd */
   "/etc/fstab",       /* AOS */
   "/etc/fstab",       /* BSDI */
   "/etc/fstab",       /* nextstep */
   "/etc/fstab",       /* cray */
   "/etc/fstab",       /* gnu */
   "/etc/fstab",       /* NT */
   "/etc/vfstab",      /* Unixware */
   "/etc/fstab",       /* openbsd */
   "/etc/default/filesys", /* sco */
   "/etc/fstab",       /* darwin */
   "/etc/vfstab",      /* ux4800 */
   "/etc/fstab",       /* qnx */
   "/etc/fstab",       /* dragonfly */
   "",                 /* mingw */
   "/etc/fstab",       /* vmware */
   "unused-blah",
   "unused-blah",
   "unused-blah",
   NULL
   };

/*********************************************************************/

char *VMAILDIR[CF_CLASSATTR] =
   {
   "-",
   "-",                                              /* see cf.defs.h */
   "/var/spool/mail",    /* sun4 */
   "/usr/spool/mail",    /* ultrix */
   "/var/mail",          /* hpux */
   "/var/spool/mail",    /* aix */
   "/var/spool/mail",    /* linux */  
   "/var/mail",          /* solaris */
   "/usr/spool/mail",    /* osf1 */
   "/usr/spool/mail",    /* digital */   
   "/var/spool/mail",    /* sun3 */
   "/usr/mail",          /* irix4 */
   "/usr/mail",          /* irix */
   "/usr/var/mail",      /* irix64 */
   "/var/mail",          /* freebsd */
   "/var/mail",          /* solarisx86 */
   "/usr/spool/mail",    /* bsd4.3 */
   "/usr/spool/mail",    /* newsos4 */
   "/var/mail",          /* netbsd */
   "/usr/spool/mail",    /* AOS */
   "/var/mail",          /* BSDI */
   "/usr/spool/mail",    /* nextstep */
   "/usr/mail",          /* cray */
   "/var/spool/mail",    /* gnu */
   "N/A",                /* NT */
   "/var/mail",          /* Unixware */
   "/var/mail",          /* openbsd */
   "/var/spool/mail",    /* sco */
   "/var/mail",          /* darwin */
   "/var/mail",          /* ux4800 */
   "/var/spool/mail",    /* qnx */
   "/var/mail",          /* dragonfly */
   "",                   /* mingw */
   "/var/spool/mail",    /* vmware */
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
   "/usr/ucb/netstat -rn",   /* sun4 */
   "/usr/ucb/netstat -rn",   /* ultrix */
   "/usr/bin/netstat -rn",   /* hpux */
   "/usr/bin/netstat -rn",   /* aix */
   "/bin/netstat -rn",       /* linux */
   "/usr/bin/netstat -rn",   /* solaris */
   "/usr/sbin/netstat -rn",  /* osf1 */
   "/usr/sbin/netstat -rn",  /* digital */   
   "/usr/ucb/netstat -rn",   /* sun3 */
   "/usr/etc/netstat -rn",   /* irix4 */
   "/usr/etc/netstat -rn",   /* irix */
   "/usr/etc/netstat -rn",   /* irix64 */
   "/usr/bin/netstat -rn",   /* freebsd */
   "/bin/netstat -rn",       /* solarisx86 */
   "/usr/ucb/netstat -rn",   /* bsd4.3 */
   "/usr/ucb/netstat -rn",   /* newsos4 */
   "/usr/bin/netstat -rn",   /* netbsd */
   "/usr/ucb/netstat -rn",   /* AOS */
   "/usr/sbin/netstat -rn",  /* BSDI */
   "/usr/ucb/netstat -rn",   /* nextstep */
   "/usr/ucb/netstat -rn",   /* cray */
   "/bin/netstat -rn",       /* gnu */
   "/cygdrive/c/WINNT/System32/netstat", /* NT */
   "/usr/bin/netstat -rn",   /* Unixware */
   "/usr/bin/netstat -rn",   /* openbsd */
   "/usr/bin/netstat -rn",   /* sco */
   "/usr/sbin/netstat -rn",  /* darwin */
   "/usr/bin/netstat -rn",   /* ux4800 */
   "/usr/bin/netstat -rn",   /* qnx */
   "/usr/bin/netstat -rn",   /* dragonfly */
   "mingw-invalid",          /* mingw */
   "/usr/bin/netstat",       /* vmware */
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
   "/etc/exports",    /* sun4 */
   "/etc/exports",    /* ultrix */
   "/etc/exports",    /* hpux */
   "/etc/exports",    /* aix */
   "/etc/exports",    /* linux */
   "/etc/dfs/dfstab", /* solaris */
   "/etc/exports",    /* osf1 */
   "/etc/exports",    /* digital */   
   "/etc/exports",    /* sun3 */
   "/etc/exports",    /* irix4 */
   "/etc/exports",    /* irix */
   "/etc/exports",    /* irix64 */
   "/etc/exports",    /* freebsd */
   "/etc/dfs/dfstab", /* solarisx86 */
   "/etc/exports",    /* bsd4.3 */
   "/etc/exports",    /* newsos4 */
   "/etc/exports",    /* netbsd */
   "/etc/exports",    /* AOS */
   "/etc/exports",    /* BSDI */
   "/etc/exports",    /* nextstep */
   "/etc/exports",    /* cray */
   "/etc/exports",    /* gnu */
   "/etc/exports",    /* NT */
   "/etc/dfs/dfstab", /* Unixware */
   "/etc/exports",    /* openbsd */
   "/etc/dfs/dfstab", /* sco */
   "/etc/exports",    /* darwin */
   "/etc/exports",    /* ux4800 */
   "/etc/exports",    /* qnx */
   "/etc/exports",    /* dragonfly */
   "",                /* mingw */
   "none",            /* vmware */
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
   "-",              /* sun4 */
   "-",              /* ultrix */
   "-",              /* hpux */
   "-",              /* aix */
   "/sbin/route",    /* linux */
   "/usr/sbin/route",/* solaris */
   "-",              /* osf1 */
   "-",              /* digital */   
   "-",              /* sun3 */
   "-",              /* irix4 */
   "-",              /* irix */
   "-",              /* irix64 */
   "/sbin/route",    /* freebsd */
   "/usr/sbin/route",/* solarisx86 */
   "-",              /* bsd4.3 */
   "-",              /* newsos4 */
   "-",              /* netbsd */
   "-",              /* AOS */
   "-",              /* BSDI */
   "-",              /* nextstep */
   "-",              /* cray */
   "-",              /* gnu */
   "-",              /* NT */
   "-",              /* Unixware */
   "/sbin/route",    /* openbsd */
   "-",              /* sco */
   "/sbin/route",    /* darwin */
   "-",              /* ux4800 */
   "-",              /* qnx */
   "/sbin/route",    /* dragonfly */
   "-",              /* mingw */
   "-",              /* vmware */
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
   "-",              /* sun4 */
   "-",              /* ultrix */
   "-",              /* hpux */
   "-",              /* aix */
   "add %s gw %s",   /* linux */
   "add %s %s",      /* solaris */
   "-",              /* osf1 */
   "-",              /* digital */   
   "-",              /* sun3 */
   "-",              /* irix4 */
   "-",              /* irix */
   "-",              /* irix64 */
   "add %s %s",      /* freebsd */
   "add %s %s",      /* solarisx86 */
   "-",              /* bsd4.3 */
   "-",              /* newsos4 */
   "-",              /* netbsd */
   "-",              /* AOS */
   "-",              /* BSDI */
   "-",              /* nextstep */
   "-",              /* cray */
   "-",              /* gnu */
   "-",              /* NT */
   "-",              /* Unixware */
   "add %s %s",      /* openbsd */
   "-",              /* sco */
   "add %s %s",      /* darwin */
   "-",              /* ux4800 */
   "-",              /* qnx */
   "add %s %s",      /* dragonfly */
   "-",              /* mingw */
   "-",              /* vmware */
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
   "-",              /* sun4 */
   "-",              /* ultrix */
   "-",              /* hpux */
   "-",              /* aix */
   "del %s",         /* linux */
   "delete %s",      /* solaris */
   "-",              /* osf1 */
   "-",              /* digital */   
   "-",              /* sun3 */
   "-",              /* irix4 */
   "-",              /* irix */
   "-",              /* irix64 */
   "delete %s",      /* freebsd */
   "delete %s",      /* solarisx86 */
   "-",              /* bsd4.3 */
   "-",              /* newsos4 */
   "-",              /* netbsd */
   "-",              /* AOS */
   "-",              /* BSDI */
   "-",              /* nextstep */
   "-",              /* cray */
   "-",              /* gnu */
   "-",              /* NT */
   "-",              /* Unixware */
   "delete %s",      /* openbsd */
   "-",              /* sco */
   "delete %s",      /* darwin */
   "-",              /* ux4800 */
   "-",              /* qnx */
   "delete %s",      /* dragonfly */
   "-",              /* mingw */
   "-",              /* vmware */
   "unused-blah",
   "unused-blah",
   "unused-blah",
   NULL
   };
