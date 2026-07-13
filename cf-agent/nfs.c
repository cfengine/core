/*
  Copyright 2024 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
#include <string_sequence.h>
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

static void GetHostAndSource(const char *buf, char *host, char *source);

static bool ConflictingOptions(const char *a, const char *b);
static bool OptionPresent(const char *opt, const Seq *actual);
static bool ConflictingOptionPresent(const char *opt, const Seq *actual);
static char *RemountOptionString(const char *opts);

static void AugmentMountInfo(Seq *list, char *host, char *source, char *mounton, char *fstype, char *options);
static bool MatchFSInFstab(char *match);
static void DeleteThisItem(Item **liststart, Item *entry);
static char *GetFstabEntryOptions(char *mountpt);
static void ReplaceFstabEntry(Item **liststart, char *mountpt, char *new_entry);

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

static void GetHostAndSource(const char *buf, char *host, char *source)
/* Extracts from buffer host & source of nfs, cifs and panfs network systems */
{
    assert(StringStartsWith(buf, "panfs://")
           || StringStartsWith(buf, "//")
           || strchr(buf, ':') != NULL);
    int index = 0; // Index for buf and host
    int source_index = 0;
    size_t slashes = 0;

    // panfs & cifs
    if (StringStartsWith(buf, "panfs://") || StringStartsWith(buf, "//"))
    {
        // copy host
        while (slashes != 3)
        {
            // TODO: Add example of what this string looks like
            if (buf[index] == '/' || buf[index] == '\\')
            {
                slashes++;
            }
            host[index] = buf[index];
            index++;
        }
        host[--index] = '\0';
    }
    // nfs
    else
    {
        while (buf[index] != ':')
        {
            host[index] = buf[index];
            index++;
        }
        host[index++] = '\0';
    }

    // copy source
    while ((buf[index] != '\0') && (buf[index] != ' '))
    {
        source[source_index++] = buf[index++];
    }
    source[source_index] = '\0';
}

/**
 * True if 'a' and 'b' are mutually exclusive mount options that cannot both
 * hold: ro/rw, hard/soft, sync/async, noatime/relatime, or a "no"-prefixed
 * option and its bare form (e.g. noexec/exec).
 */
static bool ConflictingOptions(const char *a, const char *b)
{
    /* A "no"-prefixed option vs its bare form, in either direction. */
    if (StringEqualN(a, "no", 2) && StringEqual(a + 2, b))
    {
        return true;
    }
    if (StringEqualN(b, "no", 2) && StringEqual(b + 2, a))
    {
        return true;
    }

    /* Inverse pairs that do not share the "no" prefix. */
    static const char *const pairs[][2] = {
        { "noatime", "relatime" },
        { "hard", "soft" },
        { "sync", "async" },
        { "ro", "rw" },
    };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
    {
        if ((StringEqual(a, pairs[i][0]) && StringEqual(b, pairs[i][1])) ||
            (StringEqual(a, pairs[i][1]) && StringEqual(b, pairs[i][0])))
        {
            return true;
        }
    }
    return false;
}

/**
 * True if 'opt' is present in the live options, directly or via a
 * tcp/udp<->proto= alias.
 */
static bool OptionPresent(const char *opt, const Seq *actual)
{
    const size_t len = SeqLength(actual);
    for (size_t a = 0; a < len; a++)
    {
        const char *act = SeqAt(actual, a);
        if (StringEqual(opt, act))
        {
            return true;
        }
        if ((StringEqual(opt, "tcp") && StringEqual(act, "proto=tcp")) ||
            (StringEqual(opt, "udp") && StringEqual(act, "proto=udp")) ||
            (StringEqual(opt, "proto=tcp") && StringEqual(act, "tcp")) ||
            (StringEqual(opt, "proto=udp") && StringEqual(act, "udp")))
        {
            return true;
        }
    }
    return false;
}

/**
 * True if an option contradicting 'opt' is present in the live mount.
 */
static bool ConflictingOptionPresent(const char *opt, const Seq *actual)
{
    const size_t len = SeqLength(actual);
    for (size_t a = 0; a < len; a++)
    {
        if (ConflictingOptions(opt, SeqAt(actual, a)))
        {
            return true;
        }
    }
    return false;
}

/**
 * Options for a `mount -o remount,...` command, expanding "defaults" to its
 * positives rw,suid,dev,exec,async: util-linux ignores the flags implied by a
 * bare "defaults" on a remount, so the explicit form is needed to restore a
 * drifted mount in place. Newly allocated (caller frees), NULL if opts empty.
 */
static char *RemountOptionString(const char *opts)
{
    if ((opts == NULL) || (opts[0] == '\0'))
    {
        return NULL;
    }

    Seq *in = SeqStringFromString(opts, ',');
    const size_t len = SeqLength(in);
    Seq *out = SeqNew(len + 4, free);
    for (size_t i = 0; i < len; i++)
    {
        const char *tok = SeqAt(in, i);
        SeqAppend(out, xstrdup(StringEqual(tok, "defaults")
                               ? "rw,suid,dev,exec,async" : tok));
    }
    char *result = StringJoin(out, ",");
    SeqDestroy(in);
    SeqDestroy(out);
    return result;
}

/**
 * Whether the live mount (actual_opts, from /proc/mounts) satisfies the promise.
 * Only named options are enforced; unnamed ones are left alone. The promise is
 * resolved "last wins" like `mount -o` (defaults,ro is read-only, ro,rw is rw),
 * with "defaults" expanded to rw,suid,dev,exec,async (auto/nouser aren't runtime
 * state). Each surviving option must then hold: its inverse absent, and it
 * present or a default-on flag (suid/dev/exec/async only show when negated).
 */
bool OptionsSubsetMatches(const char *promised_opts, const char *actual_opts)
{
    if (promised_opts == NULL || promised_opts[0] == '\0')
    {
        return true;
    }

    Seq *promised = SeqStringFromString(promised_opts, ',');
    Seq *actual = SeqStringFromString(actual_opts, ',');

    /* Expand "defaults" in place, preserving order for the last-wins pass. */
    Seq *eff = SeqNew(16, free);
    const size_t n_promised = SeqLength(promised);
    for (size_t p = 0; p < n_promised; p++)
    {
        const char *opt = SeqAt(promised, p);
        if (StringEqual(opt, "defaults"))
        {
            Seq *comps = SeqStringFromString("rw,suid,dev,exec,async", ',');
            SeqAppendSeq(eff, comps);
            SeqSoftDestroy(comps); /* eff now owns the expanded components */
        }
        else
        {
            SeqAppend(eff, xstrdup(opt));
        }
    }

    /* On-by-default flags the kernel does not echo (only their negatives show). */
    static const char *const default_on[] = { "suid", "dev", "exec", "async" };

    bool mismatch = false;
    const size_t n_eff = SeqLength(eff);
    for (size_t i = 0; (i < n_eff) && !mismatch; i++)
    {
        const char *x = SeqAt(eff, i);

        /* Last wins: skip this option if a later one overrides it (its inverse)
         * or repeats it. */
        bool overridden = false;
        for (size_t j = i + 1; j < n_eff; j++)
        {
            const char *y = SeqAt(eff, j);
            if (ConflictingOptions(x, y))
            {
                Log(LOG_LEVEL_VERBOSE, "Mount option '%s' overridden by later '%s'", x, y);
                overridden = true;
                break;
            }
            if (StringEqual(x, y))
            {
                overridden = true;
                break;
            }
        }
        if (overridden)
        {
            continue;
        }

        const bool is_default_on = IsStringInArray(
            x, default_on, sizeof(default_on) / sizeof(default_on[0]));

        if (ConflictingOptionPresent(x, actual) || !(OptionPresent(x, actual) || is_default_on))
        {
            mismatch = true;
        }
    }

    SeqDestroy(eff);
    SeqDestroy(promised);
    SeqDestroy(actual);

    return !mismatch;
}

bool LoadMountInfo(Seq *list)
/* This is, in fact, the most portable way to read the mount info! */
/* Depressing, isn't it? */
{
    FILE *pp;
    char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE], buf3[CF_BUFSIZE];
    int i;
    bool nfs = false, panfs = false, cifs = false;

    // get mount command without parameters
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

        Seq *words = SeqStringFromString(vbuff, ' ');

        if (SeqStringContains(words, "panfs"))
        {
            panfs = true;
        }
        else if (SeqStringContains(words, "nfs"))
        {
            nfs = true;
        }
        else if (SeqStringContains(words, "cifs"))
        {
            cifs = true;
        }

        SeqDestroy(words);

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
        if (IsAbsoluteFileName(buf3) && !cifs)
        {
            strlcpy(host, "localhost", sizeof(host));
            strlcpy(mounton, buf1, sizeof(mounton));
        }
        else
        {
            if (cifs || panfs)
            {
                GetHostAndSource(buf1, host, source);
            }
            else
            {
                sscanf(buf1, "%255[^:]:%1023s", host, source);
            }
            strlcpy(mounton, buf1, sizeof(mounton));
        }
#elif defined(_AIX)
        /* skip header */

        if (IsAbsoluteFileName(buf1) && !cifs)
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
        if (IsAbsoluteFileName(buf1) && !cifs)
        {
            Log(LOG_LEVEL_VERBOSE,
                "'%s' is a local absolute file name, setting host to localhost",
                buf1);
            strlcpy(host, "localhost", sizeof(host));
            strlcpy(mounton, buf3, sizeof(mounton));
        }
        else
        {
            if (cifs || panfs)
            {
                GetHostAndSource(buf1, host, source);
            }
            else
            {
                sscanf(buf1, "%255[^:]:%1023s", host, source);
            }
            strlcpy(mounton, buf3, sizeof(mounton));
        }
#endif

        Log(LOG_LEVEL_DEBUG, "LoadMountInfo: host '%s', source '%s', mounton '%s'", host, source, mounton);

        /* Extract the actual mount options from the parenthesized portion of mount output.
         * mount -va output format: host:source on /mountpoint type fstype (opts) */
        char mountopts[CF_BUFSIZE];
        mountopts[0] = '\0';
        char *paren = strstr(vbuff, "(");
        if (paren != NULL)
        {
            char *end = strchr(paren, ')');
            if (end != NULL)
            {
                strlcpy(mountopts, paren + 1, sizeof(mountopts));
                /* Strip trailing whitespace */
                size_t len = strlen(mountopts);
                while (len > 0 && (mountopts[len - 1] == ' ' || mountopts[len - 1] == '\t'))
                {
                    mountopts[--len] = '\0';
                }
            }
        }

        if (panfs)
        {
            AugmentMountInfo(list, host, source, mounton, "panfs", mountopts);
        }
        else if (nfs)
        {
            AugmentMountInfo(list, host, source, mounton, "nfs", mountopts);
        }
        else if (cifs)
        {
            AugmentMountInfo(list, host, source, mounton, "cifs", mountopts);
        }
        else
        {
            AugmentMountInfo(list, host, source, mounton, NULL, mountopts);
        }
    }

    free(vbuff);
    alarm(0);
    signal(SIGALRM, SIG_DFL);
    cf_pclose(pp);
    return true;
}

/*******************************************************************/

static void AugmentMountInfo(Seq *list, char *host, char *source, char *mounton, char *fstype, char *options)
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

    /* Store the fstype in options so IsForeignFileSystem can detect
     * foreign filesystems via strstr(entry->options, "nfs"/"panfs"/"cifs"). */
    if (fstype)
    {
        entry->options = xstrdup(fstype);
    }

    /* Store the full kernel-resolved options in raw_opts.
     * For unmounted filesystems (options == NULL or empty), raw_opts stays NULL
     * and will be checked in FileSystemMountedCorrectly as "not mounted". */
    if (options != NULL && options[0] != '\0')
    {
        entry->raw_opts = xstrdup(options);
    }

    SeqAppend(list, entry);
}

/*******************************************************************/

void DeleteMountInfo(Seq *list)
{
    for (size_t i = 0; i < SeqLength(list); i++)
    {
        Mount *entry = SeqAt(list, i);

        free(entry->host);
        free(entry->source);
        free(entry->mounton);
        free(entry->options);
        free(entry->raw_opts);
    }

    SeqClear(list);
}

/*******************************************************************/

int VerifyInFstab(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp, PromiseResult *result)
/* Ensure filesystem IS in fstab, and return no of changes */
{
    assert(a != NULL);
    char fstab[CF_BUFSIZE];
    char *host, *rmountpt, *mountpt, *fstype, *opts;

    if (!FSTABLIST)
    {
        if (!LoadFileAsItemList(&FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a->edits, false))
        {
            Log(LOG_LEVEL_ERR, "Couldn't open '%s'", VFSTAB[VSYSTEMHARDCLASS]);
            return 0;
        }
        else
        {
            FSTAB_EDITS = 0;
        }
    }

    if (a->mount.mount_options)
    {
        opts = Rlist2String(a->mount.mount_options, ",");
    }
    else
    {
        opts = xstrdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
    }

    host = a->mount.mount_server;
    rmountpt = a->mount.mount_source;
    mountpt = name;
    fstype = a->mount.mount_type;

    char device[CF_BUFSIZE];
    if (StringEqual(fstype, "cifs") || StringEqual(fstype, "panfs"))
    {
        NDEBUG_UNUSED int ret = snprintf(device, sizeof(device), "%s%s", host, rmountpt);
        assert(ret >= 0 && ret < CF_BUFSIZE);
    }
    else
    {
        NDEBUG_UNUSED int ret = snprintf(device, sizeof(device), "%s:%s", host, rmountpt);
        assert(ret >= 0 && ret < CF_BUFSIZE);
    }

#if defined(__QNX__) || defined(__QNXNTO__)
    // QNX documents 4 fstab fields : https://www.qnx.com/developers/docs/7.1/index.html#com.qnx.doc.neutrino.utilities/topic/f/fstab.html
    // specialdevice mountpoint type mountoptions
    // TODO Remove DUMP and PASS options used here (unsupported)?
    NDEBUG_UNUSED int ret = snprintf(fstab, CF_BUFSIZE, "%s \t %s %s\t%s 0 0", device, mountpt, fstype, opts);
    assert(ret >= 0 && ret < CF_BUFSIZE);
#elif defined(_CRAY)
    char fstype_upper[CF_BUFSIZE];
    strlcpy(fstype_upper, fstype, CF_BUFSIZE);
    ToUpperStrInplace(fstype_upper);

    NDEBUG_UNUSED int ret = snprintf(fstab, CF_BUFSIZE, "%s \t %s %s\t%s", device, mountpt, fstype_upper, opts);
    assert(ret >= 0 && ret < CF_BUFSIZE);
    break;
#elif defined(__hpux)
    // HP-UX documents 7 fstab fields: https://nixdoc.net/man-pages/HP-UX/man4/fstab.4.html
    // deviceSpecialFile directory type options backupFrequency passNumber comment
    // TODO Bring promise comment in as the 7th comment field # promise comment (stripped of newlines)
    NDEBUG_UNUSED int ret = snprintf(fstab, CF_BUFSIZE, "%s %s \t %s \t %s 0 0", device, mountpt, fstype, opts);
    assert(ret >= 0 && ret < CF_BUFSIZE);
#elif defined(_AIX)
    // AIX uses /etc/filesystems: https://www.ibm.com/docs/en/aix/7.2.0?topic=files-filesystems-file
    NDEBUG_UNUSED int ret = snprintf(fstab, CF_BUFSIZE,
                                     "%s:\n\tdev\t= %s\n\ttype\t= %s\n\tvfs\t= %s\n\tnodename\t= %s\n\tmount\t= true\n\toptions\t= %s\n\taccount\t= false\n",
                                     mountpt, rmountpt, fstype, fstype, host, opts);
    assert(ret >= 0 && ret < CF_BUFSIZE);
#elif defined(__linux__)
    NDEBUG_UNUSED int ret = snprintf(fstab, CF_BUFSIZE, "%s \t %s \t %s \t %s", device, mountpt, fstype, opts);
    assert(ret >= 0 && ret < CF_BUFSIZE);
#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__APPLE__)
    // BSDs document 6 fstab fields https://man.freebsd.org/cgi/man.cgi?fstab(5)
    // Device Mountpoint FStype Options Dump Pass
    NDEBUG_UNUSED int ret = snprintf(fstab, CF_BUFSIZE, "%s \t %s \t %s \t %s 0 0", device, mountpt, fstype, opts);
    assert(ret >= 0 && ret < CF_BUFSIZE);
#elif defined(__sun) || defined(sco) || defined(__SCO_DS)
    // SunOS uses /etc/fstab and documents 7 fields: https://docs.oracle.com/cd/E19455-01/805-6331/fsadm-59727/index.html
    // deviceToMount deviceToFsck mountPoint FStype fsckPass automount? mountOptions
    // - is used for deviceToFsck for read-only and network based file systems
    NDEBUG_UNUSED int ret = snprintf(fstab, CF_BUFSIZE, "%s - %s %s - yes %s", device, mountpt, fstype, opts);
    assert(ret >= 0 && ret < CF_BUFSIZE);
#elif defined(__CYGWIN__)
    // https://cygwin.com/cygwin-ug-net/using.html#mount-table
    NDEBUG_UNUSED int ret = snprintf(fstab, CF_BUFSIZE, "/bin/mount %s %s", device, mountpt);
    assert(ret >= 0 && ret < CF_BUFSIZE);
#else
  #error "Could not determine format of fstab entry on this platform."
#endif

    Log(LOG_LEVEL_VERBOSE, "Verifying '%s' in '%s'", mountpt, VFSTAB[VSYSTEMHARDCLASS]);

    int changes = 0;

    if (!MatchFSInFstab(mountpt))
    {
        /* CFE-90: Entry not in fstab - add it */
        AppendItem(&FSTABLIST, fstab, NULL);
        FSTAB_EDITS++;
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Adding file system entry '%s' to '%s'", fstab,
             VFSTAB[VSYSTEMHARDCLASS]);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        changes += 1;
    }
    else
    {
        /* CFE-90: Entry exists - check if options differ and update if needed */
        char *existing_opts = GetFstabEntryOptions(mountpt);
        if (existing_opts != NULL && strcmp(existing_opts, opts) != 0)
        {
            /* Replace the entire fstab entry with the corrected options */
            ReplaceFstabEntry(&FSTABLIST, mountpt, fstab);
            FSTAB_EDITS++;
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Updating file system entry for '%s' in '%s' (options: '%s' -> '%s')",
                 mountpt, VFSTAB[VSYSTEMHARDCLASS], existing_opts, opts);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            changes += 1;
        }
        free(existing_opts);
    }

    free(opts);
    return changes;
}

/*******************************************************************/

int VerifyNotInFstab(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp, PromiseResult *result)
/* Ensure filesystem is NOT in fstab, and return no of changes */
{
    char regex[CF_BUFSIZE];
    char *host, *mountpt;
    Item *ip;

    if (!FSTABLIST)
    {
        if (!LoadFileAsItemList(&FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a->edits, false))
        {
            Log(LOG_LEVEL_ERR, "Couldn't open '%s'", VFSTAB[VSYSTEMHARDCLASS]);
            return 0;
        }
        else
        {
            FSTAB_EDITS = 0;
        }
    }

    host = a->mount.mount_server;
    mountpt = name;
    int changes = 0;
    if (MatchFSInFstab(mountpt))
    {
        if (a->mount.editfstab)
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
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The device under '%s' cannot be removed from '%s'",
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
                    changes += 1;
                }
            }
#endif
        }
    }

    return changes;
}

/*******************************************************************/

PromiseResult VerifyMount(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp)
{
    assert(a != NULL);

    char comm[CF_BUFSIZE];
    FILE *pfp;
    char *host, *rmountpt, *mountpt, *opts=NULL;

    host = a->mount.mount_server;
    rmountpt = a->mount.mount_source;
    mountpt = name;

    /* Check for options required for this mount - i.e., -o ro,rsize, etc. */
    if (a->mount.mount_options)
    {
        opts = Rlist2String(a->mount.mount_options, ",");
    }
    else
    {
        opts = xstrdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;

    /* CFE-3366: gate the mount on the promise action, not just DONTDO, so a
     * dry-run (or action_policy => "warn") reports a warning and does not
     * define promise_repaired for a run that changes nothing. */
    if (!MakingInternalChanges(ctx, pp, a, &result, "mount '%s' to keep promise", mountpt))
    {
        free(opts);
        return result;
    }

    if (StringEqual(a->mount.mount_type, "panfs"))
    {
        NDEBUG_UNUSED int ret = snprintf(comm, CF_BUFSIZE, "%s -t panfs -o %s %s%s %s", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]), opts, host, rmountpt, mountpt);
        assert(ret >= 0 && ret < CF_BUFSIZE);
    }
    else if (StringEqual(a->mount.mount_type, "cifs"))
    {
        NDEBUG_UNUSED int ret = snprintf(comm, CF_BUFSIZE, "%s -t cifs -o %s %s%s %s", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]), opts, host, rmountpt, mountpt);
        assert(ret >= 0 && ret < CF_BUFSIZE);
    }
    else
    {
        NDEBUG_UNUSED int ret = snprintf(comm, CF_BUFSIZE, "%s -o %s %s:%s %s", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]), opts, host, rmountpt, mountpt);
        assert(ret >= 0 && ret < CF_BUFSIZE);
    }

    if ((pfp = cf_popen(comm, "r", true)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to open pipe from '%s'", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]));
        free(opts);
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
            free(opts);
            return PROMISE_RESULT_FAIL;
        }
    }
    else if ((strstr(line, "busy") != NULL) || (strstr(line, "Busy") != NULL))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The device under '%s' cannot be mounted", mountpt);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        cf_pclose(pfp);
        free(line);
        free(opts);
        return result;
    }

    free(line);
    cf_pclose(pfp);

    /* Since opts is either Rlist2String or xstrdup'd, we need to always free it */
    free(opts);

    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Mounting '%s' to keep promise", mountpt);
    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);

    return result;
}

/*******************************************************************/

PromiseResult VerifyUnmount(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp)
{
    char comm[CF_BUFSIZE];
    FILE *pfp;
    char *mountpt;

    mountpt = name;

    PromiseResult result = PROMISE_RESULT_NOOP;

    /* CFE-3366: gate the unmount on the promise action, not just DONTDO, so a
     * dry-run (or action_policy => "warn") reports a warning and does not
     * define promise_repaired for a run that changes nothing. */
    if (!MakingInternalChanges(ctx, pp, a, &result, "unmount '%s' to keep promise", mountpt))
    {
        return result;
    }

    NDEBUG_UNUSED int ret = snprintf(comm, CF_BUFSIZE, "%s %s", VUNMOUNTCOMM[VSYSTEMHARDCLASS], mountpt);
    assert(ret >= 0 && ret < CF_BUFSIZE);

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
        /* CfReadLine() returns -1 both at end-of-output and on a read error. A
         * successful unmount is silent, so EOF is the normal case here; only a
         * genuine read error should be reported. feof() must be consulted
         * before cf_pclose() closes and invalidates the stream. */
        bool read_error = !feof(pfp);
        cf_pclose(pfp);
        free(line);

        if (read_error)
        {
            Log(LOG_LEVEL_ERR, "Unable to read output of unmount command. (fread: %s)", GetErrorStr());
            return result;
        }
    }
    else if (res > 0 && ((strstr(line, "busy") != NULL) || (strstr(line, "Busy") != NULL)))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The device under '%s' cannot be unmounted", mountpt);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        cf_pclose(pfp);
        free(line);
        return result;
    }

    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Unmounting '%s' to keep promise", mountpt);
    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
    return result;
}

/*******************************************************************/

static bool MatchFSInFstab(char *match)
{
    Item *ip;
    const char delimit[]=" \t\r\n\v\f";
    char *fstab_line, *token;

    for (ip = FSTABLIST; ip != NULL; ip = ip->next)
    {
        fstab_line = xstrdup(ip->name);
        if(strncmp(fstab_line, "#", 1) != 0)
        {
            token = strtok(fstab_line, delimit);
            while (token != NULL)
            {
                if(strcmp(token, match) == 0)
                {
                    free(fstab_line);
                    return true;
                }
                token = strtok(NULL, delimit);
            }
        }
        free(fstab_line);
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

/*******************************************************************/
/* CFE-90: Helper functions for fstab options comparison          */
/*******************************************************************/

static char *GetFstabEntryOptions(char *mountpt)
/* Extract the options field from the fstab entry matching mountpt.
 * Returns a dynamically allocated string or NULL. */
{
    for (Item *ip = FSTABLIST; ip != NULL; ip = ip->next)
    {
        if (ip->name == NULL || ip->name[0] == '#')
        {
            continue;
        }

        /* Parse the fstab line to find the options field */
        char *orig = xstrdup(ip->name);
        char *saveptr = NULL;
        char *token = strtok_r(orig, " \t", &saveptr);
        int field = 0;
        bool found = false;

        while (token != NULL && !found)
        {
            if (field == 1)
            {
                if (StringEqual(token, mountpt))
                {
                    /* Found matching mountpoint - skip field 2 (fstype), return field 3 (options) */
                    char *skip_tok = strtok_r(NULL, " \t", &saveptr); /* field 2: type */
                    if (skip_tok != NULL)
                    {
                        char *tok = strtok_r(NULL, " \t", &saveptr); /* field 3: options */
                        if (tok != NULL)
                        {
                            free(orig);
                            return xstrdup(tok);
                        }
                    }
                }
            }
            field++;
            token = strtok_r(NULL, " \t", &saveptr);
        }
        free(orig);
    }

    return NULL;
}

static void ReplaceFstabEntry(Item **liststart, char *mountpt, char *new_entry)
/* Replace the fstab entry for mountpt with new_entry */
{
    for (Item *ip = *liststart; ip != NULL; ip = ip->next)
    {
        if (ip->name != NULL && ip->name[0] != '#')
        {
            char *orig = xstrdup(ip->name);
            char *saveptr = NULL;
            char *token = strtok_r(orig, " \t", &saveptr);
            int field = 0;
            bool found = false;

            while (token != NULL && !found)
            {
                if (field == 1)
                {
                    if (StringEqual(token, mountpt))
                    {
                        /* Found matching mountpoint - replace the entire line */
                        free(ip->name);
                        ip->name = xstrdup(new_entry);
                        found = true;
                    }
                }
                field++;
                token = strtok_r(NULL, " \t", &saveptr);
            }
            free(orig);
            if (found)
            {
                break;
            }
        }
    }
}

/*******************************************************************/
/* CFE-90: Remount reconciliation                                  */
/*******************************************************************/

/**
 * Re-read the mount table from scratch (the cached global list is stale after
 * a mount operation) and report whether the filesystem now mounted at 'name'
 * satisfies the promise: correct source (when specified) and, when options are
 * promised, a superset of the promised options.  NFS-specific options
 * (transport, NFS version, etc.) cannot be changed by a remount - see nfs(5)
 * "THE REMOUNT OPTION": https://man7.org/linux/man-pages/man5/nfs.5.html
 * The remount call can still return success without applying them, so we
 * verify the resulting state rather than trust the command's exit status.
 */
static bool LiveMountConverged(const char *name, const Attributes *a)
{
    assert(a != NULL);
    if (a == NULL)
    {
        return false;
    }
    Seq *tmp = SeqNew(100, free);
    bool converged = false;

    if (LoadMountInfo(tmp))
    {
        for (size_t i = 0; i < SeqLength(tmp); i++)
        {
            Mount *mp = SeqAt(tmp, i);
            if (mp == NULL || mp->mounton == NULL || !StringEqual(mp->mounton, name))
            {
                continue;
            }

            /* Something is mounted here - check source, then options. */
            if ((a->mount.mount_source != NULL)
                && ((mp->source == NULL) || !StringEqual(mp->source, a->mount.mount_source)))
            {
                converged = false;
            }
            else if (a->mount.mount_options != NULL)
            {
                char *opts = Rlist2String(a->mount.mount_options, ",");
                converged = (mp->raw_opts != NULL) && OptionsSubsetMatches(opts, mp->raw_opts);
                free(opts);
            }
            else
            {
                converged = true;
            }
            break;
        }
    }

    DeleteMountInfo(tmp);
    SeqDestroy(tmp);
    return converged;
}

/**
 * CFE-90: Reconcile an already-mounted filesystem that has drifted from the
 * promise, trying each a->mount.remount_methods mechanism in order (default:
 * just "remount"; the disruptive "unmount_mount" is opt-in) and re-checking
 * after each. Honors DONTDO; reports its own cfPS outcome.
 */
PromiseResult ReconcileMountOptions(EvalContext *ctx, char *name, const Attributes *a, const Promise *pp)
{
    assert(a != NULL);
    if (a == NULL)
    {
        return PROMISE_RESULT_NOOP;
    }
    PromiseResult result = PROMISE_RESULT_NOOP;
    char *opts = Rlist2String(a->mount.mount_options, ",");
    int timeout = (a->mount.remount_timeout != CF_NOINT) ? a->mount.remount_timeout : RPCTIMEOUT;

    /* Ordered method list: promise-specified, else the default remount. */
    Seq *methods = SeqNew(4, NULL); /* borrows const char*, does not own them */
    if (a->mount.remount_methods != NULL)
    {
        for (const Rlist *rp = a->mount.remount_methods; rp != NULL; rp = rp->next)
        {
            SeqAppend(methods, RlistScalarValue(rp));
        }
    }
    else
    {
        /* Default: in-place remount only. unmount_mount tears the filesystem
         * down and back up, so it is opt-in (needed for options a remount
         * can't change, e.g. NFS-negotiated vers=/rsize= or the server). */
        SeqAppend(methods, "remount");
    }

    /* CFE-3366: gate on the promise action, not just DONTDO, so a dry-run or
     * action_policy => "warn" reports a warning without defining
     * promise_repaired on a run that changes nothing. */
    if (!MakingInternalChanges(ctx, pp, a, &result,
             "reconcile mount '%s' to promised options '%s'", name,
             (opts != NULL) ? opts : ""))
    {
        SeqDestroy(methods);
        free(opts);
        return result;
    }

    bool converged = false;
    for (size_t i = 0; (i < SeqLength(methods)) && !converged; i++)
    {
        const char *method = SeqAt(methods, i);

        if (StringEqual(method, "remount"))
        {
            char comm[CF_BUFSIZE];
            char *ropts = RemountOptionString(opts);
            if (ropts != NULL)
            {
                NDEBUG_UNUSED int ret = snprintf(comm, CF_BUFSIZE, "%s -o remount,%s %s",
                                                 CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]), ropts, name);
                assert(ret >= 0 && ret < CF_BUFSIZE);
            }
            else
            {
                NDEBUG_UNUSED int ret = snprintf(comm, CF_BUFSIZE, "%s -o remount %s",
                                                 CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]), name);
                assert(ret >= 0 && ret < CF_BUFSIZE);
            }
            free(ropts);

            Log(LOG_LEVEL_VERBOSE, "Reconciling '%s' via remount: %s", name, comm);
            SetTimeOut(timeout);

            FILE *pfp = cf_popen(comm, "r", true);
            if (pfp == NULL)
            {
                Log(LOG_LEVEL_ERR, "Failed to open pipe from '%s'", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]));
            }
            else
            {
                size_t line_size = CF_BUFSIZE;
                char *line = xmalloc(line_size);
                while (CfReadLine(&line, &line_size, pfp) != -1)
                {
                    /* drain command output */
                }
                free(line);
                cf_pclose(pfp);
            }
        }
        else if (StringEqual(method, "unmount_mount"))
        {
            /* Reuse the tested helpers - both honor DONTDO and build the correct
             * per-fstype command.  This also handles a wrong-source mount, which
             * an in-place remount cannot fix. */
            Log(LOG_LEVEL_VERBOSE, "Reconciling '%s' via unmount + mount", name);
            SetTimeOut(timeout);
            result = PromiseResultUpdate(result, VerifyUnmount(ctx, name, a, pp));
            result = PromiseResultUpdate(result, VerifyMount(ctx, name, a, pp));
        }
        else
        {
            Log(LOG_LEVEL_WARNING, "Unknown remount_method '%s' for '%s' - skipping", method, name);
            continue;
        }

        /* Verify-after-act: confirm the live mount actually satisfies the promise. */
        if (LiveMountConverged(name, a))
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a,
                 "Reconciled mount '%s' to promised options '%s' via '%s'", name,
                 (opts != NULL) ? opts : "", method);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            converged = true;
        }
    }

    if (!converged)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "Could not reconcile mount '%s' to promised options '%s'", name,
             (opts != NULL) ? opts : "");
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    SeqDestroy(methods);
    free(opts);
    return result;
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
