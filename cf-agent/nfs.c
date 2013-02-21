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

#include "files_names.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "files_lib.h"
#include "item_lib.h"
#include "conversion.h"
#include "matching.h"
#include "cfstream.h"
#include "string_lib.h"
#include "pipes.h"
#include "logging.h"
#include "nfs.h"
#include "misc_lib.h"
#include "rlist.h"

/* seconds */
#define RPCTIMEOUT 60

int FSTAB_EDITS;
Item *FSTABLIST = NULL;

#ifndef __MINGW32__

static void AugmentMountInfo(Rlist **list, char *host, char *source, char *mounton, char *options);
static int MatchFSInFstab(char *match);
static void DeleteThisItem(Item **liststart, Item *entry);

static const char *VMOUNTCOMM[PLATFORM_CONTEXT_MAX] =
{
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
};

static const char *VUNMOUNTCOMM[PLATFORM_CONTEXT_MAX] =
{
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
};

static const char *VMOUNTOPTS[PLATFORM_CONTEXT_MAX] =
{
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
};

int LoadMountInfo(Rlist **list)
/* This is, in fact, the most portable way to read the mount info! */
/* Depressing, isn't it? */
{
    FILE *pp;
    char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE], buf3[CF_BUFSIZE];
    char host[CF_MAXVARSIZE], source[CF_BUFSIZE], mounton[CF_BUFSIZE], vbuff[CF_BUFSIZE];
    int i, nfs = false;

    for (i = 0; VMOUNTCOMM[VSYSTEMHARDCLASS][i] != ' '; i++)
    {
        buf1[i] = VMOUNTCOMM[VSYSTEMHARDCLASS][i];
    }

    buf1[i] = '\0';

    SetTimeOut(RPCTIMEOUT);

    if ((pp = cf_popen(buf1, "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "cf_popen", "Can't open %s\n", buf1);
        return false;
    }

    do
    {
        vbuff[0] = buf1[0] = buf2[0] = buf3[0] = source[0] = '\0';

        if (ferror(pp))         /* abortable */
        {
            CfOut(OUTPUT_LEVEL_ERROR, "ferror", "Error getting mount info\n");
            break;
        }

        if (CfReadLine(vbuff, CF_BUFSIZE, pp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        if (ferror(pp))         /* abortable */
        {
            CfOut(OUTPUT_LEVEL_ERROR, "ferror", "Error getting mount info\n");
            break;
        }

        if (strstr(vbuff, "nfs"))
        {
            nfs = true;
        }

        sscanf(vbuff, "%s%s%s", buf1, buf2, buf3);

        if ((vbuff[0] == '\0') || (vbuff[0] == '\n'))
        {
            break;
        }

        if (strstr(vbuff, "not responding"))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "%s\n", vbuff);
        }

        if (strstr(vbuff, "be root"))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Mount access is denied. You must be root.\n");
            CfOut(OUTPUT_LEVEL_ERROR, "", "Use the -n option to run safely.");
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
            CfOut(OUTPUT_LEVEL_INFORM, "", "There was an RPC timeout. Aborting mount operations.\n");
            CfOut(OUTPUT_LEVEL_INFORM, "", "Session failed while trying to talk to remote host\n");
            CfOut(OUTPUT_LEVEL_INFORM, "", "%s\n", vbuff);
            cf_pclose(pp);
            return false;
        }

#if defined(__sun) || defined(__hpux)
        if (IsAbsoluteFileName(buf3))
        {
            strcpy(host, "localhost");
            strcpy(mounton, buf1);
        }
        else
        {
            sscanf(buf1, "%[^:]:%s", host, source);
            strcpy(mounton, buf1);
        }
#elif defined(_AIX)
        /* skip header */

        if (IsAbsoluteFileName(buf1))
        {
            strcpy(host, "localhost");
            strcpy(mounton, buf2);
        }
        else
        {
            strcpy(host, buf1);
            strcpy(source, buf1);
            strcpy(mounton, buf3);
        }
#elif defined(__CYGWIN__)
        strcpy(mounton, buf2);
        strcpy(host, buf1);
#elif defined(sco) || defined(__SCO_DS)
        CfOut(OUTPUT_LEVEL_ERROR, "", "Don't understand SCO mount format, no data");
#else
        if (IsAbsoluteFileName(buf1))
        {
            strcpy(host, "localhost");
            strcpy(mounton, buf3);
        }
        else
        {
            sscanf(buf1, "%[^:]:%s", host, source);
            strcpy(mounton, buf3);
        }
#endif

        CfDebug("GOT: host=%s, source=%s, mounton=%s\n", host, source, mounton);

        if (nfs)
        {
            AugmentMountInfo(list, host, source, mounton, "nfs");
        }
        else
        {
            AugmentMountInfo(list, host, source, mounton, NULL);
        }
    }
    while (!feof(pp));

    alarm(0);
    signal(SIGALRM, SIG_DFL);
    cf_pclose(pp);
    return true;
}

/*******************************************************************/

static void AugmentMountInfo(Rlist **list, char *host, char *source, char *mounton, char *options)
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

    RlistAppendAlien(list, (void *) entry);
}

/*******************************************************************/

void DeleteMountInfo(Rlist *list)
{
    Rlist *rp, *sp;
    Mount *entry;

    for (rp = list; rp != NULL; rp = sp)
    {
        sp = rp->next;
        entry = (Mount *) rp->item;

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

        free((char *) entry);
    }
}

/*******************************************************************/

int VerifyInFstab(char *name, Attributes a, Promise *pp)
/* Ensure filesystem IS in fstab, and return no of changes */
{
    char fstab[CF_BUFSIZE];
    char *host, *rmountpt, *mountpt, *fstype, *opts;

    if (!FSTABLIST)
    {
        if (!LoadFileAsItemList(&FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a, pp))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Couldn't open %s!\n", VFSTAB[VSYSTEMHARDCLASS]);
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

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Verifying %s in %s\n", mountpt, VFSTAB[VSYSTEMHARDCLASS]);

    if (!MatchFSInFstab(mountpt))
    {
        AppendItem(&FSTABLIST, fstab, NULL);
        FSTAB_EDITS++;
        cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, "Adding file system %s:%s seems to %s.\n", host, rmountpt,
             VFSTAB[VSYSTEMHARDCLASS]);
    }

    free(opts);
    return 0;
}

/*******************************************************************/

int VerifyNotInFstab(char *name, Attributes a, Promise *pp)
/* Ensure filesystem is NOT in fstab, and return no of changes */
{
    char regex[CF_BUFSIZE];
    char *host, *mountpt, *opts;
    Item *ip;

    if (!FSTABLIST)
    {
        if (!LoadFileAsItemList(&FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a, pp))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Couldn't open %s!\n", VFSTAB[VSYSTEMHARDCLASS]);
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
            char line[CF_BUFSIZE], aixcomm[CF_BUFSIZE];

            snprintf(aixcomm, CF_BUFSIZE, "/usr/sbin/rmnfsmnt -f %s", mountpt);

            if ((pfp = cf_popen(aixcomm, "r")) == NULL)
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Failed to invoke /usr/sbin/rmnfsmnt to edit fstab");
                return 0;
            }

            while (!feof(pfp))
            {
                if (CfReadLine(line, CF_BUFSIZE, pfp) == -1)
                {
                    FatalError("Error in CfReadLine");
                }

                if (line[0] == '#')
                {
                    continue;
                }

                if (strstr(line, "busy"))
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, a, "The device under %s cannot be removed from %s\n",
                         mountpt, VFSTAB[VSYSTEMHARDCLASS]);
                    return 0;
                }
            }

            cf_pclose(pfp);

            return 0;       /* ignore internal editing for aix , always returns 0 changes */
#else
            snprintf(regex, CF_BUFSIZE, ".*[\\s]+%s[\\s]+.*", mountpt);

            for (ip = FSTABLIST; ip != NULL; ip = ip->next)
            {
                if (FullTextMatch(regex, ip->name))
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, "Deleting file system mounted on %s.\n", host);
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

int VerifyMount(char *name, Attributes a, Promise *pp)
{
    char comm[CF_BUFSIZE], line[CF_BUFSIZE];
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

    if (!DONTDO)
    {
        snprintf(comm, CF_BUFSIZE, "%s -o %s %s:%s %s", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]), opts, host, rmountpt, mountpt);

        if ((pfp = cf_popen(comm, "r")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Failed to open pipe from %s\n", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]));
            return 0;
        }

        if (CfReadLine(line, CF_BUFSIZE, pfp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        if ((strstr(line, "busy")) || (strstr(line, "Busy")))
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, a, " !! The device under %s cannot be mounted\n", mountpt);
            cf_pclose(pfp);
            return 1;
        }

        cf_pclose(pfp);
    }

    /* Since opts is either Rlist2String or xstrdup'd, we need to always free it */
    free(opts);

    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, " -> Mounting %s to keep promise\n", mountpt);
    return 0;
}

/*******************************************************************/

int VerifyUnmount(char *name, Attributes a, Promise *pp)
{
    char comm[CF_BUFSIZE], line[CF_BUFSIZE];
    FILE *pfp;
    char *mountpt;

    mountpt = name;

    if (!DONTDO)
    {
        snprintf(comm, CF_BUFSIZE, "%s %s", VUNMOUNTCOMM[VSYSTEMHARDCLASS], mountpt);

        if ((pfp = cf_popen(comm, "r")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Failed to open pipe from %s\n", VUNMOUNTCOMM[VSYSTEMHARDCLASS]);
            return 0;
        }

        if (CfReadLine(line, CF_BUFSIZE, pfp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        if ((strstr(line, "busy")) || (strstr(line, "Busy")))
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, a, " !! The device under %s cannot be unmounted\n", mountpt);
            cf_pclose(pfp);
            return 1;
        }

        cf_pclose(pfp);
    }

    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, " -> Unmounting %s to keep promise\n", mountpt);
    return 0;
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
    char line[CF_BUFSIZE];
    FILE *pp;

    if (DONTDO)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Promised to mount filesystem, but not on this trial run\n");
        return;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Attempting to mount all filesystems.\n");
    }

#if defined(__CYGWIN__)
    /* This is a shell script. Make sure it hasn't been compromised. */

    struct stat sb;

    if (cfstat("/etc/fstab", &sb) == -1)
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "File /etc/fstab was insecure. Cannot mount filesystems.\n");
                return;
            }
        }
    }
#endif

    SetTimeOut(RPCTIMEOUT);

    if ((pp = cf_popen(VMOUNTCOMM[VSYSTEMHARDCLASS], "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "cf_popen", "Failed to open pipe from %s\n", VMOUNTCOMM[VSYSTEMHARDCLASS]);
        return;
    }

    while (!feof(pp))
    {
        if (ferror(pp))         /* abortable */
        {
            CfOut(OUTPUT_LEVEL_INFORM, "ferror", "Error mounting filesystems\n");
            break;
        }

        if (CfReadLine(line, CF_BUFSIZE, pp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        if (ferror(pp))         /* abortable */
        {
            CfOut(OUTPUT_LEVEL_INFORM, "ferror", "Error mounting filesystems\n");
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
            CfOut(OUTPUT_LEVEL_ERROR, "", "There was a mount error, trying to mount one of the filesystems on this host.\n");
            break;
        }

        if ((strstr(line, "trying")) && (!strstr(line, "NFS version 2")) && (!strstr(line, "vers 3")))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Attempting abort because mount went into a retry loop.\n");
            break;
        }
    }

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

#endif /* !__MINGW32__ */
