/* cfengine for GNU
 
        Copyright (C) 1995/6
        Free Software Foundation, Inc.
 
   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/
@TOP@
/* The old route entry structure in newer BSDs */
#undef HAVE_ORTENTRY

/* Do we ahve any route entry structure ? */
#undef HAVE_RTENTRY

/* Whether to use the local REGEX functions */
#undef REGEX_MALLOC

/* Whether to use LCHOWN to change ownerships */
#undef HAVE_LCHOWN

/* Whether the thread library has setmask */
#undef HAVE_PTHREAD_SIGMASK

/* Whether the thread library has setstacksize */
#undef HAVE_PTHREAD_ATTR_SETSTACKSIZE

/* Whether libdb has db_open */
#undef HAVE_DB_CREATE

/* Does the host have /var/run directory? */
#undef HAVE_VAR_RUN


/* Special OS defines */
#undef SUN3
#undef SUN4
#undef SOLARIS
#undef ULTRIX
#undef HPuUX
#undef AIX
#undef OSF
#undef IRIX
#undef LINUX
#undef DEBIAN
#undef FREEBSD
#undef NETBSD
#undef NEWS_OS
#undef BSDOS
#undef BSD43
#undef AOS
#undef SCO
#undef NEXTSTEP
#undef CFCRAY
#undef CFQNX
#undef CFGNU
#undef UNIXWARE
#undef OPENBSD
#undef HAVE_SYS_ACL_H
#undef NOTBROKEN
#undef NT
#undef DARWIN

/* SVR4 header stuff */

#undef __EXTENSIONS__
#undef _POSIX_C_SOURCE

/* Solaris 2.6 */

#undef __BIT_TYPES_DEFINED__

/* LOCK and LOG directories */

#undef WORKDIR

/* Special CFEngine symbols */

#undef AUTOCONF_HOSTNAME
#undef AUTOCONF_SYSNAME
