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

#include "sysinfo.h"

#include "cf3.extern.h"

#include "env_context.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "vars.h"
#include "item_lib.h"
#include "matching.h"
#include "unix.h"
#include "cfstream.h"
#include "string_lib.h"
#include "logging.h"
#include "misc_lib.h"
#include "rlist.h"

#ifdef HAVE_ZONE_H
# include <zone.h>
#endif

// HP-UX mpctl() for $(sys.cpus) on HP-UX - Mantis #1069
#ifdef HAVE_SYS_MPCTL_H
# include <sys/mpctl.h>
#endif

// BSD: sysctl(3) to get kern.boottime, CPU count, etc.
// See http://www.unix.com/man-page/FreeBSD/3/sysctl/
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

void CalculateDomainName(const char *nodename, const char *dnsname, char *fqname, char *uqname, char *domain);

#ifdef __linux__
static int Linux_Fedora_Version(void);
static int Linux_Redhat_Version(void);
static void Linux_Oracle_VM_Server_Version(void);
static void Linux_Oracle_Version(void);
static int Linux_Suse_Version(void);
static int Linux_Slackware_Version(char *filename);
static int Linux_Debian_Version(void);
static int Linux_Mandrake_Version(void);
static int Linux_Mandriva_Version(void);
static int Linux_Mandriva_Version_Real(char *filename, char *relstring, char *vendor);
static int VM_Version(void);
static int Xen_Domain(void);
static int EOS_Version(void);
static int MiscOS(void);

#ifdef XEN_CPUID_SUPPORT
static void Xen_Cpuid(uint32_t idx, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
static int Xen_Hv_Check(void);
#endif

static bool ReadLine(const char *filename, char *buf, int bufsize);
static FILE *ReadFirstLine(const char *filename, char *buf, int bufsize);
#endif

static void GetCPUInfo(void);

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

void DetectDomainName(const char *orig_nodename)
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
        HardClass(ptr);

        ptr = strchr(ptr, '.');
        if (ptr != NULL)
            ptr++;
    }
    while (ptr != NULL);

    HardClass(VUQNAME);
    HardClass(VDOMAIN);

    NewScalar("sys", "host", nodename, DATA_TYPE_STRING);
    NewScalar("sys", "uqhost", VUQNAME, DATA_TYPE_STRING);
    NewScalar("sys", "fqhost", VFQNAME, DATA_TYPE_STRING);
    NewScalar("sys", "domain", VDOMAIN, DATA_TYPE_STRING);
}

/*******************************************************************/

void GetNameInfo3()
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

    CfDebug("GetNameInfo()\n");

    if (uname(&VSYSNAME) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "uname", "!!! Couldn't get kernel name info!");
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

    for (i = 0; i < PLATFORM_CONTEXT_MAX; i++)
    {
        char sysname[CF_BUFSIZE];
        strlcpy(sysname, VSYSNAME.sysname, CF_BUFSIZE);
        ToLowerStrInplace(sysname);

        if (FullTextMatch(CLASSATTRIBUTES[i][0], sysname))
        {
            if (FullTextMatch(CLASSATTRIBUTES[i][1], VSYSNAME.machine))
            {
                if (FullTextMatch(CLASSATTRIBUTES[i][2], VSYSNAME.release))
                {
                    HardClass(CLASSTEXT[i]);

                    found = true;

                    VSYSTEMHARDCLASS = (PlatformContext) i;
                    NewScalar("sys", "class", CLASSTEXT[i], DATA_TYPE_STRING);
                    break;
                }
            }
            else
            {
                CfDebug("Cfengine: I recognize %s but not %s\n", VSYSNAME.sysname, VSYSNAME.machine);
                continue;
            }
        }
    }

    if (!found)
    {
        i = 0;
    }

/*
 * solarisx86 is a historically defined class for Solaris on x86. We have to
 * define it manually now.
 */
#ifdef __sun
    if (strcmp(VSYSNAME.machine, "i86pc") == 0)
    {
        HardClass("solarisx86");
    }
#endif

    DetectDomainName(VSYSNAME.nodename);

    if ((tloc = time((time_t *) NULL)) == -1)
    {
        printf("Couldn't read system clock\n");
    }

    snprintf(workbuf, CF_BUFSIZE, "%s", CLASSTEXT[i]);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "%s", NameVersion());

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "------------------------------------------------------------------------\n\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host name is: %s\n", VSYSNAME.nodename);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Operating System Type is %s\n", VSYSNAME.sysname);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Operating System Release is %s\n", VSYSNAME.release);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Architecture = %s\n\n\n", VSYSNAME.machine);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Using internal soft-class %s for host %s\n\n", workbuf, VSYSNAME.nodename);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "The time is now %s\n\n", cf_ctime(&tloc));
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "------------------------------------------------------------------------\n\n");

    snprintf(workbuf, CF_MAXVARSIZE, "%s", cf_ctime(&tloc));
    if (Chop(workbuf, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }

    NewScalar("sys", "date", workbuf, DATA_TYPE_STRING);
    NewScalar("sys", "cdate", CanonifyName(workbuf), DATA_TYPE_STRING);
    NewScalar("sys", "os", VSYSNAME.sysname, DATA_TYPE_STRING);
    NewScalar("sys", "release", VSYSNAME.release, DATA_TYPE_STRING);
    NewScalar("sys", "version", VSYSNAME.version, DATA_TYPE_STRING);
    NewScalar("sys", "arch", VSYSNAME.machine, DATA_TYPE_STRING);
    NewScalar("sys", "workdir", CFWORKDIR, DATA_TYPE_STRING);
    NewScalar("sys", "fstab", VFSTAB[VSYSTEMHARDCLASS], DATA_TYPE_STRING);
    NewScalar("sys", "resolv", VRESOLVCONF[VSYSTEMHARDCLASS], DATA_TYPE_STRING);
    NewScalar("sys", "maildir", VMAILDIR[VSYSTEMHARDCLASS], DATA_TYPE_STRING);
    NewScalar("sys", "exports", VEXPORTS[VSYSTEMHARDCLASS], DATA_TYPE_STRING);
    NewScalar("sys", "expires", EXPIRY, DATA_TYPE_STRING);
/* FIXME: type conversion */
    NewScalar("sys", "cf_version", (char *) Version(), DATA_TYPE_STRING);

    if (PUBKEY)
    {
        HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);
        snprintf(PUBKEY_DIGEST, sizeof(PUBKEY_DIGEST), "%s", HashPrint(CF_DEFAULT_DIGEST, digest));
        NewScalar("sys", "key_digest", PUBKEY_DIGEST, DATA_TYPE_STRING);
        snprintf(workbuf, CF_MAXVARSIZE - 1, "PK_%s", CanonifyName(HashPrint(CF_DEFAULT_DIGEST, digest)));
        HardClass(workbuf);
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

        if (cfstat(name, &sb) != -1)
        {
            snprintf(quoteName, sizeof(quoteName), "\"%s\"", name);
            NewScalar("sys", shortname, quoteName, DATA_TYPE_STRING);
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

        if (cfstat(name, &sb) != -1)
        {
            snprintf(quoteName, sizeof(quoteName), "\"%s\"", name);
            NewScalar("sys", shortname, quoteName, DATA_TYPE_STRING);
        }
    }

/* Windows special directories */

#ifdef __MINGW32__
    if (NovaWin_GetWinDir(workbuf, sizeof(workbuf)))
    {
        NewScalar("sys", "windir", workbuf, DATA_TYPE_STRING);
    }

    if (NovaWin_GetSysDir(workbuf, sizeof(workbuf)))
    {
        NewScalar("sys", "winsysdir", workbuf, DATA_TYPE_STRING);
    }

    if (NovaWin_GetProgDir(workbuf, sizeof(workbuf)))
    {
        NewScalar("sys", "winprogdir", workbuf, DATA_TYPE_STRING);
    }

# ifdef _WIN64
// only available on 64 bit windows systems
    if (NovaWin_GetEnv("PROGRAMFILES(x86)", workbuf, sizeof(workbuf)))
    {
        NewScalar("sys", "winprogdir86", workbuf, DATA_TYPE_STRING);
    }

# else/* NOT _WIN64 */

    NewScalar("sys", "winprogdir86", "", DATA_TYPE_STRING);

# endif

#else /* !__MINGW32__ */

// defs on Unix for manual-building purposes

    NewScalar("sys", "windir", "/dev/null", DATA_TYPE_STRING);
    NewScalar("sys", "winsysdir", "/dev/null", DATA_TYPE_STRING);
    NewScalar("sys", "winprogdir", "/dev/null", DATA_TYPE_STRING);
    NewScalar("sys", "winprogdir86", "/dev/null", DATA_TYPE_STRING);

#endif /* !__MINGW32__ */

    if (THIS_AGENT_TYPE != AGENT_TYPE_EXECUTOR && !LOOKUP)
    {
        LoadSlowlyVaryingObservations();
    }

    EnterpriseContext();

    sprintf(workbuf, "%u_bit", (unsigned) sizeof(void*) * 8);
    HardClass(workbuf);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Additional hard class defined as: %s\n", CanonifyName(workbuf));

    snprintf(workbuf, CF_BUFSIZE, "%s_%s", VSYSNAME.sysname, VSYSNAME.release);
    HardClass(workbuf);

    HardClass(VSYSNAME.machine);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Additional hard class defined as: %s\n", CanonifyName(workbuf));

    snprintf(workbuf, CF_BUFSIZE, "%s_%s", VSYSNAME.sysname, VSYSNAME.machine);
    HardClass(workbuf);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Additional hard class defined as: %s\n", CanonifyName(workbuf));

    snprintf(workbuf, CF_BUFSIZE, "%s_%s_%s", VSYSNAME.sysname, VSYSNAME.machine, VSYSNAME.release);
    HardClass(workbuf);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Additional hard class defined as: %s\n", CanonifyName(workbuf));

#ifdef HAVE_SYSINFO
# ifdef SI_ARCHITECTURE
    sz = sysinfo(SI_ARCHITECTURE, workbuf, CF_BUFSIZE);
    if (sz == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "cfengine internal: sysinfo returned -1\n");
    }
    else
    {
        HardClass(workbuf);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Additional hard class defined as: %s\n", workbuf);
    }
# endif
# ifdef SI_PLATFORM
    sz = sysinfo(SI_PLATFORM, workbuf, CF_BUFSIZE);
    if (sz == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "cfengine internal: sysinfo returned -1\n");
    }
    else
    {
        HardClass(workbuf);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Additional hard class defined as: %s\n", workbuf);
    }
# endif
#endif

    snprintf(workbuf, CF_BUFSIZE, "%s_%s_%s_%s", VSYSNAME.sysname, VSYSNAME.machine, VSYSNAME.release,
             VSYSNAME.version);

    if (strlen(workbuf) > CF_MAXVARSIZE - 2)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "cfengine internal: $(arch) overflows CF_MAXVARSIZE! Truncating\n");
    }

    sp = xstrdup(CanonifyName(workbuf));
    NewScalar("sys", "long_arch", sp, DATA_TYPE_STRING);
    HardClass(sp);
    free(sp);

    snprintf(workbuf, CF_BUFSIZE, "%s_%s", VSYSNAME.sysname, VSYSNAME.machine);
    sp = xstrdup(CanonifyName(workbuf));
    NewScalar("sys", "ostype", sp, DATA_TYPE_STRING);
    HardClass(sp);
    free(sp);

    if (!found)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Cfengine: I don't understand what architecture this is!");
    }

    strcpy(workbuf, "compiled_on_");
    strcat(workbuf, CanonifyName(AUTOCONF_SYSNAME));
    HardClass(workbuf);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "GNU autoconf class from compile time: %s", workbuf);

/* Get IP address from nameserver */

    if ((hp = gethostbyname(VFQNAME)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Hostname lookup failed on node name \"%s\"\n", VSYSNAME.nodename);
        return;
    }
    else
    {
        memset(&cin, 0, sizeof(cin));
        cin.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Address given by nameserver: %s\n", inet_ntoa(cin.sin_addr));
        strcpy(VIPADDRESS, inet_ntoa(cin.sin_addr));

        for (i = 0; hp->h_aliases[i] != NULL; i++)
        {
            CfDebug("Adding alias %s..\n", hp->h_aliases[i]);
            HardClass(hp->h_aliases[i]);
        }
    }

#ifdef HAVE_GETZONEID
    zoneid_t zid;
    char zone[ZONENAME_MAX];
    char vbuff[CF_BUFSIZE];

    zid = getzoneid();
    getzonenamebyid(zid, zone, ZONENAME_MAX);

    NewScalar("sys", "zone", zone, DATA_TYPE_STRING);
    snprintf(vbuff, CF_BUFSIZE - 1, "zone_%s", zone);
    HardClass(vbuff);

    if (strcmp(zone, "global") == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Cfengine seems to be running inside a global solaris zone of name \"%s\"", zone);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Cfengine seems to be running inside a local solaris zone of name \"%s\"", zone);
    }
#endif
}

/*******************************************************************/

void Get3Environment()
{
    char env[CF_BUFSIZE], context[CF_BUFSIZE], name[CF_MAXVARSIZE], value[CF_BUFSIZE];
    FILE *fp;
    struct stat statbuf;
    time_t now = time(NULL);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for environment from cf-monitord...\n");

    snprintf(env, CF_BUFSIZE, "%s/state/%s", CFWORKDIR, CF_ENV_FILE);
    MapName(env);

    if (cfstat(env, &statbuf) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Unable to detect environment from cf-monitord\n\n");
        return;
    }

    if (statbuf.st_mtime < (now - 60 * 60))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Environment data are too old - discarding\n");
        unlink(env);
        return;
    }

    snprintf(value, CF_MAXVARSIZE - 1, "%s", cf_ctime(&statbuf.st_mtime));
    if (Chop(value, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }

    DeleteVariable("mon", "env_time");
    NewScalar("mon", "env_time", value, DATA_TYPE_STRING);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Loading environment...\n");

    if ((fp = fopen(env, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\nUnable to detect environment from cf-monitord\n\n");
        return;
    }

    while (!feof(fp))
    {
        context[0] = '\0';
        name[0] = '\0';
        value[0] = '\0';

        if (fgets(context, CF_BUFSIZE, fp) == NULL)
        {
            if (strlen(context))
            {
                UnexpectedError("Failed to read line from stream");
            }
        }

        if (feof(fp))
        {
            break;
        }


        if (*context == '@')
        {
            Rlist *list = NULL;
            sscanf(context + 1, "%[^=]=%[^\n]", name, value);
           
            CfDebug(" -> Setting new monitoring list %s => %s", name, value);
            list = RlistParseShown(value);
            DeleteVariable("mon", name);
            NewList("mon", name, list, DATA_TYPE_STRING_LIST);

            RlistDestroy(list);
        }
        else if (strstr(context, "="))
        {
            sscanf(context, "%255[^=]=%255[^\n]", name, value);

            if (THIS_AGENT_TYPE != AGENT_TYPE_EXECUTOR)
            {
                DeleteVariable("mon", name);
                NewScalar("mon", name, value, DATA_TYPE_STRING);
                CfDebug(" -> Setting new monitoring scalar %s => %s", name, value);
            }
        }
        else
        {
            HardClass(context);
        }
    }

    fclose(fp);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Environment data loaded\n\n");
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
            CfDebug("Identifying (%s) as one of my interfaces\n", adr);
            return true;
        }
    }

    CfDebug("(%s) is not one of my interfaces\n", adr);
    return false;
}

/*******************************************************************/

void BuiltinClasses(void)
{
    char vbuff[CF_BUFSIZE];

    HardClass("any");            /* This is a reserved word / wildcard */

    snprintf(vbuff, CF_BUFSIZE, "cfengine_%s", CanonifyName(Version()));
    CreateHardClassesFromCanonification(vbuff);

}

/*******************************************************************/

void CreateHardClassesFromCanonification(const char *canonified)
{
    char buf[CF_MAXVARSIZE];

    strlcpy(buf, canonified, sizeof(buf));

    HardClass(buf);

    char *sp;

    while ((sp = strrchr(buf, '_')))
    {
        *sp = 0;
        HardClass(buf);
    }
}

static void SetFlavour(const char *flavour)
{
    HardClass(flavour);
    NewScalar("sys", "flavour", flavour, DATA_TYPE_STRING);
    NewScalar("sys", "flavor", flavour, DATA_TYPE_STRING);
}

void OSClasses(void)
{
#ifdef __linux__
    struct stat statbuf;

/* Mandrake/Mandriva, Fedora and Oracle VM Server supply /etc/redhat-release, so
   we test for those distributions first */

    if (cfstat("/etc/mandriva-release", &statbuf) != -1)
    {
        Linux_Mandriva_Version();
    }
    else if (cfstat("/etc/mandrake-release", &statbuf) != -1)
    {
        Linux_Mandrake_Version();
    }
    else if (cfstat("/etc/fedora-release", &statbuf) != -1)
    {
        Linux_Fedora_Version();
    }
    else if (cfstat("/etc/ovs-release", &statbuf) != -1)
    {
        Linux_Oracle_VM_Server_Version();
    }
    else if (cfstat("/etc/redhat-release", &statbuf) != -1)
    {
        Linux_Redhat_Version();
    }

/* Oracle Linux >= 6 supplies separate /etc/oracle-release alongside
   /etc/redhat-release, use it to precisely identify version */

    if (cfstat("/etc/oracle-release", &statbuf) != -1)
    {
        Linux_Oracle_Version();
    }

    if (cfstat("/etc/generic-release", &statbuf) != -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a sun cobalt system.\n");
        SetFlavour("SunCobalt");
    }

    if (cfstat("/etc/SuSE-release", &statbuf) != -1)
    {
        Linux_Suse_Version();
    }

# define SLACKWARE_ANCIENT_VERSION_FILENAME "/etc/slackware-release"
# define SLACKWARE_VERSION_FILENAME "/etc/slackware-version"
    if (cfstat(SLACKWARE_VERSION_FILENAME, &statbuf) != -1)
    {
        Linux_Slackware_Version(SLACKWARE_VERSION_FILENAME);
    }
    else if (cfstat(SLACKWARE_ANCIENT_VERSION_FILENAME, &statbuf) != -1)
    {
        Linux_Slackware_Version(SLACKWARE_ANCIENT_VERSION_FILENAME);
    }

    if (cfstat("/etc/debian_version", &statbuf) != -1)
    {
        Linux_Debian_Version();
    }

    if (cfstat("/usr/bin/aptitude", &statbuf) != -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This system seems to have the aptitude package system\n");
        HardClass("have_aptitude");
    }

    if (cfstat("/etc/UnitedLinux-release", &statbuf) != -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a UnitedLinux system.\n");
        SetFlavour("UnitedLinux");
    }

    if (cfstat("/etc/alpine-release", &statbuf) != -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be an AlpineLinux system.\n");
        SetFlavour("alpinelinux");
    }

    if (cfstat("/etc/gentoo-release", &statbuf) != -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a gentoo system.\n");
        SetFlavour("gentoo");
    }

    if (cfstat("/etc/arch-release", &statbuf) != -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be an Arch Linux system.\n");
        SetFlavour("archlinux");
    }

    if (cfstat("/proc/vmware/version", &statbuf) != -1 || cfstat("/etc/vmware-release", &statbuf) != -1)
    {
        VM_Version();
    }
    else if (cfstat("/etc/vmware", &statbuf) != -1 && S_ISDIR(statbuf.st_mode))
    {
        VM_Version();
    }

    if (cfstat("/proc/xen/capabilities", &statbuf) != -1)
    {
        Xen_Domain();
    }

    if (cfstat("/etc/Eos-release", &statbuf) != -1)
    {
        EOS_Version();
        SetFlavour("Eos");
    }

    if (cfstat("/etc/issue", &statbuf) != -1)
    {
        MiscOS();
    }
    
#ifdef XEN_CPUID_SUPPORT
    else if (Xen_Hv_Check())
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a xen hv system.\n");
        HardClass("xen");
        HardClass("xen_domu_hv");
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
    SetFlavour(context);

#endif

    GetCPUInfo();

#ifdef __CYGWIN__

    for (char *sp = VSYSNAME.sysname; *sp != '\0'; sp++)
    {
        if (*sp == '-')
        {
            sp++;
            if (strncmp(sp, "5.0", 3) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be Windows 2000\n");
                HardClass("Win2000");
            }

            if (strncmp(sp, "5.1", 3) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be Windows XP\n");
                HardClass("WinXP");
            }

            if (strncmp(sp, "5.2", 3) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be Windows Server 2003\n");
                HardClass("WinServer2003");
            }

            if (strncmp(sp, "6.1", 3) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be Windows Vista\n");
                HardClass("WinVista");
            }

            if (strncmp(sp, "6.3", 3) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be Windows Server 2008\n");
                HardClass("WinServer2008");
            }
        }
    }

    NewScalar("sys", "crontab", "", DATA_TYPE_STRING);

#endif /* __CYGWIN__ */

#ifdef __MINGW32__
    HardClass(VSYSNAME.release); // code name - e.g. Windows Vista
    HardClass(VSYSNAME.version); // service pack number - e.g. Service Pack 3

    if (strstr(VSYSNAME.sysname, "workstation"))
    {
        HardClass("WinWorkstation");
    }
    else if (strstr(VSYSNAME.sysname, "server"))
    {
        HardClass("WinServer");
    }
    else if (strstr(VSYSNAME.sysname, "domain controller"))
    {
        HardClass("DomainController");
        HardClass("WinServer");
    }
    else
    {
        HardClass("unknown_ostype");
    }

    SetFlavour("windows");

#endif /* __MINGW32__ */

#ifndef _WIN32
    struct passwd *pw;
    if ((pw = getpwuid(getuid())) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "getpwuid", " !! Unable to get username for uid %ju", (uintmax_t)getuid());
    }
    else
    {
        char vbuff[CF_BUFSIZE];

        if (IsDefinedClass("SuSE", NULL))
        {
            snprintf(vbuff, CF_BUFSIZE, "/var/spool/cron/tabs/%s", pw->pw_name);
        }
        else
        {
            snprintf(vbuff, CF_BUFSIZE, "/var/spool/cron/crontabs/%s", pw->pw_name);
        }

        NewScalar("sys", "crontab", vbuff, DATA_TYPE_STRING);
    }

#endif

#if defined(__ANDROID__)
    SetFlavour("android");
#endif

#ifdef __sun
    if (FullTextMatch("joyent.*", VSYSNAME.version))
    {
        HardClass("smartos");
        HardClass("smartmachine");
    }
#endif
    
    /* FIXME: this variable needs redhat/SuSE/debian classes to be defined and
     * hence can't be initialized earlier */

    if (IsDefinedClass("redhat", NULL))
    {
        NewScalar("sys", "doc_root", "/var/www/html", DATA_TYPE_STRING);
    }

    if (IsDefinedClass("SuSE", NULL))
    {
        NewScalar("sys", "doc_root", "/srv/www/htdocs", DATA_TYPE_STRING);
    }

    if (IsDefinedClass("debian", NULL))
    {
        NewScalar("sys", "doc_root", "/var/www", DATA_TYPE_STRING);
    }
}

/*********************************************************************************/

#ifdef __linux__
static void Linux_Oracle_VM_Server_Version(void)
{
    char relstring[CF_MAXVARSIZE];
    char *r;
    int major, minor, patch;
    int revcomps;

#define ORACLE_VM_SERVER_REL_FILENAME "/etc/ovs-release"
#define ORACLE_VM_SERVER_ID "Oracle VM server"

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be Oracle VM Server");
    HardClass("redhat");
    HardClass("oraclevmserver");

    if (!ReadLine(ORACLE_VM_SERVER_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return;
    }

    if (strncmp(relstring, ORACLE_VM_SERVER_ID, strlen(ORACLE_VM_SERVER_ID)))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not identify distribution from %s\n", ORACLE_VM_SERVER_REL_FILENAME);
        return;
    }

    if ((r = strstr(relstring, "release ")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find distribution version in %s\n", ORACLE_VM_SERVER_REL_FILENAME);
        return;
    }

    revcomps = sscanf(r + strlen("release "), "%d.%d.%d", &major, &minor, &patch);

    if (revcomps > 0)
    {
        char buf[CF_BUFSIZE];

        snprintf(buf, CF_BUFSIZE, "oraclevmserver_%d", major);
        SetFlavour(buf);
    }

    if (revcomps > 1)
    {
        char buf[CF_BUFSIZE];

        snprintf(buf, CF_BUFSIZE, "oraclevmserver_%d_%d", major, minor);
        HardClass(buf);
    }

    if (revcomps > 2)
    {
        char buf[CF_BUFSIZE];

        snprintf(buf, CF_BUFSIZE, "oraclevmserver_%d_%d_%d", major, minor, patch);
        HardClass(buf);
    }
}

/*********************************************************************************/

static void Linux_Oracle_Version(void)
{
    char relstring[CF_MAXVARSIZE];
    char *r;
    int major, minor;

#define ORACLE_REL_FILENAME "/etc/oracle-release"
#define ORACLE_ID "Oracle Linux Server"

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be Oracle Linux");
    HardClass("oracle");

    if (!ReadLine(ORACLE_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return;
    }

    if (strncmp(relstring, ORACLE_ID, strlen(ORACLE_ID)))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not identify distribution from %s\n", ORACLE_REL_FILENAME);
        return;
    }

    if ((r = strstr(relstring, "release ")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find distribution version in %s\n", ORACLE_REL_FILENAME);
        return;
    }

    if (sscanf(r + strlen("release "), "%d.%d", &major, &minor) == 2)
    {
        char buf[CF_BUFSIZE];

        snprintf(buf, CF_BUFSIZE, "oracle_%d", major);
        SetFlavour(buf);

        snprintf(buf, CF_BUFSIZE, "oracle_%d_%d", major, minor);
        HardClass(buf);
    }
}

/*********************************************************************************/

static int Linux_Fedora_Version(void)
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

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a fedora system.\n");
    HardClass("redhat");
    HardClass("fedora");

/* Grab the first line from the file and then close it. */

    if (!ReadLine(FEDORA_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return 1;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for fedora core linux info...\n");

    if (!strncmp(relstring, FEDORA_ID, strlen(FEDORA_ID)))
    {
        vendor = "fedora";
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not identify OS distro from %s\n", FEDORA_REL_FILENAME);
        return 2;
    }

/* Now, grok the release.  We assume that all the strings will
 * have the word 'release' before the numerical release.
 */

    release = strstr(relstring, RELEASE_FLAG);

    if (release == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find a numeric OS release in %s\n", FEDORA_REL_FILENAME);
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
        HardClass(classbuf);
        strcat(classbuf, "_");
        strcat(classbuf, strmajor);
        SetFlavour(classbuf);
    }

    return 0;
}

/*********************************************************************************/

static int Linux_Redhat_Version(void)
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

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a redhat (or redhat-based) system.\n");
    HardClass("redhat");

/* Grab the first line from the file and then close it. */

    if (!ReadLine(RH_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return 1;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for redhat linux info in \"%s\"\n", relstring);

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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not identify OS distro from %s\n", RH_REL_FILENAME);
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find a numeric OS release in %s\n", RH_REL_FILENAME);
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
        HardClass(classbuf);
        strcat(classbuf, "_");

        if (strcmp(edition, "") != 0)
        {
            strcat(classbuf, edition);
            HardClass(classbuf);
            strcat(classbuf, "_");
        }

        strcat(classbuf, strmajor);
        HardClass(classbuf);

        if (minor != -2)
        {
            strcat(classbuf, "_");
            strcat(classbuf, strminor);
            HardClass(classbuf);
        }
    }

// Now a version without the edition

    if (major != -1 && minor != -1 && (strcmp(vendor, "") != 0))
    {
        classbuf[0] = '\0';
        strcat(classbuf, vendor);
        HardClass(classbuf);
        strcat(classbuf, "_");

        strcat(classbuf, strmajor);

        SetFlavour(classbuf);

        if (minor != -2)
        {
            strcat(classbuf, "_");
            strcat(classbuf, strminor);
            HardClass(classbuf);
        }
    }

    return 0;
}

/******************************************************************/

static int Linux_Suse_Version(void)
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

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a SuSE system.\n");
    HardClass("SuSE");

/* Grab the first line from the file and then close it. */

    fp = ReadFirstLine(SUSE_REL_FILENAME, relstring, sizeof(relstring));
    if (fp == NULL)
    {
        return 1;
    }

    strversion[0] = '\0';
    strpatch[0] = '\0';

    while (!feof(fp))
    {
        vbuf[0] = '\0';
        if (fgets(vbuf, sizeof(vbuf), fp) == NULL)
        {
            if (strlen(vbuf))
            {
                UnexpectedError("Failed to read line from stream");
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

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for SuSE enterprise info in \"%s\"\n", relstring);

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
        HardClass(classbuf);
    }
    else if (strncmp(relstring, "sles", 4) == 0)
    {
        Item *list, *ip;

        sscanf(relstring, "%[-_a-zA-Z0-9]", vbuf);
        HardClass(vbuf);

        list = SplitString(vbuf, '-');

        for (ip = list; ip != NULL; ip = ip->next)
        {
            HardClass(ip->name);
        }

        DeleteItemList(list);
    }
    else
    {
        for (version = 9; version < 13; version++)
        {
            snprintf(vbuf, CF_BUFSIZE, "%s %d ", SUSE_SLES_ID, version);
            CfDebug("Checking for suse [%s]\n", vbuf);

            if (!strncmp(relstring, vbuf, strlen(vbuf)))
            {
                snprintf(classbuf, CF_MAXVARSIZE, "SLES%d", version);
                HardClass(classbuf);
            }
            else
            {
                snprintf(vbuf, CF_BUFSIZE, "%s %d ", SUSE_SLED_ID, version);
                CfDebug("Checking for suse [%s]\n", vbuf);

                if (!strncmp(relstring, vbuf, strlen(vbuf)))
                {
                    snprintf(classbuf, CF_MAXVARSIZE, "SLED%d", version);
                    HardClass(classbuf);
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find a numeric OS release in %s\n", SUSE_REL_FILENAME);
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
                HardClass(classbuf);
                strcat(classbuf, "_");
                strcat(classbuf, strmajor);
                SetFlavour(classbuf);
                strcat(classbuf, "_");
                strcat(classbuf, strminor);
                HardClass(classbuf);

                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Discovered SuSE version %s", classbuf);
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
                HardClass(classbuf);
                strcat(classbuf, "_");
                strcat(classbuf, strmajor);
                HardClass(classbuf);
                strcat(classbuf, "_");
                strcat(classbuf, strminor);
                HardClass(classbuf);
                snprintf(classbuf, CF_MAXVARSIZE, "SuSE_%d", major);
                SetFlavour(classbuf);

                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Discovered SuSE version %s", classbuf);
                return 0;
            }
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find a numeric OS release in %s\n", SUSE_REL_FILENAME);

    return 0;
}

/******************************************************************/

static int Linux_Slackware_Version(char *filename)
{
    int major = -1;
    int minor = -1;
    int release = -1;
    char classname[CF_MAXVARSIZE] = "";
    char buffer[CF_MAXVARSIZE];

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a slackware system.\n");
    HardClass("slackware");

    if (!ReadLine(filename, buffer, sizeof(buffer)))
    {
        return 1;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for Slackware version...\n");
    switch (sscanf(buffer, "Slackware %d.%d.%d", &major, &minor, &release))
    {
    case 3:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a Slackware %u.%u.%u system.", major, minor, release);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u_%u_%u", major, minor, release);
        HardClass(classname);
        /* Fall-through */
    case 2:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a Slackware %u.%u system.", major, minor);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u_%u", major, minor);
        HardClass(classname);
        /* Fall-through */
    case 1:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a Slackware %u system.", major);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u", major);
        HardClass(classname);
        break;
    case 0:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "No Slackware version number found.\n");
        return 2;
    }
    return 0;
}

/******************************************************************/

static int Linux_Debian_Version(void)
{
#define DEBIAN_VERSION_FILENAME "/etc/debian_version"
#define DEBIAN_ISSUE_FILENAME "/etc/issue"
    int major = -1;
    int release = -1;
    int result;
    char classname[CF_MAXVARSIZE], buffer[CF_MAXVARSIZE], os[CF_MAXVARSIZE], version[CF_MAXVARSIZE];

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a debian system.\n");
    HardClass("debian");

    buffer[0] = classname[0] = '\0';

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for Debian version...\n");

    if (!ReadLine(DEBIAN_VERSION_FILENAME, buffer, sizeof(buffer)))
    {
        return 1;
    }

    result = sscanf(buffer, "%d.%d", &major, &release);

    switch (result)
    {
    case 2:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a Debian %u.%u system.", major, release);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u_%u", major, release);
        HardClass(classname);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u", major);
        SetFlavour(classname);
        break;
        /* Fall-through */
    case 1:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a Debian %u system.", major);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u", major);
        SetFlavour(classname);
        break;

    default:
        version[0] = '\0';
        sscanf(buffer, "%25[^/]", version);
        if (strlen(version) > 0)
        {
            snprintf(classname, CF_MAXVARSIZE, "debian_%s", version);
            HardClass(classname);
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
        sscanf(buffer, "%*s %*s %[^./]", version);
        snprintf(buffer, CF_MAXVARSIZE, "debian_%s", version);
        HardClass("debian");
        SetFlavour(buffer);
    }
    else if (strcmp(os, "Ubuntu") == 0)
    {
        sscanf(buffer, "%*s %[^.].%d", version, &release);
        snprintf(buffer, CF_MAXVARSIZE, "ubuntu_%s", version);
        SetFlavour(buffer);
        HardClass("ubuntu");
        if (release >= 0)
        {
            snprintf(buffer, CF_MAXVARSIZE, "ubuntu_%s_%d", version, release);
            HardClass(buffer);
        }
    }

    return 0;
}

/******************************************************************/

static int Linux_Mandrake_Version(void)
{
/* We are looking for one of the following strings... */
#define MANDRAKE_ID "Linux Mandrake"
#define MANDRAKE_REV_ID "Mandrake Linux"
#define MANDRAKE_10_1_ID "Mandrakelinux"

#define MANDRAKE_REL_FILENAME "/etc/mandrake-release"

    char relstring[CF_MAXVARSIZE];
    char *vendor = NULL;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a mandrake system.\n");
    HardClass("Mandrake");

    if (!ReadLine(MANDRAKE_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return 1;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for Mandrake linux info in \"%s\"\n", relstring);

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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not identify OS distro from %s\n", MANDRAKE_REL_FILENAME);
        return 2;
    }

    return Linux_Mandriva_Version_Real(MANDRAKE_REL_FILENAME, relstring, vendor);
}

/******************************************************************/

static int Linux_Mandriva_Version(void)
{
/* We are looking for the following strings... */
#define MANDRIVA_ID "Mandriva Linux"

#define MANDRIVA_REL_FILENAME "/etc/mandriva-release"

    char relstring[CF_MAXVARSIZE];
    char *vendor = NULL;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a mandriva system.\n");
    HardClass("Mandrake");
    HardClass("Mandriva");

    if (!ReadLine(MANDRIVA_REL_FILENAME, relstring, sizeof(relstring)))
    {
        return 1;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Looking for Mandriva linux info in \"%s\"\n", relstring);

    if (!strncmp(relstring, MANDRIVA_ID, strlen(MANDRIVA_ID)))
    {
        vendor = "mandriva";
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not identify OS distro from %s\n", MANDRIVA_REL_FILENAME);
        return 2;
    }

    return Linux_Mandriva_Version_Real(MANDRIVA_REL_FILENAME, relstring, vendor);

}

/******************************************************************/

static int Linux_Mandriva_Version_Real(char *filename, char *relstring, char *vendor)
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find a numeric OS release in %s\n", filename);
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
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not break down release version numbers in %s\n", filename);
        }
    }

    if (major != -1 && minor != -1 && strcmp(vendor, ""))
    {
        classbuf[0] = '\0';
        strcat(classbuf, vendor);
        HardClass(classbuf);
        strcat(classbuf, "_");
        strcat(classbuf, strmajor);
        HardClass(classbuf);
        if (minor != -2)
        {
            strcat(classbuf, "_");
            strcat(classbuf, strminor);
            HardClass(classbuf);
        }
    }

    return 0;
}

/******************************************************************/

static int EOS_Version()

{ char buffer[CF_BUFSIZE];

 // e.g. Arista Networks EOS 4.10.2
 
    if (ReadLine("/etc/Eos-release", buffer, sizeof(buffer)))
    {
        if (strstr(buffer, "EOS"))
        {
            char version[CF_MAXVARSIZE], class[CF_MAXVARSIZE];
            HardClass("eos");
            HardClass("arista");
            version[0] = '\0';
            sscanf(buffer, "%*s %*s %*s %s", version);
            CanonifyNameInPlace(version);
            snprintf(class, CF_MAXVARSIZE, "eos_%s", version);
            HardClass(class);
        }
    }
    
    return 0;
}

/******************************************************************/

static int MiscOS()

{ char buffer[CF_BUFSIZE];

 // e.g. BIG-IP 10.1.0 Build 3341.1084
 
    if (ReadLine("/etc/issue", buffer, sizeof(buffer)))
    {
       if (strstr(buffer, "BIG-IP"))
       {
           char version[CF_MAXVARSIZE], build[CF_MAXVARSIZE], class[CF_MAXVARSIZE];
           HardClass("big_ip");
           sscanf(buffer, "%*s %s %*s %s", version, build);
           CanonifyNameInPlace(version);
           CanonifyNameInPlace(build);
           snprintf(class, CF_MAXVARSIZE, "big_ip_%s", version);
           HardClass(class);
           snprintf(class, CF_MAXVARSIZE, "big_ip_%s_%s", version, build);
           HardClass(class);
           SetFlavour("BIG-IP");
       }
    }
    
    return 0;
}

/******************************************************************/

static int VM_Version(void)
{
    char *sp, buffer[CF_BUFSIZE], classbuf[CF_BUFSIZE], version[CF_BUFSIZE];
    int major, minor, bug;
    int sufficient = 0;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a VMware Server ESX/xSX system.\n");
    HardClass("VMware");

/* VMware Server ESX >= 3 has version info in /proc */
    if (ReadLine("/proc/vmware/version", buffer, sizeof(buffer)))
    {
        if (sscanf(buffer, "VMware ESX Server %d.%d.%d", &major, &minor, &bug) > 0)
        {
            snprintf(classbuf, CF_BUFSIZE, "VMware ESX Server %d", major);
            HardClass(classbuf);
            snprintf(classbuf, CF_BUFSIZE, "VMware ESX Server %d.%d", major, minor);
            HardClass(classbuf);
            snprintf(classbuf, CF_BUFSIZE, "VMware ESX Server %d.%d.%d", major, minor, bug);
            HardClass(classbuf);
            sufficient = 1;
        }
        else if (sscanf(buffer, "VMware ESX Server %s", version) > 0)
        {
            snprintf(classbuf, CF_BUFSIZE, "VMware ESX Server %s", version);
            HardClass(classbuf);
            sufficient = 1;
        }
    }

/* Fall back to checking for other files */

    if (sufficient < 1 && (ReadLine("/etc/vmware-release", buffer, sizeof(buffer))
                           || ReadLine("/etc/issue", buffer, sizeof(buffer))))
    {
        HardClass(buffer);

        /* Strip off the release code name e.g. "(Dali)" */
        if ((sp = strchr(buffer, '(')) != NULL)
        {
            *sp = 0;
            Chop(buffer, CF_EXPANDSIZE);
            HardClass(buffer);
        }
        sufficient = 1;
    }

    return sufficient < 1 ? 1 : 0;
}

/******************************************************************/

static int Xen_Domain(void)
{
    FILE *fp;
    char buffer[CF_BUFSIZE];
    int sufficient = 0;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "This appears to be a xen pv system.\n");
    HardClass("xen");

/* xen host will have "control_d" in /proc/xen/capabilities, xen guest will not */

    if ((fp = fopen("/proc/xen/capabilities", "r")) != NULL)
    {
        while (!feof(fp))
        {
            CfReadLine(buffer, CF_BUFSIZE, fp);
            if (strstr(buffer, "control_d"))
            {
                HardClass("xen_dom0");
                sufficient = 1;
            }
        }

        if (!sufficient)
        {
            HardClass("xen_domu_pv");
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

static const char *GetDefaultWorkDir(void)
{
    if (getuid() > 0)
    {
        static char workdir[CF_BUFSIZE];

        if (!*workdir)
        {
            struct passwd *mpw = getpwuid(getuid());

            strncpy(workdir, mpw->pw_dir, CF_BUFSIZE - 10);
            strcat(workdir, "/.cfagent");

            if (strlen(workdir) > CF_BUFSIZE / 2)
            {
                FatalError("Suspicious looking home directory. The path is too long and will lead to problems.");
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

static void GetCPUInfo()
{
#if defined(MINGW) || defined(NT)
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "!! cpu count not implemented on Windows platform\n");
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
        CfOut(OUTPUT_LEVEL_ERROR, "sysctl", "!! failed to get cpu count: %s\n", strerror(errno));
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! invalid processor count: %d\n", count);
        return;
    }
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "-> Found %d processor%s\n", count, count > 1 ? "s" : "");

    if (count == 1) {
        HardClass(buf);  // "1_cpu" from init - change if buf is ever used above
        NewScalar("sys", "cpus", "1", DATA_TYPE_STRING);
    } else {
        snprintf(buf, CF_SMALLBUF, "%d_cpus", count);
        HardClass(buf);
        snprintf(buf, CF_SMALLBUF, "%d", count);
        NewScalar("sys", "cpus", buf, DATA_TYPE_STRING);
    }
}
