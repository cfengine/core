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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <sysinfo.h>

#include <cf3.extern.h>

#include <env_context.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <scope.h>
#include <item_lib.h>
#include <matching.h>
#include <unix.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <rlist.h>
#include <audit.h>
#include <pipes.h>

#include <inttypes.h>

#ifdef HAVE_ZONE_H
# include <zone.h>
#endif

// HP-UX mpctl() for $(sys.cpus) on HP-UX - Mantis #1069
#ifdef HAVE_SYS_MPCTL_H
# include <sys/mpctl.h>
#endif

/*****************************************************/
// Uptime calculation settings for GetUptimeMinutes() - Mantis #1134
// HP-UX: pstat_getproc(2) on init (pid 1)
#ifdef HPuUX
#define _PSTAT64
#include <sys/param.h>
#include <sys/pstat.h>
#define BOOT_TIME_WITH_PSTAT_GETPROC
#endif

// Solaris: kstat() for kernel statistics
// See http://dsc.sun.com/solaris/articles/kstatc.html
// BSD also has a kstat.h (albeit in sys), so check SOLARIS just to be paranoid
#if defined(SOLARIS) && defined(HAVE_KSTAT_H)
#include <kstat.h>
#define BOOT_TIME_WITH_KSTAT
#endif

// BSD: sysctl(3) to get kern.boottime, CPU count, etc.
// See http://www.unix.com/man-page/FreeBSD/3/sysctl/
// Linux also has sys/sysctl.h, so we check KERN_BOOTTIME to make sure it's BSD
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/param.h>
#include <sys/sysctl.h>
#ifdef KERN_BOOTTIME 
#define BOOT_TIME_WITH_SYSCTL
#endif
#endif

// GNU/Linux: struct sysinfo.uptime
#ifdef HAVE_STRUCT_SYSINFO_UPTIME
#include <sys/sysinfo.h>
#define BOOT_TIME_WITH_SYSINFO
#endif

// For anything else except Windows, try {stat("/proc/1")}.st_ctime
#if !defined(__MINGW32__) && !defined(NT)
#define BOOT_TIME_WITH_PROCFS
#endif

#if defined(BOOT_TIME_WITH_SYSINFO) || defined(BOOT_TIME_WITH_SYSCTL) || \
    defined(BOOT_TIME_WITH_KSTAT) || defined(BOOT_TIME_WITH_PSTAT_GETPROC) || \
    defined(BOOT_TIME_WITH_PROCFS)
#define CF_SYS_UPTIME_IMPLEMENTED
static time_t GetBootTimeFromUptimeCommand(time_t); // Last resort
#ifdef HAVE_PCRE_H
# include <pcre.h>
#endif
#endif

/*****************************************************/

void CalculateDomainName(const char *nodename, const char *dnsname, char *fqname, char *uqname, char *domain);

#ifdef __linux__
static int Linux_Fedora_Version(EvalContext *ctx);
static int Linux_Redhat_Version(EvalContext *ctx);
static void Linux_Oracle_VM_Server_Version(EvalContext *ctx);
static void Linux_Oracle_Version(EvalContext *ctx);
static int Linux_Suse_Version(EvalContext *ctx);
static int Linux_Slackware_Version(EvalContext *ctx, char *filename);
static int Linux_Debian_Version(EvalContext *ctx);
static int Linux_Mandrake_Version(EvalContext *ctx);
static int Linux_Mandriva_Version(EvalContext *ctx);
static int Linux_Mandriva_Version_Real(EvalContext *ctx, char *filename, char *relstring, char *vendor);
static int VM_Version(EvalContext *ctx);
static int Xen_Domain(EvalContext *ctx);
static int EOS_Version(EvalContext *ctx);
static int MiscOS(EvalContext *ctx);

#ifdef XEN_CPUID_SUPPORT
static void Xen_Cpuid(uint32_t idx, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
static int Xen_Hv_Check(void);
#endif

static bool ReadLine(const char *filename, char *buf, int bufsize);
static FILE *ReadFirstLine(const char *filename, char *buf, int bufsize);
#endif

static void GetCPUInfo(EvalContext *ctx);

static const char *CLASSATTRIBUTES[PLATFORM_CONTEXT_MAX][3] =
{
    {"-", "-", "-"},            /* as appear here are matched. The fields are sysname and machine */
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
};

static const char *VRESOLVCONF[PLATFORM_CONTEXT_MAX] =
{
    "-",
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
};

static const char *VMAILDIR[PLATFORM_CONTEXT_MAX] =
{
    "-",
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
};

static const char *VEXPORTS[PLATFORM_CONTEXT_MAX] =
{
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
};


/*******************************************************************/

void CalculateDomainName(const char *nodename, const char *dnsname, char *fqname, char *uqname, char *domain)
{
    if (strstr(dnsname, "."))
    {
        strlcpy(fqname, dnsname, CF_BUFSIZE);
    }
    else
    {
        strlcpy(fqname, nodename, CF_BUFSIZE);
    }

    if ((strncmp(nodename, fqname, strlen(nodename)) == 0) && (fqname[strlen(nodename)] == '.'))
    {
        /* If hostname is not qualified */
        strcpy(domain, fqname + strlen(nodename) + 1);
        strcpy(uqname, nodename);
    }
    else
    {
        /* If hostname is qualified */

        char *p = strchr(nodename, '.');

        if (p != NULL)
        {
            strlcpy(uqname, nodename, MIN(CF_BUFSIZE, p - nodename + 1));
            strlcpy(domain, p + 1, CF_BUFSIZE);
        }
        else
        {
            strcpy(uqname, nodename);
            strcpy(domain, "");
        }
    }
}

/*******************************************************************/

void DetectDomainName(EvalContext *ctx, const char *orig_nodename)
{
    char nodename[CF_BUFSIZE];

    strcpy(nodename, orig_nodename);
    ToLowerStrInplace(nodename);

    char dnsname[CF_BUFSIZE] = "";
    char fqn[CF_BUFSIZE];

    if (gethostname(fqn, sizeof(fqn)) != -1)
    {
        struct hostent *hp;

        if ((hp = gethostbyname(fqn)))
        {
            strncpy(dnsname, hp->h_name, CF_MAXVARSIZE);
            ToLowerStrInplace(dnsname);
        }
    }

    CalculateDomainName(nodename, dnsname, VFQNAME, VUQNAME, VDOMAIN);

/*
 * VFQNAME = a.b.c.d ->
 * NewClass("a.b.c.d")
 * NewClass("b.c.d")
 * NewClass("c.d")
 * NewClass("d")
 */
    char *ptr = VFQNAME;

    do
    {
        EvalContextHeapAddHard(ctx, ptr);

        ptr = strchr(ptr, '.');
        if (ptr != NULL)
            ptr++;
    }
    while (ptr != NULL);

    EvalContextHeapAddHard(ctx, VUQNAME);
    EvalContextHeapAddHard(ctx, VDOMAIN);

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "host", nodename, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "uqhost", VUQNAME, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "fqhost", VFQNAME, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "domain", VDOMAIN, DATA_TYPE_STRING);
}

/*******************************************************************/

void DiscoverVersion(EvalContext *ctx)
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    if (3 == sscanf(Version(), "%d.%d.%d", &major, &minor, &patch))
    {
        char workbuf[CF_BUFSIZE];

        snprintf(workbuf, CF_MAXVARSIZE, "%d", major);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cf_version_major", workbuf, DATA_TYPE_STRING);
        snprintf(workbuf, CF_MAXVARSIZE, "%d", minor);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cf_version_minor", workbuf, DATA_TYPE_STRING);
        snprintf(workbuf, CF_MAXVARSIZE, "%d", patch);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cf_version_patch", workbuf, DATA_TYPE_STRING);

        snprintf(workbuf, CF_BUFSIZE, "%s%cinputs%clib%c%d.%d", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, FILE_SEPARATOR, major, minor);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "libdir", workbuf, DATA_TYPE_STRING);

        snprintf(workbuf, CF_BUFSIZE, "lib%c%d.%d", FILE_SEPARATOR, major, minor);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "local_libdir", workbuf, DATA_TYPE_STRING);
    }
    else
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cf_version_major", "BAD VERSION " VERSION, DATA_TYPE_STRING);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cf_version_minor", "BAD VERSION " VERSION, DATA_TYPE_STRING);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cf_version_patch", "BAD VERSION " VERSION, DATA_TYPE_STRING);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "libdir", CFWORKDIR, DATA_TYPE_STRING);
    }
}

void GetNameInfo3(EvalContext *ctx, AgentType agent_type)
{
    int i, found = false;
    char *sp, workbuf[CF_BUFSIZE];
    time_t tloc;
    struct hostent *hp;
    struct sockaddr_in cin;
    unsigned char digest[EVP_MAX_MD_SIZE + 1];

#ifdef _AIX
    char real_version[_SYS_NMLN];
#endif
#if defined(HAVE_SYSINFO) && (defined(SI_ARCHITECTURE) || defined(SI_PLATFORM))
    long sz;
#endif
    char *components[] = { "cf-twin", "cf-agent", "cf-serverd", "cf-monitord", "cf-know",
        "cf-report", "cf-key", "cf-runagent", "cf-execd", "cf-hub",
        "cf-promises", NULL
    };
    int have_component[11];
    struct stat sb;
    char name[CF_MAXVARSIZE], quoteName[CF_MAXVARSIZE], shortname[CF_MAXVARSIZE];

    if (uname(&VSYSNAME) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't get kernel name info!. (uname: %s)", GetErrorStr());
        memset(&VSYSNAME, 0, sizeof(VSYSNAME));
    }

#ifdef _AIX
    snprintf(real_version, _SYS_NMLN, "%.80s.%.80s", VSYSNAME.version, VSYSNAME.release);
    strncpy(VSYSNAME.release, real_version, _SYS_NMLN);
#endif

    ToLowerStrInplace(VSYSNAME.sysname);
    ToLowerStrInplace(VSYSNAME.machine);

#ifdef _AIX
    switch (_system_configuration.architecture)
    {
    case POWER_RS:
        strncpy(VSYSNAME.machine, "power", _SYS_NMLN);
        break;
    case POWER_PC:
        strncpy(VSYSNAME.machine, "powerpc", _SYS_NMLN);
        break;
    case IA64:
        strncpy(VSYSNAME.machine, "ia64", _SYS_NMLN);
        break;
    }
#endif

/*
 * solarisx86 is a historically defined class for Solaris on x86. We have to
 * define it manually now.
 */
#ifdef __sun
    if (strcmp(VSYSNAME.machine, "i86pc") == 0)
    {
        EvalContextHeapAddHard(ctx, "solarisx86");
    }
#endif

    DetectDomainName(ctx, VSYSNAME.nodename);

    if ((tloc = time((time_t *) NULL)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't read system clock");
    }
    else
    {
        snprintf(workbuf, CF_BUFSIZE, "%jd", (intmax_t) tloc);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "systime", workbuf, DATA_TYPE_INT);
        snprintf(workbuf, CF_BUFSIZE, "%jd", (intmax_t) tloc / SECONDS_PER_DAY);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "sysday", workbuf, DATA_TYPE_INT);
        i = GetUptimeMinutes(tloc);
        if (i != -1)
        {
            snprintf(workbuf, CF_BUFSIZE, "%d", i);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "uptime", workbuf, DATA_TYPE_INT);
        }
    }

    for (i = 0; i < PLATFORM_CONTEXT_MAX; i++)
    {
        char sysname[CF_BUFSIZE];
        strlcpy(sysname, VSYSNAME.sysname, CF_BUFSIZE);
        ToLowerStrInplace(sysname);

        if (StringMatchFull(CLASSATTRIBUTES[i][0], sysname))
        {
            if (StringMatchFull(CLASSATTRIBUTES[i][1], VSYSNAME.machine))
            {
                if (StringMatchFull(CLASSATTRIBUTES[i][2], VSYSNAME.release))
                {
                    EvalContextHeapAddHard(ctx, CLASSTEXT[i]);

                    found = true;

                    VSYSTEMHARDCLASS = (PlatformContext) i;
                    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "class", CLASSTEXT[i], DATA_TYPE_STRING);
                    break;
                }
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "I recognize '%s' but not '%s'", VSYSNAME.sysname, VSYSNAME.machine);
                continue;
            }
        }
    }

    if (!found)
    {
        i = 0;
    }

    snprintf(workbuf, CF_BUFSIZE, "%s", CLASSTEXT[i]);

    Log(LOG_LEVEL_VERBOSE, "%s", NameVersion());

    if (LEGACY_OUTPUT)
    {
        Log(LOG_LEVEL_VERBOSE, "------------------------------------------------------------------------");
    }
    Log(LOG_LEVEL_VERBOSE, "Host name is: %s", VSYSNAME.nodename);
    Log(LOG_LEVEL_VERBOSE, "Operating System Type is %s", VSYSNAME.sysname);
    Log(LOG_LEVEL_VERBOSE, "Operating System Release is %s", VSYSNAME.release);
    Log(LOG_LEVEL_VERBOSE, "Architecture = %s", VSYSNAME.machine);
    Log(LOG_LEVEL_VERBOSE, "Using internal soft-class %s for host %s", workbuf, VSYSNAME.nodename);
    Log(LOG_LEVEL_VERBOSE, "The time is now %s", ctime(&tloc));
    if (LEGACY_OUTPUT)
    {
        Log(LOG_LEVEL_VERBOSE, "------------------------------------------------------------------------");
    }

    snprintf(workbuf, CF_MAXVARSIZE, "%s", ctime(&tloc));
    if (Chop(workbuf, CF_EXPANDSIZE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "date", workbuf, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cdate", CanonifyName(workbuf), DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "os", VSYSNAME.sysname, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "release", VSYSNAME.release, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "version", VSYSNAME.version, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "arch", VSYSNAME.machine, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "workdir", CFWORKDIR, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "fstab", VFSTAB[VSYSTEMHARDCLASS], DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "resolv", VRESOLVCONF[VSYSTEMHARDCLASS], DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "maildir", VMAILDIR[VSYSTEMHARDCLASS], DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "exports", VEXPORTS[VSYSTEMHARDCLASS], DATA_TYPE_STRING);

    snprintf(workbuf, CF_BUFSIZE, "%s%cbin", CFWORKDIR, FILE_SEPARATOR);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "bindir", workbuf, DATA_TYPE_STRING);

    snprintf(workbuf, CF_BUFSIZE, "%s%cinputs", CFWORKDIR, FILE_SEPARATOR);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "inputdir", workbuf, DATA_TYPE_STRING);

    snprintf(workbuf, CF_BUFSIZE, "%s%cmasterfiles", CFWORKDIR, FILE_SEPARATOR);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "masterdir", workbuf, DATA_TYPE_STRING);

    snprintf(workbuf, CF_BUFSIZE, "%s%cinputs%cfailsafe.cf", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "failsafe_policy_path", workbuf, DATA_TYPE_STRING);

    snprintf(workbuf, CF_BUFSIZE, "%s%cinputs%cupdate.cf", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "update_policy_path", workbuf, DATA_TYPE_STRING);

/* FIXME: type conversion */
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cf_version", (char *) Version(), DATA_TYPE_STRING);

    DiscoverVersion(ctx);

    if (PUBKEY)
    {
        char pubkey_digest[CF_MAXVARSIZE] = { 0 };

        HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);
        HashPrintSafe(CF_DEFAULT_DIGEST, digest, pubkey_digest);

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "key_digest", pubkey_digest, DATA_TYPE_STRING);

        snprintf(workbuf, CF_MAXVARSIZE - 1, "PK_%s", pubkey_digest);
        CanonifyNameInPlace(workbuf);
        EvalContextHeapAddHard(ctx, workbuf);
    }

    for (i = 0; components[i] != NULL; i++)
    {
        snprintf(shortname, CF_MAXVARSIZE - 1, "%s", CanonifyName(components[i]));

#if defined(_WIN32)
        // twin has own dir, and is named agent
        if (i == 0)
        {
            snprintf(name, CF_MAXVARSIZE - 1, "%s%cbin-twin%ccf-agent.exe", CFWORKDIR, FILE_SEPARATOR,
                     FILE_SEPARATOR);
        }
        else
        {
            snprintf(name, CF_MAXVARSIZE - 1, "%s%cbin%c%s.exe", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR,
                     components[i]);
        }
#else
        snprintf(name, CF_MAXVARSIZE - 1, "%s%cbin%c%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, components[i]);
#endif

        have_component[i] = false;

        if (stat(name, &sb) != -1)
        {
            snprintf(quoteName, sizeof(quoteName), "\"%s\"", name);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, shortname, quoteName, DATA_TYPE_STRING);
            have_component[i] = true;
        }
    }

// If no twin, fail over the agent

    if (!have_component[0])
    {
        snprintf(shortname, CF_MAXVARSIZE - 1, "%s", CanonifyName(components[0]));

#if defined(_WIN32)
        snprintf(name, CF_MAXVARSIZE - 1, "%s%cbin%c%s.exe", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR,
                 components[1]);
#else
        snprintf(name, CF_MAXVARSIZE - 1, "%s%cbin%c%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, components[1]);
#endif

        if (stat(name, &sb) != -1)
        {
            snprintf(quoteName, sizeof(quoteName), "\"%s\"", name);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, shortname, quoteName, DATA_TYPE_STRING);
        }
    }

/* Windows special directories and tools */

#ifdef __MINGW32__
    if (NovaWin_GetWinDir(workbuf, sizeof(workbuf)))
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "windir", workbuf, DATA_TYPE_STRING);
    }

    if (NovaWin_GetSysDir(workbuf, sizeof(workbuf)))
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "winsysdir", workbuf, DATA_TYPE_STRING);

        char filename[CF_BUFSIZE];
        if (snprintf(filename, sizeof(filename), "%s%s", workbuf, "\\WindowsPowerShell\\v1.0\\powershell.exe") < sizeof(filename))
        {
            if (NovaWin_FileExists(filename))
            {
                EvalContextHeapAddHard(ctx, "powershell");
                Log(LOG_LEVEL_VERBOSE, "Additional hard class defined as: %s", "powershell");
            }
        }
    }

    if (NovaWin_GetProgDir(workbuf, sizeof(workbuf)))
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "winprogdir", workbuf, DATA_TYPE_STRING);
    }

# ifdef _WIN64
// only available on 64 bit windows systems
    if (NovaWin_GetEnv("PROGRAMFILES(x86)", workbuf, sizeof(workbuf)))
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "winprogdir86", workbuf, DATA_TYPE_STRING);
    }

# else/* NOT _WIN64 */

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "winprogdir86", "", DATA_TYPE_STRING);

# endif

#else /* !__MINGW32__ */

// defs on Unix for manual-building purposes

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "windir", "/dev/null", DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "winsysdir", "/dev/null", DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "winprogdir", "/dev/null", DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "winprogdir86", "/dev/null", DATA_TYPE_STRING);

#endif /* !__MINGW32__ */

    if (agent_type != AGENT_TYPE_EXECUTOR && !LOOKUP)
    {
        LoadSlowlyVaryingObservations(ctx);
    }

    EnterpriseContext(ctx);

    sprintf(workbuf, "%u_bit", (unsigned) sizeof(void*) * 8);
    EvalContextHeapAddHard(ctx, workbuf);
    Log(LOG_LEVEL_VERBOSE, "Additional hard class defined as: %s", CanonifyName(workbuf));

    snprintf(workbuf, CF_BUFSIZE, "%s_%s", VSYSNAME.sysname, VSYSNAME.release);
    EvalContextHeapAddHard(ctx, workbuf);

    EvalContextHeapAddHard(ctx, VSYSNAME.machine);
    Log(LOG_LEVEL_VERBOSE, "Additional hard class defined as: %s", CanonifyName(workbuf));

    snprintf(workbuf, CF_BUFSIZE, "%s_%s", VSYSNAME.sysname, VSYSNAME.machine);
    EvalContextHeapAddHard(ctx, workbuf);
    Log(LOG_LEVEL_VERBOSE, "Additional hard class defined as: %s", CanonifyName(workbuf));

    snprintf(workbuf, CF_BUFSIZE, "%s_%s_%s", VSYSNAME.sysname, VSYSNAME.machine, VSYSNAME.release);
    EvalContextHeapAddHard(ctx, workbuf);
    Log(LOG_LEVEL_VERBOSE, "Additional hard class defined as: %s", CanonifyName(workbuf));

#ifdef HAVE_SYSINFO
# ifdef SI_ARCHITECTURE
    sz = sysinfo(SI_ARCHITECTURE, workbuf, CF_BUFSIZE);
    if (sz == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "cfengine internal: sysinfo returned -1");
    }
    else
    {
        EvalContextHeapAddHard(ctx, workbuf);
        Log(LOG_LEVEL_VERBOSE, "Additional hard class defined as: %s", workbuf);
    }
# endif
# ifdef SI_PLATFORM
    sz = sysinfo(SI_PLATFORM, workbuf, CF_BUFSIZE);
    if (sz == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "cfengine internal: sysinfo returned -1");
    }
    else
    {
        EvalContextHeapAddHard(ctx, workbuf);
        Log(LOG_LEVEL_VERBOSE, "Additional hard class defined as: %s", workbuf);
    }
# endif
#endif

    snprintf(workbuf, CF_BUFSIZE, "%s_%s_%s_%s", VSYSNAME.sysname, VSYSNAME.machine, VSYSNAME.release,
             VSYSNAME.version);

    if (strlen(workbuf) > CF_MAXVARSIZE - 2)
    {
        Log(LOG_LEVEL_VERBOSE, "cfengine internal: $(arch) overflows CF_MAXVARSIZE! Truncating");
    }

    sp = xstrdup(CanonifyName(workbuf));
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "long_arch", sp, DATA_TYPE_STRING);
    EvalContextHeapAddHard(ctx, sp);
    free(sp);

    snprintf(workbuf, CF_BUFSIZE, "%s_%s", VSYSNAME.sysname, VSYSNAME.machine);
    sp = xstrdup(CanonifyName(workbuf));
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "ostype", sp, DATA_TYPE_STRING);
    EvalContextHeapAddHard(ctx, sp);
    free(sp);

    if (!found)
    {
        Log(LOG_LEVEL_ERR, "I don't understand what architecture this is");
    }

    strcpy(workbuf, "compiled_on_");
    strcat(workbuf, CanonifyName(AUTOCONF_SYSNAME));
    EvalContextHeapAddHard(ctx, workbuf);
    Log(LOG_LEVEL_VERBOSE, "GNU autoconf class from compile time: %s", workbuf);

/* Get IP address from nameserver */

    if ((hp = gethostbyname(VFQNAME)) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Hostname lookup failed on node name '%s'", VSYSNAME.nodename);
        return;
    }
    else
    {
        memset(&cin, 0, sizeof(cin));
        cin.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
        Log(LOG_LEVEL_VERBOSE, "Address given by nameserver: %s", inet_ntoa(cin.sin_addr));
        strcpy(VIPADDRESS, inet_ntoa(cin.sin_addr));

        for (i = 0; hp->h_aliases[i] != NULL; i++)
        {
            Log(LOG_LEVEL_DEBUG, "Adding alias '%s'", hp->h_aliases[i]);
            EvalContextHeapAddHard(ctx, hp->h_aliases[i]);
        }
    }

#ifdef HAVE_GETZONEID
    zoneid_t zid;
    char zone[ZONENAME_MAX];
    char vbuff[CF_BUFSIZE];

    zid = getzoneid();
    getzonenamebyid(zid, zone, ZONENAME_MAX);

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "zone", zone, DATA_TYPE_STRING);
    snprintf(vbuff, CF_BUFSIZE - 1, "zone_%s", zone);
    EvalContextHeapAddHard(ctx, vbuff);

    if (strcmp(zone, "global") == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "CFEngine seems to be running inside a global solaris zone of name '%s'", zone);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "CFEngine seems to be running inside a local solaris zone of name '%s'", zone);
    }
#endif
}

/*******************************************************************/

void Get3Environment(EvalContext *ctx, AgentType agent_type)
{
    char env[CF_BUFSIZE], context[CF_BUFSIZE], name[CF_MAXVARSIZE], value[CF_BUFSIZE];
    FILE *fp;
    struct stat statbuf;
    time_t now = time(NULL);

    Log(LOG_LEVEL_VERBOSE, "Looking for environment from cf-monitord...");

    snprintf(env, CF_BUFSIZE, "%s/state/%s", CFWORKDIR, CF_ENV_FILE);
    MapName(env);

    if (stat(env, &statbuf) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Unable to detect environment from cf-monitord");
        return;
    }

    if (statbuf.st_mtime < (now - 60 * 60))
    {
        Log(LOG_LEVEL_VERBOSE, "Environment data are too old - discarding");
        unlink(env);
        return;
    }

    snprintf(value, CF_MAXVARSIZE - 1, "%s", ctime(&statbuf.st_mtime));
    if (Chop(value, CF_EXPANDSIZE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_MON, "env_time", value, DATA_TYPE_STRING);

    Log(LOG_LEVEL_VERBOSE, "Loading environment...");

    if ((fp = fopen(env, "r")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "\nUnable to detect environment from cf-monitord");
        return;
    }

    for(;;)
    {
        name[0] = '\0';
        value[0] = '\0';

        if (fgets(context, sizeof(context), fp) == NULL)
        {
            if (ferror(fp))
            {
                UnexpectedError("Failed to read line from stream");
                break;
            }
            else /* feof */
            {
                break;
            }
        }


        if (*context == '@')
        {
            Rlist *list = NULL;
            sscanf(context + 1, "%[^=]=%[^\n]", name, value);
           
            Log(LOG_LEVEL_DEBUG, "Setting new monitoring list '%s' => '%s'", name, value);
            list = RlistParseShown(value);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_MON, name, list, DATA_TYPE_STRING_LIST);

            RlistDestroy(list);
        }
        else if (strstr(context, "="))
        {
            sscanf(context, "%255[^=]=%255[^\n]", name, value);


/*****************************************************************************/

            if (agent_type != AGENT_TYPE_EXECUTOR)
            {
                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_MON, name, value, DATA_TYPE_STRING);
                Log(LOG_LEVEL_DEBUG, "Setting new monitoring scalar '%s' => '%s'", name, value);
            }
        }
        else
        {
            EvalContextHeapAddHard(ctx, context);
        }
    }

    fclose(fp);
    Log(LOG_LEVEL_VERBOSE, "Environment data loaded");
}

/*******************************************************************/

_Bool IsInterfaceAddress(const char *adr)
 /* Does this address belong to a local interface */
{
    Item *ip;

    for (ip = IPADDRESSES; ip != NULL; ip = ip->next)
    {
        if (strncasecmp(adr, ip->name, strlen(adr)) == 0)
        {
            Log(LOG_LEVEL_DEBUG, "Identifying '%s' as one of my interfaces", adr);
            return true;
        }
    }

    Log(LOG_LEVEL_DEBUG, "'%s' is not one of my interfaces", adr);
    return false;
}

/*******************************************************************/

void BuiltinClasses(EvalContext *ctx)
{
    char vbuff[CF_BUFSIZE];

    EvalContextHeapAddHard(ctx, "any");            /* This is a reserved word / wildcard */

    snprintf(vbuff, CF_BUFSIZE, "cfengine_%s", CanonifyName(Version()));
    CreateHardClassesFromCanonification(ctx, vbuff);

}

/*******************************************************************/

void CreateHardClassesFromCanonification(EvalContext *ctx, const char *canonified)
{
    char buf[CF_MAXVARSIZE];

    strlcpy(buf, canonified, sizeof(buf));

    EvalContextHeapAddHard(ctx, buf);

    char *sp;

    while ((sp = strrchr(buf, '_')))
    {
        *sp = 0;
        EvalContextHeapAddHard(ctx, buf);
    }
}

static void SetFlavour(EvalContext *ctx, const char *flavour)
{
    EvalContextHeapAddHard(ctx, flavour);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "flavour", flavour, DATA_TYPE_STRING);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "flavor", flavour, DATA_TYPE_STRING);
}

void OSClasses(EvalContext *ctx)
{
#ifdef __linux__
    struct stat statbuf;

/* Mandrake/Mandriva, Fedora and Oracle VM Server supply /etc/redhat-release, so
   we test for those distributions first */

    if (stat("/etc/mandriva-release", &statbuf) != -1)
    {
        Linux_Mandriva_Version(ctx);
    }
    else if (stat("/etc/mandrake-release", &statbuf) != -1)
    {
        Linux_Mandrake_Version(ctx);
    }
    else if (stat("/etc/fedora-release", &statbuf) != -1)
    {
        Linux_Fedora_Version(ctx);
    }
    else if (stat("/etc/ovs-release", &statbuf) != -1)
    {
        Linux_Oracle_VM_Server_Version(ctx);
    }
    else if (stat("/etc/redhat-release", &statbuf) != -1)
    {
        Linux_Redhat_Version(ctx);
    }

/* Oracle Linux >= 6 supplies separate /etc/oracle-release alongside
   /etc/redhat-release, use it to precisely identify version */

    if (stat("/etc/oracle-release", &statbuf) != -1)
    {
        Linux_Oracle_Version(ctx);
    }

    if (stat("/etc/generic-release", &statbuf) != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "This appears to be a sun cobalt system.");
        SetFlavour(ctx, "SunCobalt");
    }

    if (stat("/etc/SuSE-release", &statbuf) != -1)
    {
        Linux_Suse_Version(ctx);
    }

# define SLACKWARE_ANCIENT_VERSION_FILENAME "/etc/slackware-release"
# define SLACKWARE_VERSION_FILENAME "/etc/slackware-version"
    if (stat(SLACKWARE_VERSION_FILENAME, &statbuf) != -1)
    {
        Linux_Slackware_Version(ctx, SLACKWARE_VERSION_FILENAME);
    }
    else if (stat(SLACKWARE_ANCIENT_VERSION_FILENAME, &statbuf) != -1)
    {
        Linux_Slackware_Version(ctx, SLACKWARE_ANCIENT_VERSION_FILENAME);
    }

    if (stat("/etc/debian_version", &statbuf) != -1)
    {
        Linux_Debian_Version(ctx);
    }

    if (stat("/usr/bin/aptitude", &statbuf) != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "This system seems to have the aptitude package system");
        EvalContextHeapAddHard(ctx, "have_aptitude");
    }

    if (stat("/etc/UnitedLinux-release", &statbuf) != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "This appears to be a UnitedLinux system.");
        SetFlavour(ctx, "UnitedLinux");
    }

    if (stat("/etc/alpine-release", &statbuf) != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "This appears to be an AlpineLinux system.");
        SetFlavour(ctx, "alpinelinux");
    }

    if (stat("/etc/gentoo-release", &statbuf) != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "This appears to be a gentoo system.");
        SetFlavour(ctx, "gentoo");
    }

    if (stat("/etc/arch-release", &statbuf) != -1)
    {
        Log(LOG_LEVEL_VERBOSE, "This appears to be an Arch Linux system.");
        SetFlavour(ctx, "archlinux");
    }

    if (stat("/proc/vmware/version", &statbuf) != -1 || stat("/etc/vmware-release", &statbuf) != -1)
    {
        VM_Version(ctx);
    }
    else if (stat("/etc/vmware", &statbuf) != -1 && S_ISDIR(statbuf.st_mode))
    {
        VM_Version(ctx);
    }

    if (stat("/proc/xen/capabilities", &statbuf) != -1)
    {
        Xen_Domain(ctx);
    }

    if (stat("/etc/Eos-release", &statbuf) != -1)
    {
        EOS_Version(ctx);
        SetFlavour(ctx, "Eos");
    }

    if (stat("/etc/issue", &statbuf) != -1)
    {
        MiscOS(ctx);
    }
    
#ifdef XEN_CPUID_SUPPORT
    else if (Xen_Hv_Check())
    {
        Log(LOG_LEVEL_VERBOSE, "This appears to be a xen hv system.");
        EvalContextHeapAddHard(ctx, "xen");
        EvalContextHeapAddHard(ctx, "xen_domu_hv");
    }
#endif

#else

    char vbuff[CF_BUFSIZE];
    strncpy(vbuff, VSYSNAME.release, CF_MAXVARSIZE);

    for (char *sp = vbuff; *sp != '\0'; sp++)
    {
        if (*sp == '-')
        {
            *sp = '\0';
            break;
        }
    }

    char context[CF_BUFSIZE];
    snprintf(context, CF_BUFSIZE, "%s_%s", VSYSNAME.sysname, vbuff);
    SetFlavour(ctx, context);

#endif

    GetCPUInfo(ctx);

#ifdef __CYGWIN__

    for (char *sp = VSYSNAME.sysname; *sp != '\0'; sp++)
    {
        if (*sp == '-')
        {
            sp++;
            if (strncmp(sp, "5.0", 3) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "This appears to be Windows 2000");
                EvalContextHeapAddHard(ctx, "Win2000");
            }

            if (strncmp(sp, "5.1", 3) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "This appears to be Windows XP");
                EvalContextHeapAddHard(ctx, "WinXP");
            }

            if (strncmp(sp, "5.2", 3) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "This appears to be Windows Server 2003");
                EvalContextHeapAddHard(ctx, "WinServer2003");
            }

            if (strncmp(sp, "6.1", 3) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "This appears to be Windows Vista");
                EvalContextHeapAddHard(ctx, "WinVista");
            }

            if (strncmp(sp, "6.3", 3) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "This appears to be Windows Server 2008");
                EvalContextHeapAddHard(ctx, "WinServer2008");
            }
        }
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "crontab", "", DATA_TYPE_STRING);

#endif /* __CYGWIN__ */

#ifdef __MINGW32__
    EvalContextHeapAddHard(ctx, VSYSNAME.release); // code name - e.g. Windows Vista
    EvalContextHeapAddHard(ctx, VSYSNAME.version); // service pack number - e.g. Service Pack 3

    if (strstr(VSYSNAME.sysname, "workstation"))
    {
        EvalContextHeapAddHard(ctx, "WinWorkstation");
    }
    else if (strstr(VSYSNAME.sysname, "server"))
    {
        EvalContextHeapAddHard(ctx, "WinServer");
    }
    else if (strstr(VSYSNAME.sysname, "domain controller"))
    {
        EvalContextHeapAddHard(ctx, "DomainController");
        EvalContextHeapAddHard(ctx, "WinServer");
    }
    else
    {
        EvalContextHeapAddHard(ctx, "unknown_ostype");
    }

    SetFlavour(ctx, "windows");

#endif /* __MINGW32__ */

#ifndef _WIN32
    struct passwd *pw;
    if ((pw = getpwuid(getuid())) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to get username for uid '%ju'. (getpwuid: %s)", (uintmax_t)getuid(), GetErrorStr());
    }
    else
    {
        char vbuff[CF_BUFSIZE];

        if (IsDefinedClass(ctx, "SuSE", NULL))
        {
            snprintf(vbuff, CF_BUFSIZE, "/var/spool/cron/tabs/%s", pw->pw_name);
        }
        else if (IsDefinedClass(ctx, "redhat", NULL))
        {
            snprintf(vbuff, CF_BUFSIZE, "/var/spool/cron/%s", pw->pw_name);
        }
        else
        {
            snprintf(vbuff, CF_BUFSIZE, "/var/spool/cron/crontabs/%s", pw->pw_name);
        }

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "crontab", vbuff, DATA_TYPE_STRING);
    }

#endif

#if defined(__ANDROID__)
    SetFlavour(ctx, "android");
#endif

#ifdef __sun
    if (FullTextMatch(ctx, "joyent.*", VSYSNAME.version))
    {
        EvalContextHeapAddHard(ctx, "smartos");
        EvalContextHeapAddHard(ctx, "smartmachine");
    }
#endif
    
    /* FIXME: this variable needs redhat/SuSE/debian classes to be defined and
     * hence can't be initialized earlier */

    if (IsDefinedClass(ctx, "redhat", NULL))
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "doc_root", "/var/www/html", DATA_TYPE_STRING);
    }

    if (IsDefinedClass(ctx, "SuSE", NULL))
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "doc_root", "/srv/www/htdocs", DATA_TYPE_STRING);
    }

    if (IsDefinedClass(ctx, "debian", NULL))
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "doc_root", "/var/www", DATA_TYPE_STRING);
    }
}

/*********************************************************************************/

#ifdef __linux__
static void Linux_Oracle_VM_Server_Version(EvalContext *ctx)
{
    char relstring[CF_MAXVARSIZE];
    char *r;
    int major, minor, patch;
    int revcomps;

#define ORACLE_VM_SERVER_REL_FILENAME "/etc/ovs-release"
#define ORACLE_VM_SERVER_ID "Oracle VM server"

    Log(LOG_LEVEL_VERBOSE, "This appears to be Oracle VM Server");
    EvalContextHeapAddHard(ctx, "redhat");
    EvalContextHeapAddHard(ctx, "oraclevmserver");

    if (!ReadLine(ORACLE_VM_SERVER_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return;
    }

    if (strncmp(relstring, ORACLE_VM_SERVER_ID, strlen(ORACLE_VM_SERVER_ID)))
    {
        Log(LOG_LEVEL_VERBOSE, "Could not identify distribution from %s", ORACLE_VM_SERVER_REL_FILENAME);
        return;
    }

    if ((r = strstr(relstring, "release ")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find distribution version in %s", ORACLE_VM_SERVER_REL_FILENAME);
        return;
    }

    revcomps = sscanf(r + strlen("release "), "%d.%d.%d", &major, &minor, &patch);

    if (revcomps > 0)
    {
        char buf[CF_BUFSIZE];

        snprintf(buf, CF_BUFSIZE, "oraclevmserver_%d", major);
        SetFlavour(ctx, buf);
    }

    if (revcomps > 1)
    {
        char buf[CF_BUFSIZE];

        snprintf(buf, CF_BUFSIZE, "oraclevmserver_%d_%d", major, minor);
        EvalContextHeapAddHard(ctx, buf);
    }

    if (revcomps > 2)
    {
        char buf[CF_BUFSIZE];

        snprintf(buf, CF_BUFSIZE, "oraclevmserver_%d_%d_%d", major, minor, patch);
        EvalContextHeapAddHard(ctx, buf);
    }
}

/*********************************************************************************/

static void Linux_Oracle_Version(EvalContext *ctx)
{
    char relstring[CF_MAXVARSIZE];
    char *r;
    int major, minor;

#define ORACLE_REL_FILENAME "/etc/oracle-release"
#define ORACLE_ID "Oracle Linux Server"

    Log(LOG_LEVEL_VERBOSE, "This appears to be Oracle Linux");
    EvalContextHeapAddHard(ctx, "oracle");

    if (!ReadLine(ORACLE_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return;
    }

    if (strncmp(relstring, ORACLE_ID, strlen(ORACLE_ID)))
    {
        Log(LOG_LEVEL_VERBOSE, "Could not identify distribution from %s", ORACLE_REL_FILENAME);
        return;
    }

    if ((r = strstr(relstring, "release ")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find distribution version in %s", ORACLE_REL_FILENAME);
        return;
    }

    if (sscanf(r + strlen("release "), "%d.%d", &major, &minor) == 2)
    {
        char buf[CF_BUFSIZE];

        snprintf(buf, CF_BUFSIZE, "oracle_%d", major);
        SetFlavour(ctx, buf);

        snprintf(buf, CF_BUFSIZE, "oracle_%d_%d", major, minor);
        EvalContextHeapAddHard(ctx, buf);
    }
}

/*********************************************************************************/

static int Linux_Fedora_Version(EvalContext *ctx)
{
#define FEDORA_ID "Fedora"
#define RELEASE_FLAG "release "

/* We are looking for one of the following strings...
 *
 * Fedora Core release 1 (Yarrow)
 * Fedora release 7 (Zodfoobar)
 */

#define FEDORA_REL_FILENAME "/etc/fedora-release"

/* The full string read in from fedora-release */
    char relstring[CF_MAXVARSIZE];
    char classbuf[CF_MAXVARSIZE];
    char *vendor = "";
    char *release = NULL;
    int major = -1;
    char strmajor[CF_MAXVARSIZE];

    Log(LOG_LEVEL_VERBOSE, "This appears to be a fedora system.");
    EvalContextHeapAddHard(ctx, "redhat");
    EvalContextHeapAddHard(ctx, "fedora");

/* Grab the first line from the file and then close it. */

    if (!ReadLine(FEDORA_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return 1;
    }

    Log(LOG_LEVEL_VERBOSE, "Looking for fedora core linux info...");

    if (!strncmp(relstring, FEDORA_ID, strlen(FEDORA_ID)))
    {
        vendor = "fedora";
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Could not identify OS distro from %s", FEDORA_REL_FILENAME);
        return 2;
    }

/* Now, grok the release.  We assume that all the strings will
 * have the word 'release' before the numerical release.
 */

    release = strstr(relstring, RELEASE_FLAG);

    if (release == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find a numeric OS release in %s", FEDORA_REL_FILENAME);
        return 2;
    }
    else
    {
        release += strlen(RELEASE_FLAG);

        if (sscanf(release, "%d", &major) != 0)
        {
            sprintf(strmajor, "%d", major);
        }
    }

    if (major != -1 && (strcmp(vendor, "") != 0))
    {
        classbuf[0] = '\0';
        strcat(classbuf, vendor);
        EvalContextHeapAddHard(ctx,classbuf);
        strcat(classbuf, "_");
        strcat(classbuf, strmajor);
        SetFlavour(ctx, classbuf);
    }

    return 0;
}

/*********************************************************************************/

static int Linux_Redhat_Version(EvalContext *ctx)
{
#define REDHAT_ID "Red Hat Linux"
#define REDHAT_AS_ID "Red Hat Enterprise Linux AS"
#define REDHAT_AS21_ID "Red Hat Linux Advanced Server"
#define REDHAT_ES_ID "Red Hat Enterprise Linux ES"
#define REDHAT_WS_ID "Red Hat Enterprise Linux WS"
#define REDHAT_C_ID "Red Hat Enterprise Linux Client"
#define REDHAT_S_ID "Red Hat Enterprise Linux Server"
#define REDHAT_W_ID "Red Hat Enterprise Linux Workstation"
#define MANDRAKE_ID "Linux Mandrake"
#define MANDRAKE_10_1_ID "Mandrakelinux"
#define WHITEBOX_ID "White Box Enterprise Linux"
#define CENTOS_ID "CentOS"
#define SCIENTIFIC_SL_ID "Scientific Linux SL"
#define SCIENTIFIC_SL6_ID "Scientific Linux"
#define SCIENTIFIC_CERN_ID "Scientific Linux CERN"
#define RELEASE_FLAG "release "
#define ORACLE_4_5_ID "Enterprise Linux Enterprise Linux Server"

/* We are looking for one of the following strings...
 *
 * Red Hat Linux release 6.2 (Zoot)
 * Red Hat Linux Advanced Server release 2.1AS (Pensacola)
 * Red Hat Enterprise Linux AS release 3 (Taroon)
 * Red Hat Enterprise Linux WS release 3 (Taroon)
 * Red Hat Enterprise Linux Client release 5 (Tikanga)
 * Red Hat Enterprise Linux Server release 5 (Tikanga)
 * Linux Mandrake release 7.1 (helium)
 * Red Hat Enterprise Linux ES release 2.1 (Panama)
 * White Box Enterprise linux release 3.0 (Liberation)
 * Scientific Linux SL Release 4.0 (Beryllium)
 * CentOS release 4.0 (Final)
 */

#define RH_REL_FILENAME "/etc/redhat-release"

/* The full string read in from redhat-release */
    char relstring[CF_MAXVARSIZE];
    char classbuf[CF_MAXVARSIZE];

/* Red Hat, Mandrake */
    char *vendor = "";

/* as (Advanced Server, Enterprise) */
    char *edition = "";

/* Where the numerical release will be found */
    char *release = NULL;
    int i;
    int major = -1;
    char strmajor[CF_MAXVARSIZE];
    int minor = -1;
    char strminor[CF_MAXVARSIZE];

    Log(LOG_LEVEL_VERBOSE, "This appears to be a redhat (or redhat-based) system.");
    EvalContextHeapAddHard(ctx, "redhat");

/* Grab the first line from the file and then close it. */

    if (!ReadLine(RH_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return 1;
    }

    Log(LOG_LEVEL_VERBOSE, "Looking for redhat linux info in '%s'", relstring);

/* First, try to grok the vendor and the edition (if any) */
    if (!strncmp(relstring, REDHAT_ES_ID, strlen(REDHAT_ES_ID)))
    {
        vendor = "redhat";
        edition = "es";
    }
    else if (!strncmp(relstring, REDHAT_WS_ID, strlen(REDHAT_WS_ID)))
    {
        vendor = "redhat";
        edition = "ws";
    }
    else if (!strncmp(relstring, REDHAT_WS_ID, strlen(REDHAT_WS_ID)))
    {
        vendor = "redhat";
        edition = "ws";
    }
    else if (!strncmp(relstring, REDHAT_AS_ID, strlen(REDHAT_AS_ID)) ||
             !strncmp(relstring, REDHAT_AS21_ID, strlen(REDHAT_AS21_ID)))
    {
        vendor = "redhat";
        edition = "as";
    }
    else if (!strncmp(relstring, REDHAT_S_ID, strlen(REDHAT_S_ID)))
    {
        vendor = "redhat";
        edition = "s";
    }
    else if (!strncmp(relstring, REDHAT_C_ID, strlen(REDHAT_C_ID))
             || !strncmp(relstring, REDHAT_W_ID, strlen(REDHAT_W_ID)))
    {
        vendor = "redhat";
        edition = "c";
    }
    else if (!strncmp(relstring, REDHAT_ID, strlen(REDHAT_ID)))
    {
        vendor = "redhat";
    }
    else if (!strncmp(relstring, MANDRAKE_ID, strlen(MANDRAKE_ID)))
    {
        vendor = "mandrake";
    }
    else if (!strncmp(relstring, MANDRAKE_10_1_ID, strlen(MANDRAKE_10_1_ID)))
    {
        vendor = "mandrake";
    }
    else if (!strncmp(relstring, WHITEBOX_ID, strlen(WHITEBOX_ID)))
    {
        vendor = "whitebox";
    }
    else if (!strncmp(relstring, SCIENTIFIC_SL_ID, strlen(SCIENTIFIC_SL_ID)))
    {
        vendor = "scientific";
        edition = "sl";
    }
    else if (!strncmp(relstring, SCIENTIFIC_CERN_ID, strlen(SCIENTIFIC_CERN_ID)))
    {
        vendor = "scientific";
        edition = "cern";
    }
    else if (!strncmp(relstring, SCIENTIFIC_SL6_ID, strlen(SCIENTIFIC_SL6_ID)))
    {
        vendor = "scientific";
        edition = "sl";
    }
    else if (!strncmp(relstring, CENTOS_ID, strlen(CENTOS_ID)))
    {
        vendor = "centos";
    }
    else if (!strncmp(relstring, ORACLE_4_5_ID, strlen(ORACLE_4_5_ID)))
    {
        vendor = "oracle";
        edition = "s";
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Could not identify OS distro from %s", RH_REL_FILENAME);
        return 2;
    }

/* Now, grok the release.  For AS, we neglect the AS at the end of the
 * numerical release because we already figured out that it *is* AS
 * from the infomation above.  We assume that all the strings will
 * have the word 'release' before the numerical release.
 */

/* Convert relstring to lowercase so that vendors like
   Scientific Linux don't fall through the cracks.
   */

    for (i = 0; i < strlen(relstring); i++)
    {
        relstring[i] = tolower(relstring[i]);
    }

    release = strstr(relstring, RELEASE_FLAG);
    if (release == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find a numeric OS release in %s", RH_REL_FILENAME);
        return 2;
    }
    else
    {
        release += strlen(RELEASE_FLAG);
        if (sscanf(release, "%d.%d", &major, &minor) == 2)
        {
            sprintf(strmajor, "%d", major);
            sprintf(strminor, "%d", minor);
        }
        /* red hat 9 is *not* red hat 9.0.
         * and same thing with RHEL AS 3
         */
        else if (sscanf(release, "%d", &major) == 1)
        {
            sprintf(strmajor, "%d", major);
            minor = -2;
        }
    }

    if (major != -1 && minor != -1 && (strcmp(vendor, "") != 0))
    {
        classbuf[0] = '\0';
        strcat(classbuf, vendor);
        EvalContextHeapAddHard(ctx, classbuf);
        strcat(classbuf, "_");

        if (strcmp(edition, "") != 0)
        {
            strcat(classbuf, edition);
            EvalContextHeapAddHard(ctx, classbuf);
            strcat(classbuf, "_");
        }

        strcat(classbuf, strmajor);
        EvalContextHeapAddHard(ctx, classbuf);

        if (minor != -2)
        {
            strcat(classbuf, "_");
            strcat(classbuf, strminor);
            EvalContextHeapAddHard(ctx, classbuf);
        }
    }

// Now a version without the edition

    if (major != -1 && minor != -1 && (strcmp(vendor, "") != 0))
    {
        classbuf[0] = '\0';
        strcat(classbuf, vendor);
        EvalContextHeapAddHard(ctx, classbuf);
        strcat(classbuf, "_");

        strcat(classbuf, strmajor);

        SetFlavour(ctx, classbuf);

        if (minor != -2)
        {
            strcat(classbuf, "_");
            strcat(classbuf, strminor);
            EvalContextHeapAddHard(ctx, classbuf);
        }
    }

    return 0;
}

/******************************************************************/

static int Linux_Suse_Version(EvalContext *ctx)
{
#define SUSE_REL_FILENAME "/etc/SuSE-release"
/* Check if it's a SuSE Enterprise version (all in lowercase) */
#define SUSE_SLES8_ID "suse sles-8"
#define SUSE_SLES_ID  "suse linux enterprise server"
#define SUSE_SLED_ID  "suse linux enterprise desktop"
#define SUSE_RELEASE_FLAG "linux "

/* The full string read in from SuSE-release */
    char relstring[CF_MAXVARSIZE];
    char classbuf[CF_MAXVARSIZE];
    char vbuf[CF_BUFSIZE], strversion[CF_MAXVARSIZE], strpatch[CF_MAXVARSIZE];

/* Where the numerical release will be found */
    char *release = NULL;
    int i, version;
    int major = -1;
    char strmajor[CF_MAXVARSIZE];
    int minor = -1;
    char strminor[CF_MAXVARSIZE];
    FILE *fp;

    Log(LOG_LEVEL_VERBOSE, "This appears to be a SuSE system.");
    EvalContextHeapAddHard(ctx, "SuSE");

/* Grab the first line from the file and then close it. */

    fp = ReadFirstLine(SUSE_REL_FILENAME, relstring, sizeof(relstring));
    if (fp == NULL)
    {
        return 1;
    }

    strversion[0] = '\0';
    strpatch[0] = '\0';

    for(;;)
    {
        if (fgets(vbuf, sizeof(vbuf), fp) == NULL)
        {
            if (ferror(fp))
            {
                UnexpectedError("Failed to read line from stream");
                break;
            }
            else /* feof */
            {
                break;
            }
        }

        if (strncmp(vbuf, "VERSION", strlen("version")) == 0)
        {
            strncpy(strversion, vbuf, sizeof(strversion));
            sscanf(strversion, "VERSION = %d", &major);
        }

        if (strncmp(vbuf, "PATCH", strlen("PATCH")) == 0)
        {
            strncpy(strpatch, vbuf, sizeof(strpatch));
            sscanf(strpatch, "PATCHLEVEL = %d", &minor);
        }
    }

    fclose(fp);

    /* Check if it's a SuSE Enterprise version  */

    Log(LOG_LEVEL_VERBOSE, "Looking for SuSE enterprise info in '%s'", relstring);

    /* Convert relstring to lowercase to handle rename of SuSE to
     * SUSE with SUSE 10.0.
     */

    for (i = 0; i < strlen(relstring); i++)
    {
        relstring[i] = tolower(relstring[i]);
    }

    /* Check if it's a SuSE Enterprise version (all in lowercase) */

    if (!strncmp(relstring, SUSE_SLES8_ID, strlen(SUSE_SLES8_ID)))
    {
        classbuf[0] = '\0';
        strcat(classbuf, "SLES8");
        EvalContextHeapAddHard(ctx, classbuf);
    }
    else if (strncmp(relstring, "sles", 4) == 0)
    {
        Item *list, *ip;

        sscanf(relstring, "%[-_a-zA-Z0-9]", vbuf);
        EvalContextHeapAddHard(ctx, vbuf);

        list = SplitString(vbuf, '-');

        for (ip = list; ip != NULL; ip = ip->next)
        {
            EvalContextHeapAddHard(ctx, ip->name);
        }

        DeleteItemList(list);
    }
    else
    {
        for (version = 9; version < 13; version++)
        {
            snprintf(vbuf, CF_BUFSIZE, "%s %d ", SUSE_SLES_ID, version);
            Log(LOG_LEVEL_DEBUG, "Checking for SUSE [%s]", vbuf);

            if (!strncmp(relstring, vbuf, strlen(vbuf)))
            {
                snprintf(classbuf, CF_MAXVARSIZE, "SLES%d", version);
                EvalContextHeapAddHard(ctx, classbuf);
            }
            else
            {
                snprintf(vbuf, CF_BUFSIZE, "%s %d ", SUSE_SLED_ID, version);
                Log(LOG_LEVEL_DEBUG, "Checking for SUSE [%s]", vbuf);

                if (!strncmp(relstring, vbuf, strlen(vbuf)))
                {
                    snprintf(classbuf, CF_MAXVARSIZE, "SLED%d", version);
                    EvalContextHeapAddHard(ctx, classbuf);
                }
            }
        }
    }

    /* Determine release version. We assume that the version follows
     * the string "SuSE Linux" or "SUSE LINUX".
     */

    release = strstr(relstring, SUSE_RELEASE_FLAG);

    if (release == NULL)
    {
        release = strstr(relstring, "opensuse");
    }

    if (release == NULL)
    {
        release = strversion;
    }

    if (release == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find a numeric OS release in %s", SUSE_REL_FILENAME);
        return 2;
    }
    else
    {
        if (strchr(release, '.'))
        {
            sscanf(release, "%*s %d.%d", &major, &minor);
            sprintf(strmajor, "%d", major);
            sprintf(strminor, "%d", minor);

            if (major != -1 && minor != -1)
            {
                strcpy(classbuf, "SuSE");
                EvalContextHeapAddHard(ctx, classbuf);
                strcat(classbuf, "_");
                strcat(classbuf, strmajor);
                SetFlavour(ctx, classbuf);
                strcat(classbuf, "_");
                strcat(classbuf, strminor);
                EvalContextHeapAddHard(ctx, classbuf);

                Log(LOG_LEVEL_VERBOSE, "Discovered SuSE version %s", classbuf);
                return 0;
            }
        }
        else
        {
            sscanf(strversion, "VERSION = %s", strmajor);
            sscanf(strpatch, "PATCHLEVEL = %s", strminor);

            if (major != -1 && minor != -1)
            {
                strcpy(classbuf, "SLES");
                EvalContextHeapAddHard(ctx, classbuf);
                strcat(classbuf, "_");
                strcat(classbuf, strmajor);
                EvalContextHeapAddHard(ctx, classbuf);
                strcat(classbuf, "_");
                strcat(classbuf, strminor);
                EvalContextHeapAddHard(ctx, classbuf);
                snprintf(classbuf, CF_MAXVARSIZE, "SuSE_%d", major);
                SetFlavour(ctx, classbuf);

                Log(LOG_LEVEL_VERBOSE, "Discovered SuSE version %s", classbuf);
                return 0;
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Could not find a numeric OS release in %s", SUSE_REL_FILENAME);

    return 0;
}

/******************************************************************/

static int Linux_Slackware_Version(EvalContext *ctx, char *filename)
{
    int major = -1;
    int minor = -1;
    int release = -1;
    char classname[CF_MAXVARSIZE] = "";
    char buffer[CF_MAXVARSIZE];

    Log(LOG_LEVEL_VERBOSE, "This appears to be a slackware system.");
    EvalContextHeapAddHard(ctx, "slackware");

    if (!ReadLine(filename, buffer, sizeof(buffer)))
    {
        return 1;
    }

    Log(LOG_LEVEL_VERBOSE, "Looking for Slackware version...");
    switch (sscanf(buffer, "Slackware %d.%d.%d", &major, &minor, &release))
    {
    case 3:
        Log(LOG_LEVEL_VERBOSE, "This appears to be a Slackware %u.%u.%u system.", major, minor, release);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u_%u_%u", major, minor, release);
        EvalContextHeapAddHard(ctx, classname);
        /* Fall-through */
    case 2:
        Log(LOG_LEVEL_VERBOSE, "This appears to be a Slackware %u.%u system.", major, minor);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u_%u", major, minor);
        EvalContextHeapAddHard(ctx, classname);
        /* Fall-through */
    case 1:
        Log(LOG_LEVEL_VERBOSE, "This appears to be a Slackware %u system.", major);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u", major);
        EvalContextHeapAddHard(ctx, classname);
        break;
    case 0:
        Log(LOG_LEVEL_VERBOSE, "No Slackware version number found.");
        return 2;
    }
    return 0;
}

/*
 * @brief : /etc/issue on debian can include special characters
 *          escaped with '/' or '@'. This function will get rid
 *          them.
 *
 * @param[in,out] buffer: string to be sanitized
 *
 * @return : 0 if everything went fine, <>0 otherwise
 */
static int LinuxDebianSanitizeIssue(char *buffer)
{
    bool escaped = false;
    char *s2, *s;
    s2 = buffer;
    for (s = buffer; *s != '\0'; s++)
    {
        if (*s=='\\' || *s=='@')
        {
             if (escaped == false)
             {
                 escaped = true;
             }
             else 
             {
                 escaped = false;
             }
        }
        else
        {
             if (escaped == false)
             {
                 *s2 = *s;
                 s2++;
             }
             else 
             {
                 escaped = false;
             }
        }
    }
    *s2 = '\0';
    s2--;
    while (*s2==' ')
    {
        *s2='\0'; 
        s2--;
    }
    return 0;
}

/******************************************************************/
static int Linux_Debian_Version(EvalContext *ctx)
{
#define DEBIAN_VERSION_FILENAME "/etc/debian_version"
#define DEBIAN_ISSUE_FILENAME "/etc/issue"
    int major = -1;
    int release = -1;
    int result;
    char classname[CF_MAXVARSIZE], buffer[CF_MAXVARSIZE], os[CF_MAXVARSIZE], version[CF_MAXVARSIZE];

    Log(LOG_LEVEL_VERBOSE, "This appears to be a debian system.");
    EvalContextHeapAddHard(ctx, "debian");

    buffer[0] = classname[0] = '\0';

    Log(LOG_LEVEL_VERBOSE, "Looking for Debian version...");

    if (!ReadLine(DEBIAN_VERSION_FILENAME, buffer, sizeof(buffer)))
    {
        return 1;
    }

    result = sscanf(buffer, "%d.%d", &major, &release);

    switch (result)
    {
    case 2:
        Log(LOG_LEVEL_VERBOSE, "This appears to be a Debian %u.%u system.", major, release);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u_%u", major, release);
        EvalContextHeapAddHard(ctx, classname);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u", major);
        SetFlavour(ctx, classname);
        break;
        /* Fall-through */
    case 1:
        Log(LOG_LEVEL_VERBOSE, "This appears to be a Debian %u system.", major);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u", major);
        SetFlavour(ctx, classname);
        break;

    default:
        version[0] = '\0';
        sscanf(buffer, "%25[^/]", version);
        if (strlen(version) > 0)
        {
            snprintf(classname, CF_MAXVARSIZE, "debian_%s", version);
            EvalContextHeapAddHard(ctx, classname);
        }
        break;
    }

    if (!ReadLine(DEBIAN_ISSUE_FILENAME, buffer, sizeof(buffer)))
    {
        return 1;
    }
    
    os[0] = '\0';
    sscanf(buffer, "%250s", os);

    if (strcmp(os, "Debian") == 0)
    {
        LinuxDebianSanitizeIssue(buffer);
        sscanf(buffer, "%*s %*s %[^./]", version);
        snprintf(buffer, CF_MAXVARSIZE, "debian_%s", version);
        EvalContextHeapAddHard(ctx, "debian");
        SetFlavour(ctx, buffer);
    }
    else if (strcmp(os, "Ubuntu") == 0)
    {
        LinuxDebianSanitizeIssue(buffer);
        sscanf(buffer, "%*s %[^.].%d", version, &release);
        snprintf(buffer, CF_MAXVARSIZE, "ubuntu_%s", version);
        SetFlavour(ctx, buffer);
        EvalContextHeapAddHard(ctx, "ubuntu");
        if (release >= 0)
        {
            snprintf(buffer, CF_MAXVARSIZE, "ubuntu_%s_%d", version, release);
            EvalContextHeapAddHard(ctx, buffer);
        }
    }

    return 0;
}

/******************************************************************/

static int Linux_Mandrake_Version(EvalContext *ctx)
{
/* We are looking for one of the following strings... */
#define MANDRAKE_ID "Linux Mandrake"
#define MANDRAKE_REV_ID "Mandrake Linux"
#define MANDRAKE_10_1_ID "Mandrakelinux"

#define MANDRAKE_REL_FILENAME "/etc/mandrake-release"

    char relstring[CF_MAXVARSIZE];
    char *vendor = NULL;

    Log(LOG_LEVEL_VERBOSE, "This appears to be a mandrake system.");
    EvalContextHeapAddHard(ctx, "Mandrake");

    if (!ReadLine(MANDRAKE_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return 1;
    }

    Log(LOG_LEVEL_VERBOSE, "Looking for Mandrake linux info in '%s'", relstring);

/* Older Mandrakes had the 'Mandrake Linux' string in reverse order */

    if (!strncmp(relstring, MANDRAKE_ID, strlen(MANDRAKE_ID)))
    {
        vendor = "mandrake";
    }
    else if (!strncmp(relstring, MANDRAKE_REV_ID, strlen(MANDRAKE_REV_ID)))
    {
        vendor = "mandrake";
    }

    else if (!strncmp(relstring, MANDRAKE_10_1_ID, strlen(MANDRAKE_10_1_ID)))
    {
        vendor = "mandrake";
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Could not identify OS distro from %s", MANDRAKE_REL_FILENAME);
        return 2;
    }

    return Linux_Mandriva_Version_Real(ctx, MANDRAKE_REL_FILENAME, relstring, vendor);
}

/******************************************************************/

static int Linux_Mandriva_Version(EvalContext *ctx)
{
/* We are looking for the following strings... */
#define MANDRIVA_ID "Mandriva Linux"

#define MANDRIVA_REL_FILENAME "/etc/mandriva-release"

    char relstring[CF_MAXVARSIZE];
    char *vendor = NULL;

    Log(LOG_LEVEL_VERBOSE, "This appears to be a mandriva system.");
    EvalContextHeapAddHard(ctx, "Mandrake");
    EvalContextHeapAddHard(ctx, "Mandriva");

    if (!ReadLine(MANDRIVA_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return 1;
    }

    Log(LOG_LEVEL_VERBOSE, "Looking for Mandriva linux info in '%s'", relstring);

    if (!strncmp(relstring, MANDRIVA_ID, strlen(MANDRIVA_ID)))
    {
        vendor = "mandriva";
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Could not identify OS distro from '%s'", MANDRIVA_REL_FILENAME);
        return 2;
    }

    return Linux_Mandriva_Version_Real(ctx, MANDRIVA_REL_FILENAME, relstring, vendor);

}

/******************************************************************/

static int Linux_Mandriva_Version_Real(EvalContext *ctx, char *filename, char *relstring, char *vendor)
{
    char *release = NULL;
    char classbuf[CF_MAXVARSIZE];
    int major = -1;
    char strmajor[CF_MAXVARSIZE];
    int minor = -1;
    char strminor[CF_MAXVARSIZE];

#define RELEASE_FLAG "release "
    release = strstr(relstring, RELEASE_FLAG);
    if (release == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find a numeric OS release in %s", filename);
        return 2;
    }
    else
    {
        release += strlen(RELEASE_FLAG);
        if (sscanf(release, "%d.%d", &major, &minor) == 2)
        {
            sprintf(strmajor, "%d", major);
            sprintf(strminor, "%d", minor);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Could not break down release version numbers in %s", filename);
        }
    }

    if (major != -1 && minor != -1 && strcmp(vendor, ""))
    {
        classbuf[0] = '\0';
        strcat(classbuf, vendor);
        EvalContextHeapAddHard(ctx, classbuf);
        strcat(classbuf, "_");
        strcat(classbuf, strmajor);
        EvalContextHeapAddHard(ctx, classbuf);
        if (minor != -2)
        {
            strcat(classbuf, "_");
            strcat(classbuf, strminor);
            EvalContextHeapAddHard(ctx, classbuf);
        }
    }

    return 0;
}

/******************************************************************/

static int EOS_Version(EvalContext *ctx)

{ char buffer[CF_BUFSIZE];

 // e.g. Arista Networks EOS 4.10.2
 
    if (ReadLine("/etc/Eos-release", buffer, sizeof(buffer)))
    {
        if (strstr(buffer, "EOS"))
        {
            char version[CF_MAXVARSIZE], class[CF_MAXVARSIZE];
            EvalContextHeapAddHard(ctx, "eos");
            EvalContextHeapAddHard(ctx, "arista");
            version[0] = '\0';
            sscanf(buffer, "%*s %*s %*s %s", version);
            CanonifyNameInPlace(version);
            snprintf(class, CF_MAXVARSIZE, "eos_%s", version);
            EvalContextHeapAddHard(ctx, class);
        }
    }
    
    return 0;
}

/******************************************************************/

static int MiscOS(EvalContext *ctx)

{ char buffer[CF_BUFSIZE];

 // e.g. BIG-IP 10.1.0 Build 3341.1084
 
    if (ReadLine("/etc/issue", buffer, sizeof(buffer)))
    {
       if (strstr(buffer, "BIG-IP"))
       {
           char version[CF_MAXVARSIZE], build[CF_MAXVARSIZE], class[CF_MAXVARSIZE];
           EvalContextHeapAddHard(ctx, "big_ip");
           sscanf(buffer, "%*s %s %*s %s", version, build);
           CanonifyNameInPlace(version);
           CanonifyNameInPlace(build);
           snprintf(class, CF_MAXVARSIZE, "big_ip_%s", version);
           EvalContextHeapAddHard(ctx, class);
           snprintf(class, CF_MAXVARSIZE, "big_ip_%s_%s", version, build);
           EvalContextHeapAddHard(ctx, class);
           SetFlavour(ctx, "BIG-IP");
       }
    }
    
    return 0;
}

/******************************************************************/

static int VM_Version(EvalContext *ctx)
{
    char *sp, buffer[CF_BUFSIZE], classbuf[CF_BUFSIZE], version[CF_BUFSIZE];
    int major, minor, bug;
    int sufficient = 0;

    Log(LOG_LEVEL_VERBOSE, "This appears to be a VMware Server ESX/xSX system.");
    EvalContextHeapAddHard(ctx, "VMware");

/* VMware Server ESX >= 3 has version info in /proc */
    if (ReadLine("/proc/vmware/version", buffer, sizeof(buffer)))
    {
        if (sscanf(buffer, "VMware ESX Server %d.%d.%d", &major, &minor, &bug) > 0)
        {
            snprintf(classbuf, CF_BUFSIZE, "VMware ESX Server %d", major);
            EvalContextHeapAddHard(ctx, classbuf);
            snprintf(classbuf, CF_BUFSIZE, "VMware ESX Server %d.%d", major, minor);
            EvalContextHeapAddHard(ctx, classbuf);
            snprintf(classbuf, CF_BUFSIZE, "VMware ESX Server %d.%d.%d", major, minor, bug);
            EvalContextHeapAddHard(ctx, classbuf);
            sufficient = 1;
        }
        else if (sscanf(buffer, "VMware ESX Server %s", version) > 0)
        {
            snprintf(classbuf, CF_BUFSIZE, "VMware ESX Server %s", version);
            EvalContextHeapAddHard(ctx, classbuf);
            sufficient = 1;
        }
    }

/* Fall back to checking for other files */

    if (sufficient < 1 && (ReadLine("/etc/vmware-release", buffer, sizeof(buffer))
                           || ReadLine("/etc/issue", buffer, sizeof(buffer))))
    {
        EvalContextHeapAddHard(ctx, buffer);

        /* Strip off the release code name e.g. "(Dali)" */
        if ((sp = strchr(buffer, '(')) != NULL)
        {
            *sp = 0;
            Chop(buffer, CF_EXPANDSIZE);
            EvalContextHeapAddHard(ctx, buffer);
        }
        sufficient = 1;
    }

    return sufficient < 1 ? 1 : 0;
}

/******************************************************************/

static int Xen_Domain(EvalContext *ctx)
{
    FILE *fp;
    char buffer[CF_BUFSIZE];
    int sufficient = 0;

    Log(LOG_LEVEL_VERBOSE, "This appears to be a xen pv system.");
    EvalContextHeapAddHard(ctx, "xen");

/* xen host will have "control_d" in /proc/xen/capabilities, xen guest will not */

    if ((fp = fopen("/proc/xen/capabilities", "r")) != NULL)
    {
        for (;;)
        {
            ssize_t res = CfReadLine(buffer, CF_BUFSIZE, fp);
            if (res == 0)
            {
                break;
            }

            if (res == -1)
            {
                /* Failure reading Xen capabilites. Do we care? */
                fclose(fp);
                return 1;
            }

            if (strstr(buffer, "control_d"))
            {
                EvalContextHeapAddHard(ctx, "xen_dom0");
                sufficient = 1;
            }
        }

        if (!sufficient)
        {
            EvalContextHeapAddHard(ctx, "xen_domu_pv");
            sufficient = 1;
        }

        fclose(fp);
    }

    return sufficient < 1 ? 1 : 0;
}

/******************************************************************/

#ifdef XEN_CPUID_SUPPORT

/* borrowed from Xen source/tools/libxc/xc_cpuid_x86.c */

static void Xen_Cpuid(uint32_t idx, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    asm(
           /* %ebx register need to be saved before usage and restored thereafter
            * for PIC-compliant code on i386 */
# ifdef __i386__
           "push %%ebx; cpuid; mov %%ebx,%1; pop %%ebx"
# else
           "push %%rbx; cpuid; mov %%ebx,%1; pop %%rbx"
# endif
  : "=a"(*eax), "=r"(*ebx), "=c"(*ecx), "=d"(*edx):"0"(idx), "2"(0));
}

/******************************************************************/

static int Xen_Hv_Check(void)
{
    uint32_t eax;
    union
    {
        uint32_t u[3];
        char s[13];
    } sig = {{0}};

    Xen_Cpuid(0x40000000, &eax, &sig.u[0], &sig.u[1], &sig.u[2]);

    if (strcmp("XenVMMXenVMM", sig.s) || (eax < 0x40000002))
    {
        return 0;
    }

    Xen_Cpuid(0x40000001, &eax, &sig.u[0], &sig.u[1], &sig.u[2]);
    return 1;
}

#endif

/******************************************************************/

static bool ReadLine(const char *filename, char *buf, int bufsize)
{
    FILE *fp = ReadFirstLine(filename, buf, bufsize);

    if (fp == NULL)
    {
        return false;
    }
    else
    {
        fclose(fp);
        return true;
    }
}

static FILE *ReadFirstLine(const char *filename, char *buf, int bufsize)
{
    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
    {
        return NULL;
    }

    if (fgets(buf, bufsize, fp) == NULL)
    {
        fclose(fp);
        return NULL;
    }

    StripTrailingNewline(buf, CF_EXPANDSIZE);

    return fp;
}
#endif /* __linux__ */

/******************************************************************/
/* User info                                                      */
/******************************************************************/

#if defined(__CYGWIN__)

static const char *GetDefaultWorkDir(void)
{
    return WORKDIR;
}

#elif defined(__ANDROID__)

static const char *GetDefaultWorkDir(void)
{
    /* getpwuid() on Android returns /data, so use compile-time default instead */
    return WORKDIR;
}

#elif !defined(__MINGW32__)

#define MAX_WORKDIR_LENGTH (CF_BUFSIZE / 2)

static const char *GetDefaultWorkDir(void)
{
    if (getuid() > 0)
    {
        static char workdir[MAX_WORKDIR_LENGTH];

        if (!*workdir)
        {
            struct passwd *mpw = getpwuid(getuid());

            if (snprintf(workdir, MAX_WORKDIR_LENGTH, "%s/.cfagent", mpw->pw_dir) >= MAX_WORKDIR_LENGTH)
            {
                return NULL;
            }
        }
        return workdir;
    }
    else
    {
        return WORKDIR;
    }
}

#endif

/******************************************************************/

const char *GetWorkDir(void)
{
    const char *workdir = getenv("CFENGINE_TEST_OVERRIDE_WORKDIR");

    return workdir == NULL ? GetDefaultWorkDir() : workdir;
}

/******************************************************************/

static void GetCPUInfo(EvalContext *ctx)
{
#if defined(MINGW) || defined(NT)
    Log(LOG_LEVEL_VERBOSE, "!! cpu count not implemented on Windows platform");
    return;
#else
    char buf[CF_SMALLBUF] = "1_cpu";
    int count = 0;
#endif

    // http://preview.tinyurl.com/c9l2sh - StackOverflow on cross-platform CPU counting
#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
    // Linux, AIX, Solaris, Darwin >= 10.4
    count = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif

#if defined(HAVE_SYS_SYSCTL_H) && defined(HW_NCPU)
    // BSD-derived platforms
    int mib[2] = { CTL_HW, HW_NCPU };
    size_t len;

    len = sizeof(count);
    if(sysctl(mib, 2, &count, &len, NULL, 0) < 0)
    {
        Log(LOG_LEVEL_ERR, "sysctl", "!! failed to get cpu count: %s", strerror(errno));
    }
#endif

#ifdef HAVE_SYS_MPCTL_H
// Itanium processors have Intel Hyper-Threading virtual-core capability,
// and the MPC_GETNUMCORES_SYS operation counts each HT virtual core,
// which is equivalent to what the /proc/stat scan delivers for Linux.
//
// A build on 11i v3 PA would have the GETNUMCORES define, but if run on an
// 11i v1 system it would fail since that OS release has only GETNUMSPUS.
// So in the presence of GETNUMCORES, we check for an invalid arg error
// and fall back to GETNUMSPUS if necessary. An 11i v1 build would work
// normally on 11i v3, because on PA-RISC cores == spus since there's no
// HT on PA-RISC, and 11i v1 only runs on PA-RISC.
#ifdef MPC_GETNUMCORES_SYS
    if ((count = mpctl(MPC_GETNUMCORES_SYS, 0, 0)) == -1) {
        if (errno == EINVAL) {
            count = mpctl(MPC_GETNUMSPUS_SYS, 0, 0);
        }
    }
#else
    count = mpctl(MPC_GETNUMSPUS_SYS, 0, 0);	// PA-RISC processor count
#endif
#endif /* HAVE_SYS_MPCTL_H */

    if (count < 1)
    {
        Log(LOG_LEVEL_VERBOSE, "invalid processor count: %d", count);
        return;
    }
    Log(LOG_LEVEL_VERBOSE, "Found %d processor%s", count, count > 1 ? "s" : "");

    if (count == 1) {
        EvalContextHeapAddHard(ctx, buf);  // "1_cpu" from init - change if buf is ever used above
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cpus", "1", DATA_TYPE_STRING);
    } else {
        snprintf(buf, CF_SMALLBUF, "%d_cpus", count);
        EvalContextHeapAddHard(ctx, buf);
        snprintf(buf, CF_SMALLBUF, "%d", count);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "cpus", buf, DATA_TYPE_STRING);
    }
}

/******************************************************************/

int GetUptimeMinutes(time_t now)
// Return the number of minutes the system has been online given the current
// time() as an argument, or return -1 if unavailable or unimplemented.
{
#ifdef CF_SYS_UPTIME_IMPLEMENTED
    time_t boot_time = 0;
    errno = 0;
#endif

#if defined(BOOT_TIME_WITH_SYSINFO)         // Most GNU, Linux platforms
    struct sysinfo s;

    if (sysinfo(&s) == 0) 
    {
       // Don't return yet, sanity checking below
       boot_time = now - s.uptime;
    }

#elif defined(BOOT_TIME_WITH_KSTAT)         // Solaris platform
#define NANOSECONDS_PER_SECOND 1000000000
    kstat_ctl_t *kc;
    kstat_t *kp;

    if(kc = kstat_open())
    {
        if(kp = kstat_lookup(kc, "unix", 0, "system_misc"))
        {
            boot_time = (time_t)(kp->ks_crtime / NANOSECONDS_PER_SECOND);
        }
        kstat_close(kc);
    }

#elif defined(BOOT_TIME_WITH_PSTAT_GETPROC) // HP-UX platform only
    struct pst_status p;

    if (pstat_getproc(&p, sizeof(p), 0, 1) == 1)
    {
        boot_time = (time_t)p.pst_start;
    }

#elif defined(BOOT_TIME_WITH_SYSCTL)        // BSD-derived platforms
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    struct timeval boot;
    size_t len;

    len = sizeof(boot);
    if (sysctl(mib, 2, &boot, &len, NULL, 0) == 0)
    {
        boot_time = boot.tv_sec;
    }

#elif defined(BOOT_TIME_WITH_PROCFS)        // Second-to-last resort: procfs
    struct stat p;

    if (stat("/proc/1", &p) == 0)
    {
        boot_time = p.st_ctime;
    }

#endif

#ifdef CF_SYS_UPTIME_IMPLEMENTED
    if(errno)
    {
        Log(LOG_LEVEL_ERR, "boot time discovery error: %s", GetErrorStr());
    }

    if(boot_time > now || boot_time <= 0)
    {
        Log(LOG_LEVEL_DEBUG, "invalid boot time found; trying uptime command");
        boot_time = GetBootTimeFromUptimeCommand(now);
    }

    return(boot_time > 0 ? ((now - boot_time) / SECONDS_PER_MINUTE) : -1);
#else
// Native NT build: MINGW; NT build: NT
// Maybe use "ULONGLONG WINAPI GetTickCount()" on Windows?
    Log(LOG_LEVEL_VERBOSE, "$(sys.uptime) is not implemented on this platform");
    return(-1);
#endif
}

/******************************************************************/

#ifdef CF_SYS_UPTIME_IMPLEMENTED
// Last resort: parse the output of the uptime command with a PCRE regexp
// and convert the uptime to boot time using "now" argument.
//
// The regexp needs to match all variants of the uptime command's output.
// Solaris 8:     10:45am up 109 day(s), 19:56, 1 user, load average: 
// HP-UX 11.11:   9:24am  up 1 min,  1 user,  load average:
//                8:23am  up 23 hrs,  0 users,  load average:
//                9:33am  up 2 days, 10 mins,  0 users,  load average:
//                11:23am  up 2 days, 2 hrs,  0 users,  load average:
// Red Hat Linux: 10:51:23 up 5 days, 19:54, 1 user, load average: 
//
// UPTIME_BACKREFS must be set to this regexp's maximum backreference
// index number (i.e., the count of left-parentheses):
#define UPTIME_REGEXP " up (\\d+ day[^,]*,|) *(\\d+( ho?u?r|:(\\d+))|(\\d+) min)"
#define UPTIME_BACKREFS 5
#define UPTIME_OVECTOR ((UPTIME_BACKREFS + 1) * 3)

static time_t GetBootTimeFromUptimeCommand(time_t now)
{
    FILE *uptimecmd;
    pcre *rx;
    int ovector[UPTIME_OVECTOR], i, seconds;
    char uptime_output[CF_SMALLBUF] = { '\0' }, *backref;
    const char *uptimepath = "/usr/bin/uptime";
    time_t uptime = 0;
    const char *errptr;
    int erroffset;
    
    rx = pcre_compile(UPTIME_REGEXP, 0, &errptr, &erroffset, NULL);
    if (rx == NULL)
    {
        Log(LOG_LEVEL_DEBUG, "failed to compile regexp to parse uptime command"); 
        return(-1);
    }

    // Try "/usr/bin/uptime" first, then "/bin/uptime"
    uptimecmd = cf_popen(uptimepath, "r", false);
    uptimecmd = uptimecmd ? uptimecmd : cf_popen((uptimepath + 4), "r", false);
    if (!uptimecmd)
    {
        Log(LOG_LEVEL_ERR, "uptime failed: (cf_popen: %s)", GetErrorStr());
        return -1;
    }
    i = CfReadLine(uptime_output, CF_SMALLBUF, uptimecmd);
    cf_pclose(uptimecmd);
    if (i < 0)
    {
        Log(LOG_LEVEL_ERR, "Reading uptime output failed. (CfReadLine: '%s')", GetErrorStr());
        return -1;
    }

    if ((i != 0) && (pcre_exec(rx, NULL, (const char *)uptime_output, i, 0, 0, ovector, UPTIME_OVECTOR) > 1))
    {
        for (i = 1; i <= UPTIME_BACKREFS ; i++)
        {
            if (ovector[i * 2 + 1] - ovector[i * 2] == 0) // strlen(backref)
            {
                continue;
            }
            backref = uptime_output + ovector[i * 2];
            // atoi() ignores non-digits, so no need to null-terminate backref
            switch(i)
            {
                case 1: // Day
                    seconds = SECONDS_PER_DAY;
                    break;
                case 2: // Hour
                    seconds = SECONDS_PER_HOUR;
                    break;
                case 4: // Minute
                case 5:
                    seconds = SECONDS_PER_MINUTE;
                    break;
                default:
                    seconds = 0;
             }
             uptime += atoi(backref) * seconds;
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "uptime PCRE match failed: regexp: '%s', uptime: '%s'", UPTIME_REGEXP, uptime_output);
    }
    pcre_free(rx);
    return(uptime ? (now - uptime) : -1);
}
#endif
