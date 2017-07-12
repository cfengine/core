/*
   Copyright 2017 Northern.tech AS

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <cf3.defs.h>

#include <actuator.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_operators.h>
#include <files_lib.h>
#include <item_lib.h>
#include <conversion.h>
#include <match_scope.h>
#include <string_lib.h>
#include <systype.h>
#include <pipes.h>
#include <nfs.h>
#include <misc_lib.h>
#include <rlist.h>
#include <eval_context.h>
#include <timeout.h>

/* seconds */
#define RPCTIMEOUT 60

static int FSTAB_EDITS = 0; /* GLOBAL_X */
static Item *FSTABLIST = NULL; /* GLOBAL_X */

static void AugmentMountInfo(Seq *list, char *host, char *source, char *mounton, char *options);
static int MatchFSInFstab(char *match);
static void DeleteThisItem(Item **liststart, Item *entry);

static const char *const VMOUNTCOMM[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = "",
    [PLATFORM_CONTEXT_OPENVZ] = "/bin/mount -va",          /* virt_host_vz_vzps */
    [PLATFORM_CONTEXT_HP] = "/sbin/mount -ea",             /* hpux */
    [PLATFORM_CONTEXT_AIX] = "/usr/sbin/mount -t nfs",     /* aix */
    [PLATFORM_CONTEXT_LINUX] = "/bin/mount -va",           /* linux */
    [PLATFORM_CONTEXT_BUSYBOX] = "/bin/mount -va",         /* linux */
    [PLATFORM_CONTEXT_SOLARIS] = "/usr/sbin/mount -a",     /* solaris */
    [PLATFORM_CONTEXT_SUN_SOLARIS] = "/usr/sbin/mount -a", /* solaris */
    [PLATFORM_CONTEXT_FREEBSD] = "/sbin/mount -va",        /* freebsd */
    [PLATFORM_CONTEXT_NETBSD] = "/sbin/mount -a",          /* netbsd */
    [PLATFORM_CONTEXT_CRAYOS] = "/etc/mount -va",          /* cray */
    [PLATFORM_CONTEXT_WINDOWS_NT] = "/bin/sh /etc/fstab",  /* NT - possible security issue */
    [PLATFORM_CONTEXT_SYSTEMV] = "/sbin/mountall",         /* Unixware */
    [PLATFORM_CONTEXT_OPENBSD] = "/sbin/mount",            /* openbsd */
    [PLATFORM_CONTEXT_CFSCO] = "/etc/mountall",            /* sco */
    [PLATFORM_CONTEXT_DARWIN] = "/sbin/mount -va",         /* darwin */
    [PLATFORM_CONTEXT_QNX] = "/bin/mount -v",              /* qnx */
    [PLATFORM_CONTEXT_DRAGONFLY] = "/sbin/mount -va",      /* dragonfly */
    [PLATFORM_CONTEXT_MINGW] = "mingw-invalid",            /* mingw */
    [PLATFORM_CONTEXT_VMWARE] = "/bin/mount -a",           /* vmware */
    [PLATFORM_CONTEXT_ANDROID] = "",                       /* android */
};

static const char *const VUNMOUNTCOMM[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = "",
    [PLATFORM_CONTEXT_OPENVZ] = "/bin/umount",          /* virt_host_vz_vzps */
    [PLATFORM_CONTEXT_HP] = "/sbin/umount",             /* hpux */
    [PLATFORM_CONTEXT_AIX] = "/usr/sbin/umount",        /* aix */
    [PLATFORM_CONTEXT_LINUX] = "/bin/umount",           /* linux */
    [PLATFORM_CONTEXT_BUSYBOX] = "/bin/umount",         /* linux */
    [PLATFORM_CONTEXT_SOLARIS] = "/etc/umount",         /* solaris */
    [PLATFORM_CONTEXT_SUN_SOLARIS] = "/etc/umount",     /* solaris */
    [PLATFORM_CONTEXT_FREEBSD] = "/sbin/umount",        /* freebsd */
    [PLATFORM_CONTEXT_NETBSD] = "/sbin/umount",         /* netbsd */
    [PLATFORM_CONTEXT_CRAYOS] = "/etc/umount",          /* cray */
    [PLATFORM_CONTEXT_WINDOWS_NT] = "/bin/umount",      /* NT */
    [PLATFORM_CONTEXT_SYSTEMV] = "/sbin/umount",        /* Unixware */
    [PLATFORM_CONTEXT_OPENBSD] = "/sbin/umount",        /* openbsd */
    [PLATFORM_CONTEXT_CFSCO] = "/etc/umount",           /* sco */
    [PLATFORM_CONTEXT_DARWIN] = "/sbin/umount",         /* darwin */
    [PLATFORM_CONTEXT_QNX] = "/bin/umount",             /* qnx */
    [PLATFORM_CONTEXT_DRAGONFLY] = "/sbin/umount",      /* dragonfly */
    [PLATFORM_CONTEXT_MINGW] = "mingw-invalid",         /* mingw */
    [PLATFORM_CONTEXT_VMWARE] = "/bin/umount",          /* vmware */
    [PLATFORM_CONTEXT_ANDROID] = "/system/xbin/umount", /* android */
};

static const char *const VMOUNTOPTS[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = "",
    [PLATFORM_CONTEXT_OPENVZ] = "defaults",          /* virt_host_vz_vzps */
    [PLATFORM_CONTEXT_HP] = "bg,hard,intr",          /* hpux */
    [PLATFORM_CONTEXT_AIX] = "bg,hard,intr",         /* aix */
    [PLATFORM_CONTEXT_LINUX] = "defaults",           /* linux */
    [PLATFORM_CONTEXT_BUSYBOX] = "defaults",         /* linux */
    [PLATFORM_CONTEXT_SOLARIS] = "bg,hard,intr",     /* solaris */
    [PLATFORM_CONTEXT_SUN_SOLARIS] = "bg,hard,intr", /* solaris */
    [PLATFORM_CONTEXT_FREEBSD] = "bg,intr",          /* freebsd */
    [PLATFORM_CONTEXT_NETBSD] = "-i,-b",             /* netbsd */
    [PLATFORM_CONTEXT_CRAYOS] = "bg,hard,intr",      /* cray */
    [PLATFORM_CONTEXT_WINDOWS_NT] = "",              /* NT */
    [PLATFORM_CONTEXT_SYSTEMV] = "bg,hard,intr",     /* Unixware */
    [PLATFORM_CONTEXT_OPENBSD] = "-i,-b",            /* openbsd */
    [PLATFORM_CONTEXT_CFSCO] = "bg,hard,intr",       /* sco */
    [PLATFORM_CONTEXT_DARWIN] = "-i,-b",             /* darwin */
    [PLATFORM_CONTEXT_QNX] = "bg,hard,intr",         /* qnx */
    [PLATFORM_CONTEXT_DRAGONFLY] = "bg,intr",        /* dragonfly */
    [PLATFORM_CONTEXT_MINGW] = "mingw-invalid",      /* mingw */
    [PLATFORM_CONTEXT_VMWARE] = "defaults",          /* vmstate */
    [PLATFORM_CONTEXT_ANDROID] = "defaults",         /* android */
};

bool LoadMountInfo(Seq *list)
/* This is, in fact, the most portable way to read the mount info! */
/* Depressing, isn't it? */
{
    FILE *pp;
    char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE], buf3[CF_BUFSIZE];
    int i, nfs = false;

    for (i = 0; VMOUNTCOMM[VSYSTEMHARDCLASS][i] != ' '; i++)
    {
        buf1[i] = VMOUNTCOMM[VSYSTEMHARDCLASS][i];
    }

    buf1[i] = '\0';

    SetTimeOut(RPCTIMEOUT);

    if ((pp = cf_popen(buf1, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Can't open '%s'. (cf_popen: %s)", buf1, GetErrorStr());
        return false;
    }

    size_t vbuff_size = CF_BUFSIZE;
    char *vbuff = xmalloc(vbuff_size);

    for (;;)
    {
        buf1[0] = buf2[0] = buf3[0] = '\0';
        nfs = false;

        ssize_t res = CfReadLine(&vbuff, &vbuff_size, pp);
        if (res == -1)
        {
            if (!feof(pp))
            {
                Log(LOG_LEVEL_ERR, "Unable to read list of mounted filesystems. (fread: %s)", GetErrorStr());
                cf_pclose(pp);
                free(vbuff);
                return false;
            }
            else
            {
                break;
            }
        }

        if (strstr(vbuff, "nfs"))
        {
            nfs = true;
        }

        // security note: buff is CF_BUFSIZE, so that is the max that can be written to buf1, buf2 or buf3
        sscanf(vbuff, "%s%s%s", buf1, buf2, buf3);

        if ((vbuff[0] == '\0') || (vbuff[0] == '\n'))
        {
            break;
        }

        if (strstr(vbuff, "not responding"))
        {
            Log(LOG_LEVEL_ERR, "%s", vbuff);
        }

        if (strstr(vbuff, "be root"))
        {
            Log(LOG_LEVEL_ERR, "Mount access is denied. You must be root. Use the -n option to run safely");
        }

        if ((strstr(vbuff, "retrying")) || (strstr(vbuff, "denied")) || (strstr(vbuff, "backgrounding")))
        {
            continue;
        }

        if ((strstr(vbuff, "exceeded")) || (strstr(vbuff, "busy")))
        {
            continue;
        }

        if (strstr(vbuff, "RPC"))
        {
            Log(LOG_LEVEL_INFO, "There was an RPC timeout. Aborting mount operations.");
            Log(LOG_LEVEL_INFO, "Session failed while trying to talk to remote host");
            Log(LOG_LEVEL_INFO, "%s", vbuff);
            cf_pclose(pp);
            free(vbuff);
            return false;
        }

        // host: max FQDN is 255 chars (max IPv6 with IPv4 tunneling is 45 chars)
        // source, mounton: hardcoding max path length to 1023; longer is very unlikely
        char host[256], source[1024], mounton[1024];
        host[0] = source[0] = mounton[0] = '\0';


#if defined(__sun) || defined(__hpux)
        if (IsAbsoluteFileName(buf3))
        {
            strlcpy(host, "localhost", sizeof(host));
            strlcpy(mounton, buf1, sizeof(mounton));
        }
        else
        {
            sscanf(buf1, "%255[^:]:%1023s", host, source);
            strlcpy(mounton, buf1, sizeof(mounton));
        }
#elif defined(_AIX)
        /* skip header */

        if (IsAbsoluteFileName(buf1))
        {
            strlcpy(host, "localhost", sizeof(host));
            strlcpy(mounton, buf2, sizeof(mounton));
        }
        else
        {
            strlcpy(host, buf1, sizeof(host));
            strlcpy(source, buf1, sizeof(source));
            strlcpy(mounton, buf3, sizeof(mounton));
        }
#elif defined(__CYGWIN__)
        strlcpy(mounton, buf2, sizeof(mounton));
        strlcpy(host, buf1, sizeof(host));
#elif defined(sco) || defined(__SCO_DS)
        Log(LOG_LEVEL_ERR, "Don't understand SCO mount format, no data");
#else
        if (IsAbsoluteFileName(buf1))
        {
            strlcpy(host, "localhost", sizeof(host));
            strlcpy(mounton, buf3, sizeof(mounton));
        }
        else
        {
            sscanf(buf1, "%255[^:]:%1023s", host, source);
            strlcpy(mounton, buf3, sizeof(mounton));
        }
#endif

        Log(LOG_LEVEL_DEBUG, "LoadMountInfo: host '%s', source '%s', mounton '%s'", host, source, mounton);

        if (nfs)
        {
            AugmentMountInfo(list, host, source, mounton, "nfs");
        }
        else
        {
            AugmentMountInfo(list, host, source, mounton, NULL);
        }
    }

    free(vbuff);
    alarm(0);
    signal(SIGALRM, SIG_DFL);
    cf_pclose(pp);
    return true;
}

/*******************************************************************/

static void AugmentMountInfo(Seq *list, char *host, char *source, char *mounton, char *options)
{
    Mount *entry = xcalloc(1, sizeof(Mount));

    if (host)
    {
        entry->host = xstrdup(host);
    }

    if (source)
    {
        entry->source = xstrdup(source);
    }

    if (mounton)
    {
        entry->mounton = xstrdup(mounton);
    }

    if (options)
    {
        entry->options = xstrdup(options);
    }

    SeqAppend(list, entry);
}

/*******************************************************************/

void DeleteMountInfo(Seq *list)
{
    for (size_t i = 0; i < SeqLength(list); i++)
    {
        Mount *entry = SeqAt(list, i);

        if (entry->host)
        {
            free(entry->host);
        }

        if (entry->source)
        {
            free(entry->source);
        }

        if (entry->mounton)
        {
            free(entry->mounton);
        }

        if (entry->options)
        {
            free(entry->options);
        }
    }

    SeqClear(list);
}

/*******************************************************************/

int VerifyInFstab(EvalContext *ctx, char *name, Attributes a, const Promise *pp, PromiseResult *result)
/* Ensure filesystem IS in fstab, and return no of changes */
{
    char fstab[CF_BUFSIZE];
    char *host, *rmountpt, *mountpt, *fstype, *opts;

    if (!FSTABLIST)
    {
        if (!LoadFileAsItemList(&FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a.edits))
        {
            Log(LOG_LEVEL_ERR, "Couldn't open '%s'", VFSTAB[VSYSTEMHARDCLASS]);
            return false;
        }
        else
        {
            FSTAB_EDITS = 0;
        }
    }

    if (a.mount.mount_options)
    {
        opts = Rlist2String(a.mount.mount_options, ",");
    }
    else
    {
        opts = xstrdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
    }

    host = a.mount.mount_server;
    rmountpt = a.mount.mount_source;
    mountpt = name;
    fstype = a.mount.mount_type;

#if defined(__QNX__) || defined(__QNXNTO__)
    snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s %s\t%s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(_CRAY)
    char fstype_upper[CF_BUFSIZE];
    strlcpy(fstype_upper, fstype, CF_BUFSIZE);
    ToUpperStrInplace(fstype_upper);

    snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s %s\t%s", host, rmountpt, mountpt, fstype_upper, opts);
    break;
#elif defined(__hpux)
    snprintf(fstab, CF_BUFSIZE, "%s:%s %s \t %s \t %s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(_AIX)
    snprintf(fstab, CF_BUFSIZE,
             "%s:\n\tdev\t= %s\n\ttype\t= %s\n\tvfs\t= %s\n\tnodename\t= %s\n\tmount\t= true\n\toptions\t= %s\n\taccount\t= false\n",
             mountpt, rmountpt, fstype, fstype, host, opts);
#elif defined(__linux__)
    snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s \t %s \t %s", host, rmountpt, mountpt, fstype, opts);
#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
    snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s \t %s \t %s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(__sun) || defined(sco) || defined(__SCO_DS)
    snprintf(fstab, CF_BUFSIZE, "%s:%s - %s %s - yes %s", host, rmountpt, mountpt, fstype, opts);
#elif defined(__CYGWIN__)
    snprintf(fstab, CF_BUFSIZE, "/bin/mount %s:%s %s", host, rmountpt, mountpt);
#endif

    Log(LOG_LEVEL_VERBOSE, "Verifying '%s' in '%s'", mountpt, VFSTAB[VSYSTEMHARDCLASS]);

    if (!MatchFSInFstab(mountpt))
    {
        AppendItem(&FSTABLIST, fstab, NULL);
        FSTAB_EDITS++;
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Adding file system '%s:%s' to '%s'", host, rmountpt,
             VFSTAB[VSYSTEMHARDCLASS]);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    }

    free(opts);
    return 0;
}

/*******************************************************************/

int VerifyNotInFstab(EvalContext *ctx, char *name, Attributes a, const Promise *pp, PromiseResult *result)
/* Ensure filesystem is NOT in fstab, and return no of changes */
{
    char regex[CF_BUFSIZE];
    char *host, *mountpt, *opts;
    Item *ip;

    if (!FSTABLIST)
    {
        if (!LoadFileAsItemList(&FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a.edits))
        {
            Log(LOG_LEVEL_ERR, "Couldn't open '%s'", VFSTAB[VSYSTEMHARDCLASS]);
            return false;
        }
        else
        {
            FSTAB_EDITS = 0;
        }
    }

    if (a.mount.mount_options)
    {
        opts = Rlist2String(a.mount.mount_options, ",");
    }
    else
    {
        opts = xstrdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
    }

    host = a.mount.mount_server;
    mountpt = name;

    if (MatchFSInFstab(mountpt))
    {
        if (a.mount.editfstab)
        {
#if defined(_AIX)
            FILE *pfp;
            char aixcomm[CF_BUFSIZE];

            snprintf(aixcomm, CF_BUFSIZE, "/usr/sbin/rmnfsmnt -f %s", mountpt);

            if ((pfp = cf_popen(aixcomm, "r", true)) == NULL)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Failed to invoke /usr/sbin/rmnfsmnt to edit fstab");
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                return 0;
            }

            size_t line_size = CF_BUFSIZE;
            char *line = xmalloc(line_size);

            for (;;)
            {
                ssize_t res = getline(&line, &line_size, pfp);

                if (res == -1)
                {
                    if (!feof(pfp))
                    {
                        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to read output of /bin/rmnfsmnt");
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                        cf_pclose(pfp);
                        free(line);
                        return 0;
                    }
                    else
                    {
                        break;
                    }
                }

                if (line[0] == '#')
                {
                    continue;
                }

                if (strstr(line, "busy"))
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp, a, "The device under '%s' cannot be removed from '%s'",
                         mountpt, VFSTAB[VSYSTEMHARDCLASS]);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
                    free(line);
                    return 0;
                }
            }

            free(line);
            cf_pclose(pfp);

            return 0;       /* ignore internal editing for aix , always returns 0 changes */
#else
            Item* next;
            snprintf(regex, CF_BUFSIZE, ".*[\\s]+%s[\\s]+.*", mountpt);

            for (ip = FSTABLIST; ip != NULL; ip = next)
            {
                next = ip->next;
                if (FullTextMatch(ctx, regex, ip->name))
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Deleting file system mounted on '%s'", host);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                    // Check host name matches too?
                    DeleteThisItem(&FSTABLIST, ip);
                    FSTAB_EDITS++;
                }
            }
#endif
        }
    }

    if (a.mount.mount_options)
    {
        free(opts);
    }

    return 0;
}

/*******************************************************************/

PromiseResult VerifyMount(EvalContext *ctx, char *name, Attributes a, const Promise *pp)
{
    char comm[CF_BUFSIZE];
    FILE *pfp;
    char *host, *rmountpt, *mountpt, *opts=NULL;

    host = a.mount.mount_server;
    rmountpt = a.mount.mount_source;
    mountpt = name;

    /* Check for options required for this mount - i.e., -o ro,rsize, etc. */
    if (a.mount.mount_options)
    {
        opts = Rlist2String(a.mount.mount_options, ",");
    }
    else
    {
        opts = xstrdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!DONTDO)
    {
        snprintf(comm, CF_BUFSIZE, "%s -o %s %s:%s %s", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]), opts, host, rmountpt, mountpt);

        if ((pfp = cf_popen(comm, "r", true)) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Failed to open pipe from '%s'", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]));
            return PROMISE_RESULT_FAIL;
        }

        size_t line_size = CF_BUFSIZE;
        char *line = xmalloc(line_size);

        ssize_t res = CfReadLine(&line, &line_size, pfp);

        if (res == -1)
        {
            if (!feof(pfp))
            {
                Log(LOG_LEVEL_ERR, "Unable to read output of mount command. (fread: %s)", GetErrorStr());
                cf_pclose(pfp);
                free(line);
                return PROMISE_RESULT_FAIL;
            }
        }
        else if ((strstr(line, "busy")) || (strstr(line, "Busy")))
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp, a, "The device under '%s' cannot be mounted", mountpt);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            cf_pclose(pfp);
            free(line);
            return 1;
        }

        free(line);
        cf_pclose(pfp);
    }

    /* Since opts is either Rlist2String or xstrdup'd, we need to always free it */
    free(opts);

    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Mounting '%s' to keep promise", mountpt);
    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);

    return result;
}

/*******************************************************************/

PromiseResult VerifyUnmount(EvalContext *ctx, char *name, Attributes a, const Promise *pp)
{
    char comm[CF_BUFSIZE];
    FILE *pfp;
    char *mountpt;

    mountpt = name;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!DONTDO)
    {
        snprintf(comm, CF_BUFSIZE, "%s %s", VUNMOUNTCOMM[VSYSTEMHARDCLASS], mountpt);

        if ((pfp = cf_popen(comm, "r", true)) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Failed to open pipe from %s", VUNMOUNTCOMM[VSYSTEMHARDCLASS]);
            return result;
        }

        size_t line_size = CF_BUFSIZE;
        char *line = xmalloc(line_size);

        ssize_t res = CfReadLine(&line, &line_size, pfp);
        if (res == -1)
        {
            cf_pclose(pfp);
            free(line);

            if (!feof(pfp))
            {
                Log(LOG_LEVEL_ERR, "Unable to read output of unmount command. (fread: %s)", GetErrorStr());
                return result;
            }
        }
        else if (res > 0 && ((strstr(line, "busy")) || (strstr(line, "Busy"))))
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp, a, "The device under '%s' cannot be unmounted", mountpt);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            cf_pclose(pfp);
            free(line);
            return result;
        }
    }

    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Unmounting '%s' to keep promise", mountpt);
    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
    return result;
}

/*******************************************************************/

static int MatchFSInFstab(char *match)
{
    Item *ip;

    for (ip = FSTABLIST; ip != NULL; ip = ip->next)
    {
        if (strstr(ip->name, match))
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

void MountAll()
{
    FILE *pp;

    if (DONTDO)
    {
        Log(LOG_LEVEL_VERBOSE, "Promised to mount filesystem, but not on this trial run");
        return;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Attempting to mount all filesystems.");
    }

#if defined(__CYGWIN__)
    /* This is a shell script. Make sure it hasn't been compromised. */

    struct stat sb;

    if (stat("/etc/fstab", &sb) == -1)
    {
        int fd;
        if ((fd = creat("/etc/fstab", 0755)) > 0)
        {
            if (write(fd, "#!/bin/sh\n\n", 10) != 10)
            {
                UnexpectedError("Failed to write to file '/etc/fstab'");
            }
            close(fd);
        }
        else
        {
            if (sb.st_mode & (S_IWOTH | S_IWGRP))
            {
                Log(LOG_LEVEL_ERR, "File /etc/fstab was insecure. Cannot mount filesystems.");
                return;
            }
        }
    }
#endif

    SetTimeOut(RPCTIMEOUT);

    const char *cmd = VMOUNTCOMM[VSYSTEMHARDCLASS];

    if ((pp = cf_popen(cmd, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to open pipe from '%s'. (cf_popen: %s)",
            cmd, GetErrorStr());
        return;
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    for (;;)
    {
        ssize_t res = CfReadLine(&line, &line_size, pp);
        if (res == -1)
        {
            if (!feof(pp))
            {
                Log(LOG_LEVEL_ERR,
                    "Error reading output of command '%s' (ferror: %s)",
                    cmd, GetErrorStr());
            }
            break;
        }

        if ((strstr(line, "already mounted")) || (strstr(line, "exceeded")) || (strstr(line, "determined")))
        {
            continue;
        }

        if (strstr(line, "not supported"))
        {
            continue;
        }

        if ((strstr(line, "denied")) || (strstr(line, "RPC")))
        {
            Log(LOG_LEVEL_ERR,
                "There was a mount error while trying to mount the filesystems"
                " (command '%s')", cmd);
            break;
        }

        if ((strstr(line, "trying")) && (!strstr(line, "NFS version 2")) && (!strstr(line, "vers 3")))
        {
            Log(LOG_LEVEL_ERR,
                "Attempting filesystems mount aborted because command"
                " '%s' went into a retry loop", cmd);
            break;
        }
    }

    free(line);
    alarm(0);
    signal(SIGALRM, SIG_DFL);
    cf_pclose(pp);
}

/*******************************************************************/
/* Addendum                                                        */
/*******************************************************************/

static void DeleteThisItem(Item **liststart, Item *entry)
{
    Item *ip, *sp;

    if (entry != NULL)
    {
        if (entry->name != NULL)
        {
            free(entry->name);
        }

        sp = entry->next;

        if (entry == *liststart)
        {
            *liststart = sp;
        }
        else
        {
            for (ip = *liststart; ip->next != entry; ip = ip->next)
            {
            }

            ip->next = sp;
        }

        free((char *) entry);
    }
}

void CleanupNFS(void)
{
    Log(LOG_LEVEL_VERBOSE, "Number of changes observed in '%s' is %d", VFSTAB[VSYSTEMHARDCLASS], FSTAB_EDITS);

    if (FSTAB_EDITS && FSTABLIST && !DONTDO)
    {
        RawSaveItemList(FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], NewLineMode_Unix);
        DeleteItemList(FSTABLIST);
        FSTABLIST = NULL;
        FSTAB_EDITS = 0;
    }
}
