/* cfengine for GNU

        Copyright (C) 1995
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
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


/*******************************************************************/
/*                                                                 */
/*  HEAER for cfengine                                            */
/*                                                                 */
/*******************************************************************/

/* Hard link this file between cf2/cf3 for consistent update */

#include "conf.h"

#ifdef NT
#  define MAX_FILENAME 227
#  define WINVER 0x501
#else
#  define MAX_FILENAME 254
#endif

#include <stdio.h>
#include <math.h>

#ifndef _GETOPT_H
#include "../pub/getopt.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#else
#define _SYS_NMLN       257

struct utsname
   {
   char    sysname[_SYS_NMLN];
   char    nodename[_SYS_NMLN];
   char    release[_SYS_NMLN];
   char    version[_SYS_NMLN];
   char    machine[_SYS_NMLN];
   };

#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef HAVE_SYS_SYSTEMINFO_H
# include <sys/systeminfo.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(s) ((unsigned)(s) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(s) (((s) & 255) == 0)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(s) ((s) & 0)  /* Can't use for BSD */
#endif
#ifndef WTERMSIG
#define WTERMSIG(s) ((s) & 0)
#endif

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <errno.h>

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#   include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <signal.h>

#ifdef MINGW
#define LOG_LOCAL0      (16<<3)
#define LOG_LOCAL1      (17<<3)
#define LOG_LOCAL2      (18<<3)
#define LOG_LOCAL3      (19<<3)
#define LOG_LOCAL4      (20<<3)
#define LOG_LOCAL5      (21<<3)
#define LOG_LOCAL6      (22<<3)
#define LOG_LOCAL7      (23<<3)
#define LOG_USER        (1<<3)
#define LOG_DAEMON      (3<<3)
#else
#include <syslog.h>
#endif

extern int errno;

/* Do this for ease of configuration from the Makefile */

#ifdef HPuUX
#define HPUX
#endif

#ifdef SunOS
#define SUN4
#endif

/* end of patch */

#ifdef AIX
#ifndef ps2
#include <sys/statfs.h>
#endif

#include <sys/systemcfg.h>
#endif

#ifdef SOLARIS
#include <sys/statvfs.h>
#undef nfstype
#endif

#ifndef HAVE_BCOPY
#define bcopy(fr,to,n)  memcpy(to,fr,n)  /* Eliminate ucblib */
#define bcmp(s1, s2, n) memcmp ((s1), (s2), (n))
#define bzero(s, n)     memset ((s), 0, (n))
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


#ifdef DARWIN
#include <sys/malloc.h>
#include <sys/paths.h>
#endif

#ifdef HAVE_SYS_MALLOC_H
#ifdef DARWIN
#include <sys/malloc.h>
#include <sys/paths.h>
#endif
#else
#ifdef HAVE_MALLOC_H
#ifndef OPENBSD
#ifdef __FreeBSD__
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#endif
#endif
#endif

#include <fcntl.h>

#ifdef HAVE_VFS_H
# include <sys/vfs.h>
#endif

#ifdef HPUX
# include <sys/dirent.h>
#endif

#ifdef HAVE_UTIME_H
# include <utime.h>      /* use utime not utimes for portability */
#elif TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#elif HAVE_SYS_TIME_H
#  include <sys/time.h>
#elif ! defined(AOS)
#  include <time.h>
#endif

#ifdef HAVE_TIME_H
# include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifndef MINGW
#include <pwd.h>
#include <grp.h>
#endif

#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif


#ifdef MINGW
# include <windows.h>
# include <accctrl.h>
# include <aclapi.h>
# include <psapi.h>
# include <wchar.h>
# include <sddl.h>
# include <tlhelp32.h>
# include <iphlpapi.h>
# include <ws2tcpip.h>
# include <objbase.h>  // for disphelper
# ifdef HAVE_WINSOCK2_H
#  include <winsock2.h>
# else
#  include <winsock.h>
# endif
#else
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <netinet/in.h>
# ifndef AOS
#  include <arpa/inet.h>
# endif
# include <netdb.h>
# if !defined LINUX && !defined NT
# include <sys/protosw.h>
# undef sgi
#  include <net/route.h>
# endif
#endif

#ifdef LINUX
#ifdef __GLIBC__
# include <net/route.h>
# include <netinet/in.h>
#else
# include <linux/route.h>
# include <linux/in.h>
#endif
#endif

#ifdef HAVE_PCRE_H
# include <pcreposix.h>
#elif HAVE_RXPOSIX_H
# include <rxposix.h>
#elif  HAVE_REGEX_H
# include <regex.h>
#endif

#ifndef HAVE_SNPRINTF
#include "../pub/snprintf.h"
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 1
#endif

#ifndef HAVE_CLOCKID_T
typedef int clockid_t;
#endif

#ifdef HAVE_PTHREAD_H
# define __USE_GNU 1

# include <pthread.h>
# ifndef _SC_THREAD_STACK_MIN
# define _SC_THREAD_STACK_MIN PTHREAD_STACK_MIN
# endif
#endif

#ifdef HAVE_SCHED_H
# include <sched.h>
#endif

#ifdef WITH_SELINUX
# include <selinux/selinux.h>
#endif

/*******************************************************************/
/* Various defines                                                 */
/*******************************************************************/

#define true  1
#define false 0
#define CF_BUFSIZE 4096
#define CF_BILLION 1000000000L
#define CF_EXPANDSIZE (2*CF_BUFSIZE)
#define CF_ALLCLASSSIZE (4*CF_BUFSIZE)
#define CF_BUFFERMARGIN 32
#define CF_BLOWFISHSIZE 16
#define CF_SMALLBUF 128
#define CF_MAXVARSIZE 1024
#define CF_MAXSIDSIZE 2048  /* Windows only: Max size (bytes) of security identifers */
#define CF_NONCELEN (CF_BUFSIZE/16)
#define CF_MAXLINKSIZE 256
#define CF_MAXLINKLEVEL 4
#define CF_MAXFARGS 8
#define CF_MAX_IP_LEN 64       /* numerical ip length */
#define CF_PROCCOLS 16
#define CF_HASHTABLESIZE 4969 /* prime number */
#define CF_MACROALPHABET 61    /* a-z, A-Z plus a bit */
#define CF_MAXSHELLARGS 64
#define CF_MAX_SCLICODES 16
#define CF_SAMEMODE 0
#define CF_SAME_OWNER ((uid_t)-1)
#define CF_UNKNOWN_OWNER ((uid_t)-2)
#define CF_SAME_GROUP ((gid_t)-1)
#define CF_UNKNOWN_GROUP ((gid_t)-2)
#define CF_NOSIZE    -1
#define CF_EXTRASPC 8      /* pads items during AppendItem for eol handling in editfiles */
#define CF_INFINITY ((int)999999999)
#define CF_TICKS_PER_DAY 86400 /* 60 * 60 *24 */
#define CF_TICKS_PER_HOUR 3600 /* 60 * 60 */
#define CF_HALF_HOUR 1800      /* 60 * 30 */
#define CF_NOT_CONNECTED -1
#define CF_COULD_NOT_CONNECT -2
#define CF_RECURSION_LIMIT 100
#define CF_MONDAY_MORNING 342000
#define CF_NOVAL -0.7259285297502359
#define CF_UNUSED_CHAR (char)127



#define CF_INDEX_FIELD_LEN 7
#define CF_INDEX_OFFSET  CF_INDEX_FIELD_LEN+1

#define CF_IFREQ 2048    /* Reportedly the largest size that does not segfault 32/64 bit*/
#define CF_ADDRSIZE 128

#define CF_METHODEXEC 0
#define CF_METHODREPLY  1

/* these should be >0 to prevent contention */

#define CF_EXEC_IFELAPSED 1
#define CF_EXEC_EXPIREAFTER 1

#define MAXIP4CHARLEN 16

/*******************************************************************/
/*  DBM                                                            */
/*******************************************************************/

// maximum amount of simultanious open DBs
#define MAX_OPENDB 30

#ifdef TCDB
# include <tcutil.h>
# include <tchdb.h>

typedef struct
  {
  TCHDB *hdb;
  char *valmemp;  // allocated on demand, freed on db close
  }
CF_TCDB;

typedef struct
   {
   char *curkey;
   char *curval;
   }
CF_TCDBC;

# define CF_DB CF_TCDB
# define CF_DBC CF_TCDBC
# define DB_FEXT "tcdb"

#elif defined(QDB)

# include <depot.h>

typedef struct
   {
   DEPOT *depot;
   char *valmemp;  // allocated on demand, freed on db close
   }
CF_QDB;

typedef struct
   {
   char *curkey;
   char *curval;
   }
CF_QDBC;

# define CF_DB CF_QDB
# define CF_DBC CF_QDBC
# define DB_FEXT "qdbm"


#else

# ifdef SOLARIS
#  ifndef u_int32_t
#   define u_int32_t uint32_t
#   define u_int16_t uint16_t
#   define u_int8_t uint8_t
#  endif
# endif

# include <db.h>
# define CF_DB DB
# define CF_DBC DBC
# define DB_FEXT "db"
# define BDB 1
#endif


/*******************************************************************/
/*  Windows                                                        */
/*******************************************************************/

#ifdef MINGW
# define MAXHOSTNAMELEN 256  // always adequate: http://msdn.microsoft.com/en-us/library/ms738527(VS.85).aspx
# define NULLFILE "nul"
# define EXEC_SUFFIX ".exe"

typedef u_long in_addr_t;  // as seen in in_addr struct in winsock.h
#ifndef VER_SUITE_WH_SERVER  // shold be in winnt.h, but is not in current mingw-version
# define VER_SUITE_WH_SERVER 0x00008000
#endif

/* Dummy signals, can be set to anything below 23 but
 * 2, 4, 8, 11, 15, 21, 22 which are taken.
 * Calling signal() with anything from below causes SIG_ERR
 * to be returned.                                         */

# define SIGALRM 1
# define SIGHUP 3
# define SIGTRAP 5
# define SIGKILL 6
# define SIGPIPE 7
# define SIGCONT 9
# define SIGSTOP 10
# define SIGQUIT 12
# define SIGCHLD 13
# define SIGUSR1 14
# define SIGUSR2 16
# define SIGBUS 17

# if !defined( _TIMESPEC_DEFINED) && !defined(HAVE_STRUCT_TIMESPEC)
#  define HAVE_STRUCT_TIMESPEC 1
   struct timespec {
           long tv_sec;
           long tv_nsec;
   };
# endif /* NOT _TIMESPEC_DEFINED */

#else  /* NOT MINGW */
# define NULLFILE "/dev/null"
# define EXEC_SUFFIX ""
#endif  /* NOT MINGW */

/*******************************************************************/
/* Class array limits                                              */
/* This is the only place you ever need to edit anything           */
/*******************************************************************/

#define CF_CLASSATTR 38         /* increase this for each new class added */
                                /* It defines the array size for class data */
#define CF_ATTRDIM 3            /* Only used in CLASSATTRUBUTES[][] defn */

   /* end class array limits */

/*******************************************************************/

/* database file names */

#define CF_CLASSUSAGE     "cf_classes" "." DB_FEXT
#define CF_PERFORMANCE    "performance" "." DB_FEXT
#define CF_CHKDB          "checksum_digests" "." DB_FEXT
#define CF_AVDB_FILE      "cf_observations" "." DB_FEXT
#define CF_STATEDB_FILE   "cf_state" "." DB_FEXT
#define CF_LASTDB_FILE    "cf_LastSeen" "." DB_FEXT
#define CF_AUDITDB_FILE   "cf_Audit" "." DB_FEXT
#define CF_LOCKDB_FILE    "cf_lock" "." DB_FEXT

/* end database file names */

#define CF_VALUE_LOG      "cf_value.log"


#define CF_STATELOG_FILE "state_log"
#define CF_ENVNEW_FILE   "env_data.new"
#define CF_ENV_FILE      "env_data"

#define CF_TCPDUMP_COMM "/usr/sbin/tcpdump -t -n -v"
#define CF_SCLI_COMM "/usr/local/bin/scli"


#define CF_INPUTSVAR "CFINPUTS"          /* default name for file path var */
#define CF_ALLCLASSESVAR "CFALLCLASSES"  /* default name for CFALLCLASSES env */
#define CF_INF_RECURSE -99             /* code used to signify inf in recursion */
#define CF_TRUNCATE -1
#define CF_EMPTYFILE -2
#define CF_USELOGFILE true              /* synonyms for tidy.c */
#define CF_NOLOGFILE  false
#define CF_SAVED ".cfsaved"
#define CF_EDITED ".cfedited"
#define CF_NEW ".cfnew"
#define CFD_TERMINATOR "---cfXen/gine/cfXen/gine---"
#define CFD_TRUE "CFD_TRUE"
#define CFD_FALSE "CFD_FALSE"
#define CF_ANYCLASS "any"
#define CF_NOCLASS "XX_CF_opposite_any_XX"
#define CF_NOUSER -99
#define CF_RSA_PROTO_OFFSET 24
#define CF_PROTO_OFFSET 16
#define CF_INBAND_OFFSET 8
#define CF_SMALL_OFFSET 2

/* digest sizes */
#define CF_MD5_LEN 16
#define CF_SHA_LEN 20
#define CF_SHA1_LEN 20
#define CF_BEST_LEN 0
#define CF_CRYPT_LEN 64
#define CF_SHA224_LEN 28
#define CF_SHA256_LEN 32
#define CF_SHA384_LEN 48
#define CF_SHA512_LEN 64

#define CF_DONE 't'
#define CF_MORE 'm'

#ifndef ERESTARTSYS
# define ERESTARTSYS EINTR
#endif

#define CF_FAILEDSTR "BAD: Unspecified server refusal (see verbose server output)"
#define CF_CHANGEDSTR1 "BAD: File changed "   /* Split this so it cannot be recognized */
#define CF_CHANGEDSTR2 "while copying"

#define CF_START_DOMAIN "undefined.domain"

#define CFLOGSIZE 1048576                  /* Size of lock-log before rotation */

/* Output control defines */

#define Verbose if (VERBOSE || DEBUG || D2) printf
#define EditVerbose  if (EDITVERBOSE || DEBUG || D2) printf
#define Debug4  if (D4) printf
#define Debug3  if (D3 || DEBUG || D2) printf
#define Debug2  if (DEBUG || D2) printf
#define Debug1  if (DEBUG || D1) printf
#define Debug   if (DEBUG || D1 || D2) printf
#define DebugVoid if (false) printf
#define Silent if (! SILENT || VERBOSE || DEBUG || D2) printf
#define DaemonOnly if (ISCFENGINE) yyerror("This belongs in cfservd.conf")
#define CfengineOnly if (! ISCFENGINE) yyerror("This belongs in cfagent.conf")

/* GNU REGEX */

#define BYTEWIDTH 8

/*****************************************************************************/

/* Auditing key */

#define CF_NOP      'n'
#define CF_CHG      'c'
#define CF_WARN     'w'  /* something wrong but nothing done */
#define CF_FAIL     'f'
#define CF_DENIED   'd'
#define CF_TIMEX    't'
#define CF_INTERPT  'i'
#define CF_REGULAR  'r'
#define CF_REPORT   'R'
#define CF_UNKNOWN  'u'

/*****************************************************************************/

#define CFGRACEPERIOD 4.0     /* training period in units of counters (weeks,iterations)*/
#define cf_noise_threshold 6  /* number that does not warrent large anomaly status */
#define big_number 100000
#define LDT_BUFSIZE 10
#define CF_GRAINS   64
#define ATTR     11
#define CF_NETATTR   7 /* icmp udp dns tcpsyn tcpfin tcpack */
#define PH_LIMIT 10
#define CF_WEEK   (7.0*24.0*3600.0)
#define CF_HOUR   3600
#define CF_RELIABLE_CLASSES 7*24         /* CF_WEEK/CF_HOUR */
#define CF_MEASURE_INTERVAL (5.0*60.0)

#define CF_OBSERVABLES 91

struct QPoint
   {
   double q;
   double expect;
   double var;
   };

struct Event
   {
   time_t t;
   struct QPoint Q;
   };

struct Averages
   {
   struct QPoint Q[CF_OBSERVABLES];
   };

struct OldAverages /* For conversion to new db */
   {
   double expect_number_of_users;
   double expect_rootprocs;
   double expect_otherprocs;
   double expect_diskfree;
   double expect_loadavg;
   double expect_incoming[ATTR];
   double expect_outgoing[ATTR];
   double expect_pH[PH_LIMIT];
   double var_number_of_users;
   double var_rootprocs;
   double var_otherprocs;
   double var_diskfree;
   double var_loadavg;
   double var_incoming[ATTR];
   double var_outgoing[ATTR];
   double var_pH[PH_LIMIT];
   double expect_netin[CF_NETATTR];
   double expect_netout[CF_NETATTR];
   double var_netin[CF_NETATTR];
   double var_netout[CF_NETATTR];
   };

/******************************************************************/

struct LockData
   {
   pid_t pid;
   time_t time;
   };

/*****************************************************************************/

#define CF_AUDIT_COMMENT 128
#define CF_AUDIT_VERSION 64
#define CF_AUDIT_DATE    32

struct AuditLog        /* key includes operation and date */
   {
   char  operator[CF_AUDIT_COMMENT];
   char  comment[CF_AUDIT_COMMENT];
   char  filename[CF_AUDIT_COMMENT];
   char  bundle[CF_AUDIT_VERSION]; /* not used in cf2 */
   char  version[CF_AUDIT_VERSION];
   char  date[CF_AUDIT_DATE];
   short lineno;
   char  status;
   };

/*******************************************************************/
/* Copy file defines                                               */
/*******************************************************************/

            /* Based heavily on cp.c in GNU-fileutils */

#ifndef DEV_BSIZE
#ifdef BSIZE
#define DEV_BSIZE BSIZE
#else /* !BSIZE */
#define DEV_BSIZE 4096
#endif /* !BSIZE */
#endif /* !DEV_BSIZE */

/* Extract or fake data from a `struct stat'.
   ST_BLKSIZE: Optimal I/O blocksize for the file, in bytes.
   ST_NBLOCKS: Number of 512-byte blocks in the file
   (including indirect blocks). */


#define SMALL_BLOCK_BUF_SIZE 512

#ifndef HAVE_ST_BLOCKS
# define ST_BLKSIZE(statbuf) DEV_BSIZE
# if defined(_POSIX_SOURCE) || !defined(BSIZE) /* fileblocks.c uses BSIZE.  */
#  define ST_NBLOCKS(statbuf) (((statbuf).st_size + 512 - 1) / 512)
# else /* !_POSIX_SOURCE && BSIZE */
#  define ST_NBLOCKS(statbuf) (st_blocks ((statbuf).st_size))
# endif /* !_POSIX_SOURCE && BSIZE */
#else /* HAVE_ST_BLOCKS */
/* Some systems, like Sequents, return st_blksize of 0 on pipes. */
# define ST_BLKSIZE(statbuf) ((statbuf).st_blksize > 0 \
                               ? (statbuf).st_blksize : DEV_BSIZE)
# if defined(hpux) || defined(__hpux__) || defined(__hpux)
/* HP-UX counts st_blocks in 1024-byte units.
   This loses when mixing HP-UX and BSD filesystems with NFS.  */
#  define ST_NBLOCKS(statbuf) ((statbuf).st_blocks * 2)
# else /* !hpux */
#  if defined(_AIX) && defined(_I386)
/* AIX PS/2 counts st_blocks in 4K units.  */
#    define ST_NBLOCKS(statbuf) ((statbuf).st_blocks * 8)
#  else /* not AIX PS/2 */
#    define ST_NBLOCKS(statbuf) ((statbuf).st_blocks)
#  endif /* not AIX PS/2 */
# endif /* !hpux */
#endif /* HAVE_ST_BLOCKS */

#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif

/*******************************************************************/
/* Client server defines                                           */
/*******************************************************************/

#define CFENGINE_SERVICE "cfengine"

enum PROTOS
   {
   cfd_exec,
   cfd_auth,
   cfd_get,
   cfd_opendir,
   cfd_synch,
   cfd_classes,
   cfd_md5,
   cfd_smd5,
   cfd_cauth,
   cfd_sauth,
   cfd_ssynch,
   cfd_sget,
   cfd_version,
   cfd_sopendir,
   cfd_var,
   cfd_svar,
   cfd_context,
   cfd_scontext,
   cfd_bad
   };

#define CF_WORDSIZE 8 /* Number of bytes in a word */

/*******************************************************************/

enum cf_filetype
   {
   cf_reg,
   cf_link,
   cf_dir,
   cf_fifo,
   cf_block,
   cf_char,
   cf_sock
   };

/*******************************************************************/

enum roles
   {
   cf_connect,
   cf_accept
   };

/*******************************************************************/

struct cfstat
   {
   char             *cf_filename;   /* What file are we statting? */
   char             *cf_server;     /* Which server did this come from? */
   enum cf_filetype  cf_type;       /* enum filetype */
   mode_t            cf_lmode;      /* Mode of link, if link */
   mode_t            cf_mode;       /* Mode of remote file, not link */
   uid_t             cf_uid;        /* User ID of the file's owner */
   gid_t             cf_gid;        /* Group ID of the file's group */
   off_t             cf_size;       /* File size in bytes */
   time_t            cf_atime;      /* Time of last access */
   time_t            cf_mtime;      /* Time of last data modification */
   time_t            cf_ctime;      /* Time of last file status change */
   char              cf_makeholes;  /* what we need to know from blksize and blks */
   char             *cf_readlink;   /* link value or NULL */
   int               cf_failed;     /* stat returned -1 */
   int               cf_nlink;      /* Number of hard links */
   int               cf_ino;        /* inode number on server */
   dev_t             cf_dev;        /* device number */
   struct cfstat    *next;
   };

/*******************************************************************/

struct cfdir
   {
   DIR         *cf_dirh;
   struct Item *cf_list;
   struct Item *cf_listpos;  /* current pos */
   };

typedef struct cfdir CFDIR;

/*******************************************************************/

struct cfdirent
   {
   struct dirent *cf_dirp;
   char   d_name[CF_BUFSIZE];   /* This is bigger than POSIX */
   };


/*******************************************************************/

enum cfsizes
   {
   cfabs,
   cfpercent
   };

/*******************************************************************/

enum statepolicy
   {
   cfreset,        /* Policy when trying to add already defined persistent states */
   cfpreserve
   };

/*******************************************************************/

enum classes
   {
   empty,
   soft,
   sun4,
   ultrx,
   hp,
   aix,
   linuxx,
   solaris,
   osf,
   digital,
   sun3,
   irix4,
   irix,
   irix64,
   freebsd,
   solarisx86,
   bsd4_3,
   newsos,
   netbsd,
   aos,
   bsd_i,
   nextstep,
   crayos,
   GnU,
   cfnt,
   unix_sv,
   openbsd,
   cfsco,
   darwin,
   ux4800,
   qnx,
   dragonfly,
   mingw,
   vmware,
   unused1,
   unused2,
   unused3
   };

/*******************************************************************/

enum fileactions
   {
   warnall,
   warnplain,
   warndirs,
   fixall,
   fixplain,
   fixdirs,
   touch,
   linkchildren,
   create,
   compress,
   alert
   };

/*******************************************************************/

enum SignalNames
   {
   cfnosignal,
   cfhup,
   cfint,
   cfquit,
   cfill,
   cftrap,
   cfiot,
   cfemt,
   cffpr,
   cfkill,
   cfbus,
   cfsegv,
   cfsys,
   cfpipe,
   cfalrm,
   cfterm
   };

#define highest_signal 64

/*******************************************************************/

enum modestate
   {
   wild,
   who,
   which
   };

enum modesort
   {
   unknown,
   numeric,
   symbolic
   };

/*******************************************************************/

enum cmpsense   /* For package version comparison */
   {
   cmpsense_eq,
   cmpsense_gt,
   cmpsense_lt,
   cmpsense_ge,
   cmpsense_le,
   cmpsense_ne,
   cmpsense_none
   };

/*******************************************************************/

enum pkgmgrs    /* Available package managers to query in packages: */
   {
   pkgmgr_rpm,
   pkgmgr_dpkg,
   pkgmgr_sun,
   pkgmgr_aix,
   pkgmgr_portage,
   pkgmgr_freebsd,
   pkgmgr_none
   };

/*******************************************************************/

enum pkgactions /* What to do with a package if it is found/not found */
    {
    pkgaction_install,
    pkgaction_remove,
    pkgaction_upgrade,
    pkgaction_fix,
    pkgaction_none
    };

/*******************************************************************/

typedef char flag;

enum socks
   {
   netbiosns,
   netbiosdgm,
   netbiosssn,
   irc,
   cfengine,
   nfsd,
   smtp,
   www,
   ftp,
   ssh,
   wwws
   };

enum iptypes
   {
   icmp,
   udp,
   dns,
   tcpsyn,
   tcpack,
   tcpfin,
   tcpmisc
   };

enum observables
    {
    ob_users,
    ob_rootprocs,
    ob_otherprocs,
    ob_diskfree,
    ob_loadavg,
    ob_netbiosns_in,
    ob_netbiosns_out,
    ob_netbiosdgm_in,
    ob_netbiosdgm_out,
    ob_netbiosssn_in,
    ob_netbiosssn_out,
    ob_irc_in,
    ob_irc_out,
    ob_cfengine_in,
    ob_cfengine_out,
    ob_nfsd_in,
    ob_nfsd_out,
    ob_smtp_in,
    ob_smtp_out,
    ob_www_in,
    ob_www_out,
    ob_ftp_in,
    ob_ftp_out,
    ob_ssh_in,
    ob_ssh_out,
    ob_wwws_in,
    ob_wwws_out,
    ob_icmp_in,
    ob_icmp_out,
    ob_udp_in,
    ob_udp_out,
    ob_dns_in,
    ob_dns_out,
    ob_tcpsyn_in,
    ob_tcpsyn_out,
    ob_tcpack_in,
    ob_tcpack_out,
    ob_tcpfin_in,
    ob_tcpfin_out,
    ob_tcpmisc_in,
    ob_tcpmisc_out,
    ob_webaccess,
    ob_weberrors,
    ob_syslog,
    ob_messages,
    ob_temp0,
    ob_temp1,
    ob_temp2,
    ob_temp3,
    ob_cpuall,
    ob_cpu0,
    ob_cpu1,
    ob_cpu2,
    ob_cpu3,
    ob_spare
    };


struct sock
   {
   char *portnr;
   char *name;
   enum observables in;
   enum observables out;
   };


/*******************************************************************/

struct cfagent_connection
   {
   int sd;
   int trust;               /* true if key being accepted on trust */
   int authenticated;
   int protoversion;
   int family;                              /* AF_INET or AF_INET6 */
   char localip[CF_MAX_IP_LEN];
   char remoteip[CF_MAX_IP_LEN];
   unsigned char *session_key;
   char encryption_type;
   short error;
   };


/*******************************************************************/

struct cfObject
   {
   char *scope;                         /* Name of object (scope) */
   void *hashtable[CF_HASHTABLESIZE];   /* Variable heap  */
   char type[CF_HASHTABLESIZE];         /* scalar or itlist? */
   char *classlist;                     /* Private classes -- ? */
   struct Item *actionsequence;
   struct cfObject *next;
   };

/*

 $(globalvar)
 $(obj.name)

*/

/*******************************************************************/

struct CompressedArray
   {
   int key;
   char *value;
   struct CompressedArray *next;
   };

/*******************************************************************/

struct Audit
   {
   char *version;
   char *filename;
   char *date;
   unsigned char digest[EVP_MAX_MD_SIZE+1];
   struct Audit *next;
   };

/*******************************************************************/
/* Action /promise types                                           */
/*******************************************************************/

struct Item
   {
   char   done;
   char  *name;
   char  *classes;
   int    counter;
   time_t time;
   struct Item *next;
   };

/*******************************************************************/

struct TwoDimList
   {
   short is2d;                  /* true if list > 1 */
   short rounds;
   short tied;                  /* do variables march together or in rounds ? */
   char  sep;                   /* list separator */
   struct Item *ilist;          /* Each node contains a list */
   struct Item *current;        /* A static working pointer */
   struct TwoDimList *next;
   };

/*******************************************************************/

struct UidList
   {
#ifdef MINGW  // TODO: remove uid for NT ?
     char sid[CF_MAXSIDSIZE];  /* Invalid sid indicates unset */
#endif  /* MINGW */
   uid_t uid;
   char *uidname;                               /* when uid is -2 */
   struct UidList *next;
   };

/*******************************************************************/

struct GidList
   {
   gid_t gid;
   char *gidname;                               /* when gid is -2 */
   struct GidList *next;
   };

/*******************************************************************/

enum cffstype
   {
   posixfs,
   solarisfs,
   dfsfs,
   afsfs,
   hpuxfs,
   ntfs,
   badfs
   };

/*******************************************************************/

enum matchtypes
    {
    literalStart,
    literalComplete,
    literalSomewhere,
    regexComplete,
    NOTliteralStart,
    NOTliteralComplete,
    NOTliteralSomewhere,
    NOTregexComplete
    };

/*******************************************************************/

struct Auth
   {
   char *path;
   struct Item *accesslist;
   struct Item *maproot;     /* which hosts should have root read access */
   int encrypt;              /* which files HAVE to be transmitted securely */
   int literal;
   int classpattern;
   struct Auth *next;
   };

/*******************************************************************/

struct Strategy
   {
   char    done;
   char   *name;
   char   *classes;
   char   type;                 /* default r=random */
   struct Item *strategies;
   struct Strategy *next;
   };

/*******************************************************************/
/* Checksum database structures                                    */
/*******************************************************************/

struct Checksum_Value
   {
   unsigned char mess_digest[EVP_MAX_MD_SIZE+1];  /* Content digest */
   unsigned char attr_digest[EVP_MAX_MD_SIZE+1];  /* Attribute digest */
   };

/*******************************************************************/
/* Ultrix/BSD don't have all these from sys/stat.h                 */
/*******************************************************************/

# ifndef S_IFBLK
#  define S_IFBLK 0060000
# endif
# ifndef S_IFCHR
#  define S_IFCHR 0020000
# endif
# ifndef S_IFDIR
#  define S_IFDIR 0040000
# endif
# ifndef S_IFIFO
#  define S_IFIFO 0010000
# endif
# ifndef S_IFREG
#  define S_IFREG 0100000
# endif
# ifndef S_IFLNK
#  define S_IFLNK 0120000
# endif
# ifndef S_IFSOCK
#  define S_IFSOCK 0140000
# endif
# ifndef S_IFMT
#  define S_IFMT  00170000
# endif


#ifndef S_ISREG
# define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
# define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISLNK
# define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_ISFIFO
# define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)
#endif
#ifndef S_ISCHR
# define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)
#endif
#ifndef S_ISBLK
# define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)
#endif
#ifndef S_ISSOCK
# define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)
#endif

#ifndef S_IRUSR
#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
#endif

/********************************************************************/
/* *BSD chflags stuff -                                             */
/********************************************************************/

# if !defined UF_NODUMP
#  define UF_NODUMP 0
# endif
# if !defined UF_IMMUTABLE
#  define UF_IMMUTABLE 0
# endif
# if !defined UF_APPEND
#  define UF_APPEND 0
# endif
# if !defined UF_OPAQUE
#  define UF_OPAQUE 0
# endif
# if !defined UF_NOUNLINK
#  define UF_NOUNLINK 0
# endif
# if !defined SF_ARCHIVED
#  define SF_ARCHIVED 0
# endif
# if !defined SF_IMMUTABLE
#  define SF_IMMUTABLE 0
# endif
# if !defined SF_APPEND
#  define SF_APPEND 0
# endif
# if !defined SF_NOUNLINK
#  define SF_NOUNLINK 0
# endif
# define CHFLAGS_MASK  ( UF_NODUMP | UF_IMMUTABLE | UF_APPEND | UF_OPAQUE | UF_NOUNLINK | SF_ARCHIVED | SF_IMMUTABLE | SF_APPEND | SF_NOUNLINK )

/* For cygwin32 */

#if !defined O_BINARY
#  define O_BINARY 0
#endif

/*******************************************************************/
/* File path manipulation primitives                               */
/*******************************************************************/

/* Defined maximum length of a filename. */

/* File node separator (cygwin can use \ or / but prefer \ for communicating
 * with native windows commands). */

#ifdef NT
#  define IsFileSep(c) ((c) == '\\' || (c) == '/')
#else
#  define IsFileSep(c) ((c) == '/')
#endif


/********************************************************************/
/* All prototypes                                                   */
/********************************************************************/

#include "prototypes.h"
