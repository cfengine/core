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

#ifndef CFENGINE_CF_DEFS_H
#define CFENGINE_CF_DEFS_H

#include "conf.h"

#define _GNU_SOURCE 1

#ifdef NT
# define MAX_FILENAME 227
# define WINVER 0x501
# define FD_SETSIZE 512         // increase select(2) FD limit from 64
#else
# define MAX_FILENAME 254
#endif

#ifdef MINGW
# include <winsock2.h>
# include <windows.h>
# include <accctrl.h>
# include <aclapi.h>
# include <psapi.h>
# include <wchar.h>
# include <sddl.h>
# include <tlhelp32.h>
# include <iphlpapi.h>
# include <ws2tcpip.h>
# include <objbase.h>           // for disphelper
#endif

#include <stdio.h>
#include <math.h>

#ifndef _GETOPT_H
# include "../pub/getopt.h"
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#ifdef HAVE_UNAME
# include <sys/utsname.h>
#else
# define _SYS_NMLN       257

struct utsname
{
    char sysname[_SYS_NMLN];
    char nodename[_SYS_NMLN];
    char release[_SYS_NMLN];
    char version[_SYS_NMLN];
    char machine[_SYS_NMLN];
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
# include <sys/mount.h>
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
# define WIFSIGNALED(s) ((s) & 0)       /* Can't use for BSD */
#endif
#ifndef WTERMSIG
# define WTERMSIG(s) ((s) & 0)
#endif

#include "bool.h"
#include "compiler.h"

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
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <signal.h>

#ifdef MINGW
# define LOG_LOCAL0      (16<<3)
# define LOG_LOCAL1      (17<<3)
# define LOG_LOCAL2      (18<<3)
# define LOG_LOCAL3      (19<<3)
# define LOG_LOCAL4      (20<<3)
# define LOG_LOCAL5      (21<<3)
# define LOG_LOCAL6      (22<<3)
# define LOG_LOCAL7      (23<<3)
# define LOG_USER        (1<<3)
# define LOG_DAEMON      (3<<3)

#else /* NOT MINGW */
# include <syslog.h>
#endif

/* Do this for ease of configuration from the Makefile */

#ifdef HPuUX
# define HPUX
#endif

#ifdef SunOS
# define SUN4
#endif

/* end of patch */

#ifdef AIX
# ifndef ps2
#  include <sys/statfs.h>
# endif

# include <sys/systemcfg.h>
#endif

#ifdef SOLARIS
# include <sys/statvfs.h>
# undef nfstype
#endif

#if !HAVE_DECL_DIRFD
int dirfd(DIR *dirp);
#endif

/* strndup is defined as a macro on many systems */
#if !HAVE_DECL_STRNDUP
# ifndef strndup
char *strndup(const char *s, size_t n);
# endif
#endif

#if !HAVE_DECL_STRNLEN
size_t strnlen(const char *str, size_t maxlen);
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if !HAVE_DECL_STRLCPY
size_t strlcpy(char *destination, const char *source, size_t size);
#endif

#if !HAVE_DECL_STRLCAT
size_t strlcat(char *destination, const char *source, size_t size);
#endif

#ifdef DARWIN
# include <sys/malloc.h>
# include <sys/paths.h>
#endif

#ifdef HAVE_SYS_MALLOC_H
# ifdef DARWIN
#  include <sys/malloc.h>
#  include <sys/paths.h>
# endif
#else
# ifdef HAVE_MALLOC_H
#  ifndef OPENBSD
#   ifdef __FreeBSD__
#    include <stdlib.h>
#   else
#    include <malloc.h>
#   endif
#  endif
# endif
#endif

#include <fcntl.h>

#ifdef HAVE_VFS_H
# include <sys/vfs.h>
#endif

#ifdef HPUX
# include <sys/dirent.h>
#endif

#ifdef HAVE_UTIME_H
# include <utime.h>             /* use utime not utimes for portability */
#elif TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#elif HAVE_SYS_TIME_H
# include <sys/time.h>
#elif ! defined(AOS)
# include <time.h>
#endif

#ifdef HAVE_TIME_H
# include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifndef MINGW
# include <pwd.h>
# include <grp.h>
#endif

#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

#ifndef MINGW
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# ifndef AOS
#  include <arpa/inet.h>
# endif
# include <netdb.h>
# if !defined LINUX && !defined NT
#  include <sys/protosw.h>
#  undef sgi
#  include <net/route.h>
# endif
#endif

#ifdef LINUX
# ifdef __GLIBC__
#  include <net/route.h>
#  include <netinet/in.h>
# else
#  include <linux/route.h>
#  include <linux/in.h>
# endif
#endif

#ifndef CLOCK_REALTIME
# define CLOCK_REALTIME 1
#endif

#ifndef HAVE_CLOCKID_T
typedef int clockid_t;
#endif

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#if defined(HAVE_PTHREAD)
# define __USE_GNU 1

# include <pthread.h>
# ifndef _SC_THREAD_STACK_MIN
#  define _SC_THREAD_STACK_MIN PTHREAD_STACK_MIN
# endif

# ifndef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#  define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP PTHREAD_MUTEX_INITIALIZER
# endif

#endif

#ifdef HAVE_SCHED_H
# include <sched.h>
#endif

#ifdef WITH_SELINUX
# include <selinux/selinux.h>
#endif

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef INADDR_NONE
# define INADDR_NONE ((unsigned long int) 0xffffffff)
#endif
#ifndef HAVE_SETEGID
int setegid(gid_t gid);
#endif
#if !HAVE_DECL_DRAND48
double drand48(void);
#endif
#if !HAVE_DECL_SRAND48
void srand48(long seed);
#endif
#if !HAVE_DECL_CLOCK_GETTIME
int clock_gettime(clockid_t clock_id, struct timespec *tp);
#endif
#ifdef MINGW
unsigned int alarm(unsigned int seconds);
#endif
#if !HAVE_DECL_REALPATH
char *realpath(const char *path, char *resolved_path);
#endif

#ifndef HAVE_GETNETGRENT
int setnetgrent(const char *netgroup);
int getnetgrent(char **host, char **user, char **domain);
void endnetgrent(void);
#endif
#ifndef HAVE_UNAME
int uname(struct utsname *name);
#endif
#if !HAVE_DECL_STRSTR
char *strstr(const char *haystack, const char *needle);
#endif
#if !HAVE_DECL_STRCASESTR
char *strcasestr(const char *haystack, const char *needle);
#endif
#if !HAVE_DECL_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#endif
#if !HAVE_DECL_STRNCASECMP
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
#if !HAVE_DECL_STRDUP
char *strdup(const char *str);
#endif
#if !HAVE_DECL_MEMDUP
void *memdup(const void *mem, size_t size);
#endif
#if !HAVE_DECL_STRERROR
char *strerror(int err);
#endif
#ifndef HAVE_PUTENV
int putenv(char *s);
#endif
#if !HAVE_DECL_UNSETENV
int unsetenv(const char *name);
#endif
#ifndef HAVE_SETEUID
int seteuid(uid_t euid);
#endif
#ifndef HAVE_SETEUID
int setegid(gid_t egid);
#endif
#if !HAVE_DECL_SETLINEBUF
void setlinebuf(FILE *stream);
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
# if !HAVE_VSNPRINTF
int rpl_vsnprintf(char *, size_t, const char *, va_list);
# endif
# if !HAVE_SNPRINTF
int rpl_snprintf(char *, size_t, const char *, ...);
# endif
# if !HAVE_VASPRINTF
int rpl_vasprintf(char **, const char *, va_list);
# endif
# if !HAVE_ASPRINTF
int rpl_asprintf(char **, const char *, ...);
# endif
#endif /* HAVE_STDARG_H */
#if !defined(isfinite)
# define isfinite(x) finite(x)
#endif

#if !HAVE_DECL_STRRSTR
char *strrstr(const char *haystack, const char *needle);
#endif
#if !HAVE_DECL_MKDTEMP
char *mkdtemp(char *template);
#endif

/*******************************************************************/
/* Preprocessor tricks                                             */
/*******************************************************************/

/* Convert integer constant to string */
#define STRINGIFY__INTERNAL_MACRO(x) #x
#define TOSTRING(x) STRINGIFY__INTERNAL_MACRO(x)

/*******************************************************************/
/* Various defines                                                 */
/*******************************************************************/

#define CF_BUFSIZE 4096
/* max size of plaintext in one transaction, see
   net.c:SendTransaction(), leave space for encryption padding
   (assuming max 64*8 = 512-bit cipher block size)*/
#define CF_MAXTRANSSIZE (CF_BUFSIZE - CF_INBAND_OFFSET - 64)
#define CF_BILLION 1000000000L
#define CF_EXPANDSIZE (2*CF_BUFSIZE)
#define CF_ALLCLASSSIZE (4*CF_BUFSIZE)
#define CF_BUFFERMARGIN 128
#define CF_BLOWFISHSIZE 16
#define CF_SMALLBUF 128
#define CF_MAXVARSIZE 1024
#define CF_MAXSIDSIZE 2048      /* Windows only: Max size (bytes) of security identifiers */
#define CF_NONCELEN (CF_BUFSIZE/16)
#define CF_MAXLINKSIZE 256
#define CF_MAXLINKLEVEL 4
#define CF_MAXFARGS 8
#define CF_MAX_IP_LEN 64        /* numerical ip length */
#define CF_PROCCOLS 16
/* #define CF_HASHTABLESIZE 7919  prime number */
#define CF_HASHTABLESIZE 8192
#define CF_MACROALPHABET 61     /* a-z, A-Z plus a bit */
#define CF_ALPHABETSIZE 256
#define CF_MAXSHELLARGS 64
#define CF_MAX_SCLICODES 16
#define CF_SAMEMODE 7777
#define CF_SAME_OWNER ((uid_t)-1)
#define CF_UNKNOWN_OWNER ((uid_t)-2)
#define CF_SAME_GROUP ((gid_t)-1)
#define CF_UNKNOWN_GROUP ((gid_t)-2)
#define CF_NOSIZE    -1
#define CF_EXTRASPC 8           /* pads items during AppendItem for eol handling in editfiles */
#define CF_INFINITY ((int)999999999)
#define SOCKET_INVALID -1
#define CF_COULD_NOT_CONNECT -2
#define CF_RECURSION_LIMIT 100
#define CF_MONDAY_MORNING 345600
#define CF_NOVAL -0.7259285297502359
#define CF_UNUSED_CHAR (char)127

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR (60 * SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY (24 * SECONDS_PER_HOUR)
#define SECONDS_PER_WEEK (7 * SECONDS_PER_DAY)

/* Long-term monitoring constants */

#define HOURS_PER_SHIFT 6
#define SECONDS_PER_SHIFT (HOURS_PER_SHIFT * SECONDS_PER_HOUR)
#define SHIFTS_PER_DAY 4
#define SHIFTS_PER_WEEK (4*7)

#define CF_INDEX_FIELD_LEN 7
#define CF_INDEX_OFFSET  CF_INDEX_FIELD_LEN+1

#define CF_IFREQ 2048           /* Reportedly the largest size that does not segfault 32/64 bit */
#define CF_ADDRSIZE 128

#define CF_METHODEXEC 0
#define CF_METHODREPLY  1

/* these should be >0 to prevent contention */

#define CF_EXEC_IFELAPSED 0
#define CF_EDIT_IFELAPSED 3     /* NOTE: If doing copy template then edit working copy,
                                   the edit ifelapsed must not be higher than
                                   the copy ifelapsed. This will make the working
                                   copy equal to the copied template file - not the
                                   copied + edited file. */
#define CF_EXEC_EXPIREAFTER 1

#define MAXIP4CHARLEN 16
#define PACK_UPIFELAPSED_SALT "packageuplist"

/*******************************************************************/
/*  Windows                                                        */
/*******************************************************************/

#ifdef MINGW
# define MAXHOSTNAMELEN 256     // always adequate: http://msdn.microsoft.com/en-us/library/ms738527(VS.85).aspx
# define NULLFILE "nul"
# define EXEC_SUFFIX ".exe"

// as seen in in_addr struct in winsock.h
typedef u_long in_addr_t;

// shold be in winnt.h, but is not in current MinGW version
# ifndef VER_SUITE_WH_SERVER
#  define VER_SUITE_WH_SERVER 0x00008000
# endif

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
struct timespec
{
    long tv_sec;
    long tv_nsec;
};
# endif/* NOT _TIMESPEC_DEFINED */

#else /* NOT MINGW */
# define NULLFILE "/dev/null"
# define EXEC_SUFFIX ""
#endif /* NOT MINGW */

/*******************************************************************/
/* Class array limits                                              */
/* This is the only place you ever need to edit anything           */
/*******************************************************************/

#define CF_CLASSATTR 38         /* increase this for each new class added */
                                /* It defines the array size for class data */
#define CF_ATTRDIM 3            /* Only used in CLASSATTRUBUTES[][] defn */

   /* end class array limits */

/*******************************************************************/

#define CF_VALUE_LOG      "cf_value.log"
#define CF_FILECHANGE     "file_change.log"
#define CF_PROMISE_LOG    "promise_summary.log"

#define CF_STATELOG_FILE "state_log"
#define CF_ENVNEW_FILE   "env_data.new"
#define CF_ENV_FILE      "env_data"

#define CF_TCPDUMP_COMM "/usr/sbin/tcpdump -t -n -v"

#define CF_INPUTSVAR "CFINPUTS" /* default name for file path var */
#define CF_ALLCLASSESVAR "CFALLCLASSES" /* default name for CFALLCLASSES env */
#define CF_INF_RECURSE -99      /* code used to signify inf in recursion */
#define CF_TRUNCATE -1
#define CF_EMPTYFILE -2
#define CF_USELOGFILE true      /* synonyms for tidy.c */
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

#ifndef EOPNOTSUPP
# define EOPNOTSUPP EINVAL
#endif

#ifndef ENOTSUPP
# define ENOTSUPP EINVAL
#endif

#define CF_FAILEDSTR "BAD: Unspecified server refusal (see verbose server output)"
#define CF_CHANGEDSTR1 "BAD: File changed "     /* Split this so it cannot be recognized */
#define CF_CHANGEDSTR2 "while copying"

#define CF_START_DOMAIN "undefined.domain"

#define CFLOGSIZE 1048576       /* Size of lock-log before rotation */

/* Output control defines */

#define CfDebug   if (DEBUG) printf

/* GNU REGEX */

#define BYTEWIDTH 8

/*****************************************************************************/

/* Auditing key */

#define CF_NOP      'n'
#define CF_CHG      'c'
#define CF_WARN     'w'         /* something wrong but nothing done */
#define CF_FAIL     'f'
#define CF_DENIED   'd'
#define CF_TIMEX    't'
#define CF_INTERPT  'i'
#define CF_REGULAR  'r'
#define CF_REPORT   'R'
#define CF_UNKNOWN  'u'

/*****************************************************************************/

#define CFGRACEPERIOD 4.0       /* training period in units of counters (weeks,iterations) */
#define cf_noise_threshold 6    /* number that does not warrent large anomaly status */
#define MON_THRESHOLD_HIGH 1000000      // samples should stay below this threshold
#define LDT_BUFSIZE 10
#define CF_GRAINS   64
#define ATTR     11
#define CF_NETATTR   7          /* icmp udp dns tcpsyn tcpfin tcpack */
#define PH_LIMIT 10
#define CF_RELIABLE_CLASSES 7*24        /* WEEK/HOUR */
#define CF_MEASURE_INTERVAL (5.0*60.0)
#define CF_SHIFT_INTERVAL (6*3600.0)

#define CF_OBSERVABLES 91

#include "statistics.h"

typedef struct
{
    time_t t;
    QPoint Q;
} Event;

typedef struct
{
    QPoint Q[CF_OBSERVABLES];
} Averages;

/******************************************************************/

typedef struct
{
    pid_t pid;
    time_t time;
} LockData;

/*****************************************************************************/

#define CF_AUDIT_COMMENT 128
#define CF_AUDIT_VERSION 64
#define CF_AUDIT_DATE    32

/* key includes operation and date */
typedef struct
{
    char operator[CF_AUDIT_COMMENT];
    char comment[CF_AUDIT_COMMENT];
    char filename[CF_AUDIT_COMMENT];
    char bundle[CF_AUDIT_VERSION];      /* not used in cf2 */
    char version[CF_AUDIT_VERSION];
    char date[CF_AUDIT_DATE];
    short line_number;
    char status;
} AuditLog;

/*******************************************************************/
/* Copy file defines                                               */
/*******************************************************************/

            /* Based heavily on cp.c in GNU-fileutils */

#ifndef DEV_BSIZE
# ifdef BSIZE
#  define DEV_BSIZE BSIZE
# else/* !BSIZE */
#  define DEV_BSIZE 4096
# endif/* !BSIZE */
#endif /* !DEV_BSIZE */

/* Extract or fake data from a `struct stat'.
   ST_BLKSIZE: Optimal I/O blocksize for the file, in bytes.
   ST_NBLOCKS: Number of 512-byte blocks in the file
   (including indirect blocks). */

#define SMALL_BLOCK_BUF_SIZE 512

#ifndef HAVE_ST_BLOCKS
# define ST_BLKSIZE(statbuf) DEV_BSIZE
# if defined(_POSIX_SOURCE) || !defined(BSIZE)  /* fileblocks.c uses BSIZE.  */
#  define ST_NBLOCKS(statbuf) (((statbuf).st_size + 512 - 1) / 512)
# else/* !_POSIX_SOURCE && BSIZE */
#  define ST_NBLOCKS(statbuf) (st_blocks ((statbuf).st_size))
# endif/* !_POSIX_SOURCE && BSIZE */
#else /* HAVE_ST_BLOCKS */
/* Some systems, like Sequents, return st_blksize of 0 on pipes. */
# define ST_BLKSIZE(statbuf) ((statbuf).st_blksize > 0 \
                               ? (statbuf).st_blksize : DEV_BSIZE)
# if defined(hpux) || defined(__hpux__) || defined(__hpux)
/* HP-UX counts st_blocks in 1024-byte units.
   This loses when mixing HP-UX and BSD filesystems with NFS.  */
#  define ST_NBLOCKS(statbuf) ((statbuf).st_blocks * 2)
# else/* !hpux */
#  if defined(_AIX) && defined(_I386)
/* AIX PS/2 counts st_blocks in 4K units.  */
#   define ST_NBLOCKS(statbuf) ((statbuf).st_blocks * 8)
#  else
      /* not AIX PS/2 */
#   define ST_NBLOCKS(statbuf) ((statbuf).st_blocks)
#  endif
       /* not AIX PS/2 */
# endif/* !hpux */
#endif /* HAVE_ST_BLOCKS */

#ifndef SEEK_CUR
# define SEEK_CUR 1
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
    cfd_squery,
    cfd_bad
};

#define CF_WORDSIZE 8           /* Number of bytes in a word */

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

typedef struct Stat_ Stat;

struct Stat_
{
    char *cf_filename;          /* What file are we statting? */
    char *cf_server;            /* Which server did this come from? */
    enum cf_filetype cf_type;   /* enum filetype */
    mode_t cf_lmode;            /* Mode of link, if link */
    mode_t cf_mode;             /* Mode of remote file, not link */
    uid_t cf_uid;               /* User ID of the file's owner */
    gid_t cf_gid;               /* Group ID of the file's group */
    off_t cf_size;              /* File size in bytes */
    time_t cf_atime;            /* Time of last access */
    time_t cf_mtime;            /* Time of last data modification */
    time_t cf_ctime;            /* Time of last file status change */
    char cf_makeholes;          /* what we need to know from blksize and blks */
    char *cf_readlink;          /* link value or NULL */
    int cf_failed;              /* stat returned -1 */
    int cf_nlink;               /* Number of hard links */
    int cf_ino;                 /* inode number on server */
    dev_t cf_dev;               /* device number */
    Stat *next;
};

/*******************************************************************/

typedef struct Item_ Item;

/*******************************************************************/

enum cfsizes
{
    cfabs,
    cfpercent
};

/*******************************************************************/

enum statepolicy
{
    cfreset,                    /* Policy when trying to add already defined persistent states */
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

typedef struct
{
    char *portnr;
    char *name;
    enum observables in;
    enum observables out;
} Sock;

/*******************************************************************/

typedef struct
{
    int sd;
    int trust;                  /* true if key being accepted on trust */
    int authenticated;
    int protoversion;
    int family;                 /* AF_INET or AF_INET6 */
    char username[CF_SMALLBUF];
    char localip[CF_MAX_IP_LEN];
    char remoteip[CF_MAX_IP_LEN];
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    unsigned char *session_key;
    char encryption_type;
    short error;
} AgentConnection;

/*******************************************************************/

typedef struct CompressedArray_ CompressedArray;

struct CompressedArray_
{
    int key;
    char *value;
    CompressedArray *next;
};

/*******************************************************************/

typedef struct Audit_ Audit;

struct Audit_
{
    char *version;
    char *filename;
    char *date;
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    Audit *next;
};

/*******************************************************************/
/* Action /promise types                                           */
/*******************************************************************/

struct Item_
{
    char done;
    char *name;
    char *classes;
    int counter;
    time_t time;
    Item *next;
};

/*******************************************************************/

typedef struct UidList_ UidList;

struct UidList_
{
#ifdef MINGW                    // TODO: remove uid for NT ?
    char sid[CF_MAXSIDSIZE];    /* Invalid sid indicates unset */
#endif                          /* MINGW */
    uid_t uid;
    char *uidname;              /* when uid is -2 */
    UidList *next;
};

/*******************************************************************/

typedef struct GidList_ GidList;

struct GidList_
{
    gid_t gid;
    char *gidname;              /* when gid is -2 */
    GidList *next;
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

typedef struct Auth_ Auth;

struct Auth_
{
    char *path;
    Item *accesslist;
    Item *maproot;              /* which hosts should have root read access */
    int encrypt;                /* which files HAVE to be transmitted securely */
    int literal;
    int classpattern;
    Auth *next;
};

/*******************************************************************/
/* Checksum database structures                                    */
/*******************************************************************/

typedef struct
{
    unsigned char mess_digest[EVP_MAX_MD_SIZE + 1];     /* Content digest */
    unsigned char attr_digest[EVP_MAX_MD_SIZE + 1];     /* Attribute digest */
} ChecksumValue;

/*******************************************************************/
/* Ultrix/BSD don't have all these from sys/stat.h                 */
/*******************************************************************/

#ifndef S_IFBLK
# define S_IFBLK 0060000
#endif
#ifndef S_IFCHR
# define S_IFCHR 0020000
#endif
#ifndef S_IFDIR
# define S_IFDIR 0040000
#endif
#ifndef S_IFIFO
# define S_IFIFO 0010000
#endif
#ifndef S_IFREG
# define S_IFREG 0100000
#endif
#ifndef S_IFLNK
# define S_IFLNK 0120000
#endif
#ifndef S_IFSOCK
# define S_IFSOCK 0140000
#endif
#ifndef S_IFMT
# define S_IFMT  00170000
#endif

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
# define S_IRWXU 00700
# define S_IRUSR 00400
# define S_IWUSR 00200
# define S_IXUSR 00100

# define S_IRWXG 00070
# define S_IRGRP 00040
# define S_IWGRP 00020
# define S_IXGRP 00010

# define S_IRWXO 00007
# define S_IROTH 00004
# define S_IWOTH 00002
# define S_IXOTH 00001
#endif

/* Too bad we don't have FD_CLOEXEC -- but we can fake it */
#ifndef FD_CLOEXEC
# define FD_CLOEXEC 0
#endif

/********************************************************************/
/* *BSD chflags stuff -                                             */
/********************************************************************/

#if !defined UF_NODUMP
# define UF_NODUMP 0
#endif
#if !defined UF_IMMUTABLE
# define UF_IMMUTABLE 0
#endif
#if !defined UF_APPEND
# define UF_APPEND 0
#endif
#if !defined UF_OPAQUE
# define UF_OPAQUE 0
#endif
#if !defined UF_NOUNLINK
# define UF_NOUNLINK 0
#endif
#if !defined SF_ARCHIVED
# define SF_ARCHIVED 0
#endif
#if !defined SF_IMMUTABLE
# define SF_IMMUTABLE 0
#endif
#if !defined SF_APPEND
# define SF_APPEND 0
#endif
#if !defined SF_NOUNLINK
# define SF_NOUNLINK 0
#endif
#define CHFLAGS_MASK  ( UF_NODUMP | UF_IMMUTABLE | UF_APPEND | UF_OPAQUE | UF_NOUNLINK | SF_ARCHIVED | SF_IMMUTABLE | SF_APPEND | SF_NOUNLINK )

/* For cygwin32 */

#if !defined O_BINARY
# define O_BINARY 0
#endif

/*******************************************************************/
/* File path manipulation primitives                               */
/*******************************************************************/

/* Defined maximum length of a filename. */

/* File node separator (cygwin can use \ or / but prefer \ for communicating
 * with native windows commands). */

#ifdef NT
# define IsFileSep(c) ((c) == '\\' || (c) == '/')
#else
# define IsFileSep(c) ((c) == '/')
#endif

/* Nobody already knows why it was needed in first place. Please test whether
   removing this variable is harmless on HP/UX nowadays. */

#ifdef HPuUX
int Error;
#endif

#endif
