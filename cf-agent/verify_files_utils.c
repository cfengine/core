/*
   Copyright 2017 Northern.tech AS

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

#include <verify_files_utils.h>

#include <actuator.h>
#include <attributes.h>
#include <dir.h>
#include <files_names.h>
#include <files_links.h>
#include <files_copy.h>
#include <files_properties.h>
#include <locks.h>
#include <instrumentation.h>
#include <match_scope.h>
#include <files_interfaces.h>
#include <promises.h>
#include <files_operators.h>
#include <item_lib.h>
#include <client_code.h>
#include <files_hashes.h>
#include <files_repository.h>
#include <files_select.h>
#include <files_changes.h>
#include <expand.h>
#include <conversion.h>
#include <pipes.h>
#include <verify_acl.h>
#include <eval_context.h>
#include <vars.h>
#include <exec_tools.h>
#include <comparray.h>
#include <string_lib.h>
#include <files_lib.h>
#include <rlist.h>
#include <policy.h>
#include <scope.h>
#include <misc_lib.h>
#include <abstract_dir.h>
#include <verify_files_hashes.h>
#include <audit.h>
#include <retcode.h>
#include <cf-agent-enterprise-stubs.h>
#include <conn_cache.h>
#include <stat_cache.h>                      /* remote_stat,StatCacheLookup */
#include <known_dirs.h>

#include <cf-windows-functions.h>

#define CF_RECURSION_LIMIT 100

static const Rlist *AUTO_DEFINE_LIST = NULL; /* GLOBAL_P */

static Item *VSETXIDLIST = NULL;

const Rlist *SINGLE_COPY_LIST = NULL; /* GLOBAL_P */
static Rlist *SINGLE_COPY_CACHE = NULL; /* GLOBAL_X */

static bool TransformFile(EvalContext *ctx, char *file, Attributes attr, const Promise *pp, PromiseResult *result);
static PromiseResult VerifyName(EvalContext *ctx, char *path, struct stat *sb, Attributes attr, const Promise *pp);
static PromiseResult VerifyDelete(EvalContext *ctx,
                                  const char *path, const struct stat *sb,
                                  Attributes attr, const Promise *pp);
static PromiseResult VerifyCopy(EvalContext *ctx, const char *source, char *destination, Attributes attr, const Promise *pp,
                                CompressedArray **inode_cache, AgentConnection *conn);
static PromiseResult TouchFile(EvalContext *ctx, char *path, Attributes attr, const Promise *pp);
static PromiseResult VerifyFileAttributes(EvalContext *ctx, const char *file, struct stat *dstat, Attributes attr, const Promise *pp);
static int PushDirState(EvalContext *ctx, char *name, struct stat *sb);
static bool PopDirState(int goback, char *name, struct stat *sb, DirectoryRecursion r);
static bool CheckLinkSecurity(struct stat *sb, char *name);
static int CompareForFileCopy(char *sourcefile, char *destfile, struct stat *ssb, struct stat *dsb, FileCopy fc, AgentConnection *conn);
static void FileAutoDefine(EvalContext *ctx, char *destfile);
static void TruncateFile(char *name);
static void RegisterAHardLink(int i, char *value, Attributes attr, CompressedArray **inode_cache);
static PromiseResult VerifyCopiedFileAttributes(EvalContext *ctx, const char *src, const char *dest, struct stat *sstat, struct stat *dstat, Attributes attr, const Promise *pp);
static int cf_stat(const char *file, struct stat *buf, FileCopy fc, AgentConnection *conn);
#ifndef __MINGW32__
static int cf_readlink(EvalContext *ctx, char *sourcefile, char *linkbuf, int buffsize, Attributes attr, const Promise *pp, AgentConnection *conn, PromiseResult *result);
#endif
static int SkipDirLinks(EvalContext *ctx, char *path, const char *lastnode, DirectoryRecursion r);
static int DeviceBoundary(struct stat *sb, dev_t rootdevice);
static PromiseResult LinkCopy(EvalContext *ctx, char *sourcefile, char *destfile, struct stat *sb, Attributes attr,
                              const Promise *pp, CompressedArray **inode_cache, AgentConnection *conn);

#ifndef __MINGW32__
static void LoadSetxid(void);
static void SaveSetxid(bool modified);
static PromiseResult VerifySetUidGid(EvalContext *ctx, const char *file, struct stat *dstat, mode_t newperm, const Promise *pp, Attributes attr);
#endif
#ifdef __APPLE__
static int VerifyFinderType(EvalContext *ctx, const char *file, Attributes a, const Promise *pp, PromiseResult *result);
#endif
static void VerifyFileChanges(const char *file, struct stat *sb, Attributes attr, const Promise *pp);
static PromiseResult VerifyFileIntegrity(EvalContext *ctx, const char *file, Attributes attr, const Promise *pp);

extern Attributes GetExpandedAttributes(EvalContext *ctx, const Promise *pp, const Attributes *attr);
extern void ClearExpandedAttributes(Attributes *a);

void SetFileAutoDefineList(const Rlist *auto_define_list)
{
    AUTO_DEFINE_LIST = auto_define_list;
}

void VerifyFileLeaf(EvalContext *ctx, char *path, struct stat *sb, Attributes attr, const Promise *pp, PromiseResult *result)
{
/* Here we can assume that we are in the parent directory of the leaf */

    Log(LOG_LEVEL_VERBOSE, "Handling file existence constraints on '%s'", path);

    /* Update this.promiser again, and overwrite common attributes (classes, action) accordingly */

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser", path, CF_DATA_TYPE_STRING, "source=promise");        // Parameters may only be scalars
    Attributes org_attr = GetFilesAttributes(ctx, pp);
    attr = GetExpandedAttributes(ctx, pp, &org_attr);

    if (attr.transformer != NULL)
    {
        if (!TransformFile(ctx, path, attr, pp, result))
        {
            /* NOP? */
        }
    }
    else
    {
        if (attr.haverename)
        {
            *result = PromiseResultUpdate(*result, VerifyName(ctx, path, sb, attr, pp));
        }

        if (attr.havedelete)
        {
            *result = PromiseResultUpdate(*result, VerifyDelete(ctx, path, sb, attr, pp));
        }

        if (attr.touch)
        {
            *result = PromiseResultUpdate(*result, TouchFile(ctx, path, attr, pp)); // intrinsically non-convergent op
        }
    }

    if (attr.haveperms || attr.havechange || attr.acl.acl_entries)
    {
        if (S_ISDIR(sb->st_mode) && attr.recursion.depth && !attr.recursion.include_basedir &&
            (strcmp(path, pp->promiser) == 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Promise to skip base directory '%s'", path);
        }
        else
        {
            *result = PromiseResultUpdate(*result, VerifyFileAttributes(ctx, path, sb, attr, pp));
        }
    }
    ClearExpandedAttributes(&attr);
}

/* Checks whether item matches a list of wildcards */
static int MatchRlistItem(EvalContext *ctx, const Rlist *listofregex, const char *teststring)
{
    for (const Rlist *rp = listofregex; rp != NULL; rp = rp->next)
    {
        /* Avoid using regex if possible, due to memory leak */

        if (strcmp(teststring, RlistScalarValue(rp)) == 0)
        {
            return (true);
        }

        /* Make it commutative */

        if (FullTextMatch(ctx, RlistScalarValue(rp), teststring))
        {
            return true;
        }
    }

    return false;
}

/* (conn == NULL) then copy is from localhost. */
static PromiseResult CfCopyFile(EvalContext *ctx, char *sourcefile,
                                char *destfile, struct stat ssb,
                                Attributes attr, const Promise *pp,
                                CompressedArray **inode_cache,
                                AgentConnection *conn)
{
    const char *lastnode;
    struct stat dsb;
    int found;
    mode_t srcmode = ssb.st_mode;

    const char *server;
    if (conn != NULL)
    {
        server = conn->this_server;
    }
    else
    {
        server = "localhost";
    }

#ifdef __MINGW32__
    if (attr.copy.copy_links != NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "copy_from.copylink_patterns is ignored on Windows "
            "(source files cannot be symbolic links)");
    }
#endif /* __MINGW32__ */

    attr.link.when_no_file = cfa_force;

    if (strcmp(sourcefile, destfile) == 0 &&
        strcmp(server, "localhost") == 0)
    {
        Log(LOG_LEVEL_INFO,
            "File copy promise loop: file/dir '%s' is its own source",
            sourcefile);
        return PROMISE_RESULT_NOOP;
    }

    if (attr.haveselect && !SelectLeaf(ctx, sourcefile, &ssb, attr.select))
    {
        Log(LOG_LEVEL_DEBUG, "Skipping non-selected file '%s'", sourcefile);
        return PROMISE_RESULT_NOOP;
    }

    if (RlistIsInListOfRegex(SINGLE_COPY_CACHE, destfile))
    {
        Log(LOG_LEVEL_INFO, "Skipping single-copied file '%s'", destfile);
        return PROMISE_RESULT_NOOP;
    }

    if (attr.copy.link_type != FILE_LINK_TYPE_NONE)
    {
        lastnode = ReadLastNode(sourcefile);

        if (MatchRlistItem(ctx, attr.copy.link_instead, lastnode))
        {
            if (MatchRlistItem(ctx, attr.copy.copy_links, lastnode))
            {
                Log(LOG_LEVEL_INFO,
                    "File %s matches both copylink_patterns and linkcopy_patterns"
                    " - promise loop (skipping)!",
                      sourcefile);
                return PROMISE_RESULT_NOOP;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Copy item '%s' marked for linking",
                    sourcefile);
#ifdef __MINGW32__
                Log(LOG_LEVEL_VERBOSE,
                    "Links are not yet supported on Windows"
                    " - copying '%s' instead", sourcefile);
#else
                return LinkCopy(ctx, sourcefile, destfile, &ssb,
                                attr, pp, inode_cache, conn);
#endif
            }
        }
    }

    found = lstat(destfile, &dsb);

    if (found != -1)
    {
        if ((S_ISLNK(dsb.st_mode) && attr.copy.link_type == FILE_LINK_TYPE_NONE)
            || (S_ISLNK(dsb.st_mode) && !S_ISLNK(ssb.st_mode)))
        {
            if (!S_ISLNK(ssb.st_mode) &&
                attr.copy.type_check &&
                attr.copy.link_type != FILE_LINK_TYPE_NONE)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                     "File image exists but destination type is silly "
                     "(file/dir/link doesn't match)");
                PromiseRef(LOG_LEVEL_ERR, pp);
                return PROMISE_RESULT_FAIL;
            }

            if (DONTDO)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Need to remove old symbolic link '%s' "
                    "to make way for copy", destfile);
            }
            else
            {
                if (unlink(destfile) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                         "Couldn't remove link '%s'. (unlink: %s)",
                         destfile, GetErrorStr());
                    return PROMISE_RESULT_FAIL;
                }

                Log(LOG_LEVEL_VERBOSE,
                    "Removing old symbolic link '%s' to make way for copy",
                    destfile);
                found = -1;
            }
        }
    }
    else
    {
        MakeParentDirectory(destfile, true);
    }

    if (attr.copy.min_size != CF_NOINT)
    {
        if ((ssb.st_size < attr.copy.min_size)
            || (ssb.st_size > attr.copy.max_size))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, attr,
                 "Source file '%s' size is not in the permitted safety range",
                 sourcefile);
            return PROMISE_RESULT_NOOP;
        }
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (found == -1)
    {
        if (attr.transaction.action == cfa_warn)
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr,
                 "Image file '%s' is non-existent and should be a copy of '%s'",
                 destfile, sourcefile);
            return PromiseResultUpdate(result, PROMISE_RESULT_WARN);
        }

        if (S_ISREG(srcmode) ||
            (S_ISLNK(srcmode) && attr.copy.link_type == FILE_LINK_TYPE_NONE))
        {
            if (DONTDO)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "'%s' wasn't at destination (needs copying)",
                    destfile);
                return result;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE,
                    "'%s' wasn't at destination (copying)",
                    destfile);

                Log(LOG_LEVEL_INFO, "Copying from '%s:%s'",
                    server, sourcefile);
            }

            if (S_ISLNK(srcmode) && attr.copy.link_type != FILE_LINK_TYPE_NONE)
            {
                Log(LOG_LEVEL_VERBOSE, "'%s' is a symbolic link", sourcefile);
                result = PromiseResultUpdate(
                    result, LinkCopy(ctx, sourcefile, destfile, &ssb,
                                     attr, pp, inode_cache, conn));
            }
            else if (CopyRegularFile(ctx, sourcefile, destfile, ssb, dsb, attr,
                                     pp, inode_cache, conn, &result))
            {
                if (stat(destfile, &dsb) == -1)
                {
                    Log(LOG_LEVEL_ERR,
                        "Can't stat destination file '%s'. (stat: %s)",
                        destfile, GetErrorStr());
                }
                else
                {
                    result = PromiseResultUpdate(
                        result, VerifyCopiedFileAttributes(ctx, sourcefile,
                                                           destfile, &ssb,
                                                           &dsb, attr, pp));
                }

                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, attr,
                     "Updated file from '%s:%s'",
                     server, sourcefile);

                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);

                if (SINGLE_COPY_LIST)
                {
                    RlistPrependScalarIdemp(&SINGLE_COPY_CACHE, destfile);
                }

                if (MatchRlistItem(ctx, AUTO_DEFINE_LIST, destfile))
                {
                    FileAutoDefine(ctx, destfile);
                }
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp,
                     attr, "Copy from '%s:%s' failed",
                     server, sourcefile);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            }

            return result;
        }

        if (S_ISFIFO(srcmode))
        {
#ifdef HAVE_MKFIFO
            if (DONTDO)
            {
                Log(LOG_LEVEL_INFO, "Need to make FIFO '%s'", destfile);
            }
            else if (mkfifo(destfile, srcmode))
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                     "Cannot create fifo '%s'. (mkfifo: %s)",
                     destfile, GetErrorStr());
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                return result;
            }

            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr,
                 "Created fifo '%s'", destfile);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
#endif
        }
        else
        {
#ifndef __MINGW32__                   // only regular files on windows
            if (S_ISBLK(srcmode) || S_ISCHR(srcmode) || S_ISSOCK(srcmode))
            {
                if (DONTDO)
                {
                    Log(LOG_LEVEL_INFO, "Make BLK/CHR/SOCK '%s'", destfile);
                }
                else if (mknod(destfile, srcmode, ssb.st_rdev))
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                         "Cannot create special file '%s'. (mknod: %s)",
                         destfile, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    return result;
                }

                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr,
                     "Created special file/device '%s'.", destfile);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
#endif /* !__MINGW32__ */
        }

        if ((S_ISLNK(srcmode)) && (attr.copy.link_type != FILE_LINK_TYPE_NONE))
        {
            result = PromiseResultUpdate(
                result, LinkCopy(ctx, sourcefile, destfile, &ssb,
                                 attr, pp, inode_cache, conn));
        }
    }
    else
    {
        int ok_to_copy = false;

        Log(LOG_LEVEL_VERBOSE, "Destination file '%s' already exists",
            destfile);

        if (attr.copy.compare == FILE_COMPARATOR_EXISTS)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Existence only is promised, no copying required");
            return result;
        }

        if (!attr.copy.force_update)
        {
            ok_to_copy = CompareForFileCopy(sourcefile, destfile, &ssb,
                                            &dsb, attr.copy, conn);
        }
        else
        {
            ok_to_copy = true;
        }

        if (attr.copy.type_check &&
            attr.copy.link_type != FILE_LINK_TYPE_NONE)
        {
            if (((S_ISDIR(dsb.st_mode)) && (!S_ISDIR(ssb.st_mode))) ||
                ((S_ISREG(dsb.st_mode)) && (!S_ISREG(ssb.st_mode))) ||
                ((S_ISBLK(dsb.st_mode)) && (!S_ISBLK(ssb.st_mode))) ||
                ((S_ISCHR(dsb.st_mode)) && (!S_ISCHR(ssb.st_mode))) ||
                ((S_ISSOCK(dsb.st_mode)) && (!S_ISSOCK(ssb.st_mode))) ||
                ((S_ISFIFO(dsb.st_mode)) && (!S_ISFIFO(ssb.st_mode))) ||
                ((S_ISLNK(dsb.st_mode)) && (!S_ISLNK(ssb.st_mode))))
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                     "Promised file copy %s "
                     "exists but type mismatch with source '%s'",
                     destfile, sourcefile);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                return result;
            }
        }

        if (ok_to_copy && (attr.transaction.action == cfa_warn))
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr,
                 "Image file '%s' exists but is not up to date wrt '%s' "
                 "(only a warning has been promised)",
                 destfile, sourcefile);
            result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            return result;
        }

        if (attr.copy.force_update ||
            ok_to_copy ||
            S_ISLNK(ssb.st_mode))                     /* Always check links */
        {
            if (S_ISREG(srcmode) ||
                attr.copy.link_type == FILE_LINK_TYPE_NONE)
            {
                if (DONTDO)
                {
                    Log(LOG_LEVEL_ERR,
                        "Should update file '%s' from source '%s' on '%s'",
                        destfile, sourcefile, server);
                    return result;
                }

                if (MatchRlistItem(ctx, AUTO_DEFINE_LIST, destfile))
                {
                    FileAutoDefine(ctx, destfile);
                }

                if (CopyRegularFile(ctx, sourcefile, destfile, ssb, dsb, attr,
                                    pp, inode_cache, conn, &result))
                {
                    if (stat(destfile, &dsb) == -1)
                    {
                        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED,
                             pp, attr,
                             "Can't stat destination '%s'. (stat: %s)",
                             destfile, GetErrorStr());
                        result = PromiseResultUpdate(
                            result, PROMISE_RESULT_INTERRUPTED);
                    }
                    else
                    {
                        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp,
                             attr, "Updated '%s' from source '%s' on '%s'",
                             destfile, sourcefile, server);
                        result = PromiseResultUpdate(
                            result, PROMISE_RESULT_CHANGE);
                        result = PromiseResultUpdate(
                            result, VerifyCopiedFileAttributes(ctx, sourcefile,
                                                               destfile, &ssb,
                                                               &dsb, attr, pp));
                    }

                    if (RlistIsInListOfRegex(SINGLE_COPY_LIST, destfile))
                    {
                        RlistPrependScalarIdemp(&SINGLE_COPY_CACHE, destfile);
                    }
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                         "Was not able to copy '%s' to '%s'",
                         sourcefile, destfile);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }

                return result;
            }

            if (S_ISLNK(ssb.st_mode))
            {
                result = PromiseResultUpdate(
                    result, LinkCopy(ctx, sourcefile, destfile, &ssb,
                                     attr, pp, inode_cache, conn));
            }
        }
        else
        {
            result = PromiseResultUpdate(
                result, VerifyCopiedFileAttributes(ctx, sourcefile, destfile,
                                                   &ssb, &dsb, attr, pp));

            /* Now we have to check for single copy, even though nothing was
               copied otherwise we can get oscillations between multipe
               versions if type is based on a checksum */

            if (RlistIsInListOfRegex(SINGLE_COPY_LIST, destfile))
            {
                RlistPrependScalarIdemp(&SINGLE_COPY_CACHE, destfile);
            }

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, attr,
                 "File '%s' is an up to date copy of source", destfile);
        }
    }

    return result;
}

static PromiseResult PurgeLocalFiles(EvalContext *ctx, Item *filelist, const char *localdir, Attributes attr,
                                     const Promise *pp, AgentConnection *conn)
{
    Dir *dirh;
    struct stat sb;
    const struct dirent *dirp;
    char filename[CF_BUFSIZE] = { 0 };

    if (strlen(localdir) < 2)
    {
        Log(LOG_LEVEL_ERR, "Purge of '%s' denied - too dangerous!", localdir);
        return PROMISE_RESULT_NOOP;
    }

    /* If we purge with no authentication we wipe out EVERYTHING ! */

    if (conn && (!conn->authenticated))
    {
        Log(LOG_LEVEL_VERBOSE, "Not purge local files '%s' - no authenticated contact with a source", localdir);
        return PROMISE_RESULT_NOOP;
    }

    if (!attr.havedepthsearch)
    {
        Log(LOG_LEVEL_VERBOSE, "No depth search when copying '%s' so purging does not apply", localdir);
        return PROMISE_RESULT_NOOP;
    }

/* chdir to minimize the risk of race exploits during copy (which is inherently dangerous) */

    if (safe_chdir(localdir) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Can't chdir to local directory '%s'. (chdir: %s)", localdir, GetErrorStr());
        return PROMISE_RESULT_NOOP;
    }

    if ((dirh = DirOpen(".")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Can't open local directory '%s'. (opendir: %s)", localdir, GetErrorStr());
        return PROMISE_RESULT_NOOP;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (!ConsiderLocalFile(dirp->d_name, localdir))
        {
            continue;
        }

        if (!IsItemIn(filelist, dirp->d_name))
        {
            strncpy(filename, localdir, CF_BUFSIZE - 2);

            AddSlash(filename);

            if (strlcat(filename, dirp->d_name, CF_BUFSIZE) >= CF_BUFSIZE)
            {
                Log(LOG_LEVEL_ERR, "Path name is too long in PurgeLocalFiles");
            }

            if (DONTDO || attr.transaction.action == cfa_warn)
            {
                Log(LOG_LEVEL_WARNING, "Need to purge '%s' from copy dest directory", filename);
                result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            }
            else
            {
                Log(LOG_LEVEL_INFO, "Purging '%s' in copy dest directory", filename);

                if (lstat(filename, &sb) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, attr, "Couldn't stat '%s' while purging. (lstat: %s)",
                         filename, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                }
                else if (S_ISDIR(sb.st_mode))
                {
                    if (!DeleteDirectoryTree(filename))
                    {
                        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Unable to purge directory tree '%s'", filename);
                        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    }
                    else if (rmdir(filename) == -1)
                    {
                        if (errno != ENOENT)
                        {
                            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Unable to purge directory '%s'", filename);
                            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                        }
                    }
                }
                else if (unlink(filename) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Couldn't delete '%s' while purging", filename);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }
            }
        }
    }

    DirClose(dirh);

    return result;
}

static PromiseResult SourceSearchAndCopy(EvalContext *ctx, const char *from, char *to, int maxrecurse, Attributes attr,
                                         const Promise *pp, dev_t rootdevice, CompressedArray **inode_cache, AgentConnection *conn)
{
    struct stat sb, dsb;
    /* TODO overflow check all these str*cpy()s in here! */
    char newfrom[CF_BUFSIZE], newto[CF_BUFSIZE];
    Item *namecache = NULL;
    const struct dirent *dirp;
    AbstractDir *dirh;

    if (maxrecurse == 0)        /* reached depth limit */
    {
        Log(LOG_LEVEL_DEBUG, "MAXRECURSE ran out, quitting at level '%s'", from);
        return PROMISE_RESULT_NOOP;
    }

    if (strlen(from) == 0)      /* Check for root dir */
    {
        from = "/";
    }

    /* Check that dest dir exists before starting */

    strlcpy(newto, to, sizeof(newto) - 10);
    AddSlash(newto);
    strcat(newto, "dummy");

    if (attr.transaction.action != cfa_warn)
    {
        struct stat tostat;

        if (!MakeParentDirectory(newto, attr.move_obstructions))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Unable to make directory for '%s' in file-copy '%s' to '%s'", newto,
                 attr.copy.source, attr.copy.destination);
            return PROMISE_RESULT_FAIL;
        }

        DeleteSlash(to);

        /* Set aside symlinks */

        if (lstat(to, &tostat) != 0)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, attr, "Unable to stat newly created directory '%s'. (lstat: %s)",
                 to, GetErrorStr());
            return PROMISE_RESULT_WARN;
        }

        if (S_ISLNK(tostat.st_mode))
        {
            char backup[CF_BUFSIZE];
            mode_t mask;

            if (!attr.move_obstructions)
            {
                Log(LOG_LEVEL_INFO, "Path '%s' is a symlink. Unable to move it aside without move_obstructions is set",
                      to);
                return PROMISE_RESULT_NOOP;
            }

            strcpy(backup, to);
            DeleteSlash(to);
            strcat(backup, ".cf-moved");

            if (rename(to, backup) == -1)
            {
                Log(LOG_LEVEL_INFO, "Unable to backup old '%s'", to);
                unlink(to);
            }

            mask = umask(0);
            if (mkdir(to, DEFAULTMODE) == -1)
            {
                Log(LOG_LEVEL_ERR, "Unable to make directory '%s'. (mkdir: %s)", to, GetErrorStr());
                umask(mask);
                return PROMISE_RESULT_NOOP;
            }
            umask(mask);
        }
    }

    /* Send OPENDIR command. */
    if ((dirh = AbstractDirOpen(from, attr.copy, conn)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp, attr, "copy can't open directory '%s'", from);
        return PROMISE_RESULT_INTERRUPTED;
    }

    /* No backslashes over the network. */
    const char sep = (conn != NULL) ? '/' : FILE_SEPARATOR;

    PromiseResult result = PROMISE_RESULT_NOOP;
    for (dirp = AbstractDirRead(dirh); dirp != NULL; dirp = AbstractDirRead(dirh))
    {
        /* This sends 1st STAT command. */
        if (!ConsiderAbstractFile(dirp->d_name, from, attr.copy, conn))
        {
            if (conn != NULL &&
                conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp,
                     attr, "connection error");
                AbstractDirClose(dirh);
                return PROMISE_RESULT_INTERRUPTED;
            }
            else
            {
                continue;
            }
        }

        if (attr.copy.purge)
        {
            /* Append this file to the list of files not to purge */
            AppendItem(&namecache, dirp->d_name, NULL);
        }

        /* Assemble pathnames. TODO check overflow. */
        strlcpy(newfrom, from, sizeof(newfrom));
        strlcpy(newto, to, sizeof(newto));

        if (!PathAppend(newfrom, sizeof(newfrom), dirp->d_name, sep))
        {
            Log(LOG_LEVEL_ERR, "Internal limit reached in SourceSearchAndCopy(),"
                " source path too long: '%s' + '%s'",
                newfrom, dirp->d_name);
            AbstractDirClose(dirh);
            return result;                             /* TODO return FAIL? */
        }

        /* This issues a 2nd STAT command, hopefully served from cache. */

        if ((attr.recursion.travlinks) || (attr.copy.link_type == FILE_LINK_TYPE_NONE))
        {
            /* No point in checking if there are untrusted symlinks here,
               since this is from a trusted source, by definition */

            if (cf_stat(newfrom, &sb, attr.copy, conn) == -1)
            {
                Log(LOG_LEVEL_VERBOSE, "Can't stat '%s'. (cf_stat: %s)", newfrom, GetErrorStr());
                if (conn != NULL &&
                    conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp,
                         attr, "connection error");
                    AbstractDirClose(dirh);
                    return PROMISE_RESULT_INTERRUPTED;
                }
                else
                {
                    continue;
                }
            }
        }
        else
        {
            if (cf_lstat(newfrom, &sb, attr.copy, conn) == -1)
            {
                Log(LOG_LEVEL_VERBOSE, "Can't stat '%s'. (cf_stat: %s)", newfrom, GetErrorStr());
                if (conn != NULL &&
                    conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
                {
                    cfPS(ctx, LOG_LEVEL_INFO,
                         PROMISE_RESULT_INTERRUPTED, pp, attr,
                         "connection error");
                    AbstractDirClose(dirh);
                    return PROMISE_RESULT_INTERRUPTED;
                }
                else
                {
                    continue;
                }
            }
        }

        /* If "collapse_destination_dir" is set, we skip subdirectories, which
         * means we are not creating them in the destination folder. */

        if (!attr.copy.collapse ||
            (attr.copy.collapse && !S_ISDIR(sb.st_mode)))
        {
            if (!PathAppend(newto, sizeof(newto), dirp->d_name,
                            FILE_SEPARATOR))
            {
                Log(LOG_LEVEL_ERR,
                    "Internal limit reached in SourceSearchAndCopy(),"
                    " dest path too long: '%s' + '%s'",
                    newto, dirp->d_name);
                AbstractDirClose(dirh);
                return result;                         /* TODO result FAIL? */
            }
        }

        if ((attr.recursion.xdev) && (DeviceBoundary(&sb, rootdevice)))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping '%s' on different device", newfrom);
            continue;
        }

        if (S_ISDIR(sb.st_mode))
        {
            if (attr.recursion.travlinks)
            {
                Log(LOG_LEVEL_VERBOSE, "Traversing directory links during copy is too dangerous, pruned");
                continue;
            }

            if (SkipDirLinks(ctx, newfrom, dirp->d_name, attr.recursion))
            {
                continue;
            }

            memset(&dsb, 0, sizeof(struct stat));

            /* Only copy dirs if we are tracking subdirs */

            if ((!attr.copy.collapse) && (stat(newto, &dsb) == -1))
            {
                if (mkdir(newto, 0700) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, attr, "Can't make directory '%s'. (mkdir: %s)",
                         newto, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                    continue;
                }

                if (stat(newto, &dsb) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, attr,
                         "Can't stat local copy '%s' - failed to establish directory. (stat: %s)", newto, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                    continue;
                }
            }

            Log(LOG_LEVEL_VERBOSE, "Entering '%s'", newto);

            if (!attr.copy.collapse)
            {
                VerifyCopiedFileAttributes(ctx, newfrom, newto, &sb, &dsb, attr, pp);
            }

            result = PromiseResultUpdate(result, SourceSearchAndCopy(ctx, newfrom, newto, maxrecurse - 1, attr,
                                                                     pp, rootdevice, inode_cache, conn));
        }
        else
        {
            result = PromiseResultUpdate(result, VerifyCopy(ctx, newfrom, newto, attr, pp, inode_cache, conn));
        }

        if (conn != NULL &&
            conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED, pp,
                 attr, "connection error");
            AbstractDirClose(dirh);
            return PROMISE_RESULT_INTERRUPTED;
        }
    }

    if (attr.copy.purge)
    {
        PurgeLocalFiles(ctx, namecache, to, attr, pp, conn);
        DeleteItemList(namecache);
    }

    AbstractDirClose(dirh);

    return result;
}

static PromiseResult VerifyCopy(EvalContext *ctx,
                                const char *source, char *destination,
                                Attributes attr, const Promise *pp,
                                CompressedArray **inode_cache,
                                AgentConnection *conn)
{
    AbstractDir *dirh;
    char sourcefile[CF_BUFSIZE];
    char sourcedir[CF_BUFSIZE];
    char destdir[CF_BUFSIZE];
    char destfile[CF_BUFSIZE];
    struct stat ssb, dsb;
    const struct dirent *dirp;
    int found;

    if (attr.copy.link_type == FILE_LINK_TYPE_NONE)
    {
        Log(LOG_LEVEL_DEBUG, "Treating links as files for '%s'", source);
        found = cf_stat(source, &ssb, attr.copy, conn);
    }
    else
    {
        found = cf_lstat(source, &ssb, attr.copy, conn);
    }

    if (found == -1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
             "Can't stat '%s' in verify copy", source);
        return PROMISE_RESULT_FAIL;
    }

    if (ssb.st_nlink > 1)      /* Preserve hard link structure when copying */
    {
        RegisterAHardLink(ssb.st_ino, destination, attr, inode_cache);
    }

    if (S_ISDIR(ssb.st_mode))
    {
        PromiseResult result = PROMISE_RESULT_NOOP;

        strcpy(sourcedir, source);
        AddSlash(sourcedir);
        strcpy(destdir, destination);
        AddSlash(destdir);

        if ((dirh = AbstractDirOpen(sourcedir, attr.copy, conn)) == NULL)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                 "Can't open directory '%s'. (opendir: %s)",
                 sourcedir, GetErrorStr());
            return PROMISE_RESULT_FAIL;
        }

        /* Now check any overrides */

        if (stat(destdir, &dsb) == -1)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                 "Can't stat directory '%s'. (stat: %s)",
                 destdir, GetErrorStr());
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
        else
        {
            result = PromiseResultUpdate(
                result, VerifyCopiedFileAttributes(ctx, sourcedir, destdir,
                                                   &ssb, &dsb, attr, pp));
        }

        /* No backslashes over the network. */
        const char sep = (conn != NULL) ? '/' : FILE_SEPARATOR;

        for (dirp = AbstractDirRead(dirh); dirp != NULL;
             dirp = AbstractDirRead(dirh))
        {
            if (!ConsiderAbstractFile(dirp->d_name, sourcedir,
                                      attr.copy, conn))
            {
                if (conn != NULL &&
                    conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_INTERRUPTED,
                         pp, attr, "connection error");
                    return PROMISE_RESULT_INTERRUPTED;
                }
                else
                {
                    continue;
                }
            }

            strcpy(sourcefile, sourcedir);

            if (!PathAppend(sourcefile, sizeof(sourcefile), dirp->d_name,
                            sep))
            {
                /* TODO return FAIL */
                FatalError(ctx, "VerifyCopy sourcefile buffer limit");
            }

            strcpy(destfile, destdir);

            if (!PathAppend(destfile, sizeof(destfile), dirp->d_name,
                            FILE_SEPARATOR))
            {
                /* TODO return FAIL */
                FatalError(ctx, "VerifyCopy destfile buffer limit");
            }

            if (attr.copy.link_type == FILE_LINK_TYPE_NONE)
            {
                if (cf_stat(sourcefile, &ssb, attr.copy, conn) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                         "Can't stat source file (notlinked) '%s'. (stat: %s)",
                         sourcefile, GetErrorStr());
                    return PROMISE_RESULT_FAIL;
                }
            }
            else
            {
                if (cf_lstat(sourcefile, &ssb, attr.copy, conn) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                         "Can't stat source file '%s'. (lstat: %s)",
                         sourcefile, GetErrorStr());
                    return PROMISE_RESULT_FAIL;
                }
            }

            result = PromiseResultUpdate(
                result, CfCopyFile(ctx, sourcefile, destfile, ssb,
                                   attr, pp, inode_cache, conn));
        }

        AbstractDirClose(dirh);
        return result;
    }
    else
    {
        strcpy(sourcefile, source);
        strcpy(destfile, destination);

        return CfCopyFile(ctx, sourcefile, destfile, ssb,
                          attr, pp, inode_cache, conn);
    }
}

static PromiseResult LinkCopy(EvalContext *ctx, char *sourcefile, char *destfile, struct stat *sb, Attributes attr, const Promise *pp,
                              CompressedArray **inode_cache, AgentConnection *conn)
/* Link the file to the source, instead of copying */
#ifdef __MINGW32__
{
    Log(LOG_LEVEL_VERBOSE, "Windows does not support symbolic links");
    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Windows can't link '%s' to '%s'", sourcefile, destfile);
    return PROMISE_RESULT_FAIL;
}
#else                           /* !__MINGW32__ */
{
    char linkbuf[CF_BUFSIZE];
    const char *lastnode;
    struct stat dsb;
    PromiseResult result = PROMISE_RESULT_NOOP;

    linkbuf[0] = '\0';

    if ((S_ISLNK(sb->st_mode)) && (cf_readlink(ctx, sourcefile, linkbuf, CF_BUFSIZE, attr, pp, conn, &result) == -1))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Can't readlink '%s'", sourcefile);
        return PROMISE_RESULT_FAIL;
    }
    else if (S_ISLNK(sb->st_mode))
    {
        Log(LOG_LEVEL_VERBOSE, "Checking link from '%s' to '%s'", destfile, linkbuf);

        if ((attr.copy.link_type == FILE_LINK_TYPE_ABSOLUTE) && (!IsAbsoluteFileName(linkbuf)))        /* Not absolute path - must fix */
        {
            char vbuff[CF_BUFSIZE];

            strlcpy(vbuff, sourcefile, CF_BUFSIZE);
            ChopLastNode(vbuff);
            AddSlash(vbuff);
            strncat(vbuff, linkbuf, CF_BUFSIZE - 1);
            strlcpy(linkbuf, vbuff, CF_BUFSIZE);
        }
    }
    else
    {
        strlcpy(linkbuf, sourcefile, CF_BUFSIZE);
    }

    lastnode = ReadLastNode(sourcefile);

    if (MatchRlistItem(ctx, attr.copy.copy_links, lastnode))
    {
        struct stat ssb;

        ExpandLinks(linkbuf, sourcefile, 0);
        Log(LOG_LEVEL_VERBOSE, "Link item in copy '%s' marked for copying from '%s' instead", sourcefile,
              linkbuf);
        stat(linkbuf, &ssb);
        return CfCopyFile(ctx, linkbuf, destfile, ssb, attr, pp, inode_cache, conn);
    }

    int status;
    switch (attr.copy.link_type)
    {
    case FILE_LINK_TYPE_SYMLINK:

        if (*linkbuf == '.')
        {
            status = VerifyRelativeLink(ctx, destfile, linkbuf, attr, pp);
        }
        else
        {
            status = VerifyLink(ctx, destfile, linkbuf, attr, pp);
        }
        break;

    case FILE_LINK_TYPE_RELATIVE:
        status = VerifyRelativeLink(ctx, destfile, linkbuf, attr, pp);
        break;

    case FILE_LINK_TYPE_ABSOLUTE:
        status = VerifyAbsoluteLink(ctx, destfile, linkbuf, attr, pp);
        break;

    case FILE_LINK_TYPE_HARDLINK:
        status = VerifyHardLink(ctx, destfile, linkbuf, attr, pp);
        break;

    default:
        ProgrammingError("Unhandled link type in switch: %d", attr.copy.link_type);
    }

    if ((status == PROMISE_RESULT_CHANGE) || (status == PROMISE_RESULT_NOOP))
    {
        if (lstat(destfile, &dsb) == -1)
        {
            Log(LOG_LEVEL_ERR, "Can't lstat '%s'. (lstat: %s)", destfile, GetErrorStr());
        }
        else
        {
            result = PromiseResultUpdate(result, VerifyCopiedFileAttributes(ctx, sourcefile, destfile, sb, &dsb, attr, pp));
        }

        if (status == PROMISE_RESULT_CHANGE)
        {
            cfPS(ctx, LOG_LEVEL_INFO, status, pp, attr, "Created link '%s'", destfile);
            result = PromiseResultUpdate(result, status);
        }
        else if (status == PROMISE_RESULT_NOOP)
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, status, pp, attr, "Link '%s' as promised", destfile);
            result = PromiseResultUpdate(result, status);
        }
        else // TODO: is this reachable?
        {
            cfPS(ctx, LOG_LEVEL_INFO, status, pp, attr, "Unable to create link '%s'", destfile);
            result = PromiseResultUpdate(result, status);
        }
    }

    return result;
}
#endif /* !__MINGW32__ */

bool CopyRegularFile(EvalContext *ctx, const char *source, const char *dest, struct stat sstat, struct stat dstat,
                     Attributes attr, const Promise *pp, CompressedArray **inode_cache,
                     AgentConnection *conn, PromiseResult *result)
{
    char backup[CF_BUFSIZE];
    char new[CF_BUFSIZE], *linkable;
    int remote = false, backupisdir = false, backupok = false, discardbackup;
    struct stat s;

#ifdef HAVE_UTIME_H
    struct utimbuf timebuf;
#endif

#ifdef __APPLE__
/* For later copy from new to dest */
    char *rsrcbuf;
    int rsrcbytesr;             /* read */
    int rsrcbytesw;             /* written */
    int rsrcbytesl;             /* to read */
    int rsrcrd;
    int rsrcwd;

/* Keep track of if a resrouce fork */
    int rsrcfork = 0;
#endif

    discardbackup = ((attr.copy.backup == BACKUP_OPTION_NO_BACKUP) || (attr.copy.backup == BACKUP_OPTION_REPOSITORY_STORE));

    if (DONTDO)
    {
        Log(LOG_LEVEL_ERR, "Promise requires copy from '%s' to '%s'", source, dest);
        return false;
    }

    /* Make an assoc array of inodes used to preserve hard links */

    linkable = CompressedArrayValue(*inode_cache, sstat.st_ino);

    if (sstat.st_nlink > 1)     /* Preserve hard links, if possible */
    {
        if ((CompressedArrayElementExists(*inode_cache, sstat.st_ino)) && (strcmp(dest, linkable) != 0))
        {
            unlink(dest);
            MakeHardLink(ctx, dest, linkable, attr, pp, result);
            return true;
        }
    }

    if (conn != NULL)
    {
        assert(attr.copy.servers && strcmp(RlistScalarValue(attr.copy.servers), "localhost"));
        Log(LOG_LEVEL_DEBUG, "This is a remote copy from server '%s'", RlistScalarValue(attr.copy.servers));
        remote = true;
    }

#ifdef __APPLE__
    if (strstr(dest, _PATH_RSRCFORKSPEC))
    {
        char *tmpstr = xstrndup(dest, CF_BUFSIZE);

        rsrcfork = 1;
        /* Drop _PATH_RSRCFORKSPEC */
        char *forkpointer = strstr(tmpstr, _PATH_RSRCFORKSPEC);
        *forkpointer = '\0';

        strlcpy(new, tmpstr, CF_BUFSIZE);

        free(tmpstr);
    }
    else
    {
#endif

        strlcpy(new, dest, CF_BUFSIZE);

        if (!JoinSuffix(new, sizeof(new), CF_NEW))
        {
            Log(LOG_LEVEL_ERR, "Unable to construct filename for copy");
            return false;
        }

#ifdef __APPLE__
    }
#endif

    if (remote)
    {
        if (conn->error)
        {
            return false;
        }

        if (!CopyRegularFileNet(source, new, sstat.st_size, attr.copy.encrypt, conn))
        {
            return false;
        }
    }
    else
    {
        if (!CopyRegularFileDisk(source, new))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Failed copying file '%s' to '%s'", source, new);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return false;
        }

        if (attr.copy.stealth)
        {
#ifdef HAVE_UTIME_H
            timebuf.actime = sstat.st_atime;
            timebuf.modtime = sstat.st_mtime;
            utime(source, &timebuf);
#endif
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Copy of regular file succeeded '%s' to '%s'", source, new);

    backup[0] = '\0';

    if (!discardbackup)
    {
        char stamp[CF_BUFSIZE];
        time_t stampnow;

        Log(LOG_LEVEL_DEBUG, "Backup file '%s'", source);

        strlcpy(backup, dest, CF_BUFSIZE);

        if (attr.copy.backup == BACKUP_OPTION_TIMESTAMP)
        {
            stampnow = time((time_t *) NULL);
            snprintf(stamp, CF_BUFSIZE - 1, "_%lu_%s", CFSTARTTIME, CanonifyName(ctime(&stampnow)));

            if (!JoinSuffix(backup, sizeof(backup), stamp))
            {
                return false;
            }
        }

        if (!JoinSuffix(backup, sizeof(backup), CF_SAVED))
        {
            return false;
        }

        /* Now in case of multiple copies of same object, try to avoid overwriting original backup */

        if (lstat(backup, &s) != -1)
        {
            if (S_ISDIR(s.st_mode))     /* if there is a dir in the way */
            {
                backupisdir = true;
                PurgeLocalFiles(ctx, NULL, backup, attr, pp, conn);
                rmdir(backup);
            }

            unlink(backup);
        }

        if (rename(dest, backup) == -1)
        {
            /* ignore */
        }

        backupok = (lstat(backup, &s) != -1);   /* Did the rename() succeed? NFS-safe */
    }
    else
    {
        /* Mainly important if there is a dir in the way */

        if (stat(dest, &s) != -1)
        {
            if (S_ISDIR(s.st_mode))
            {
                PurgeLocalFiles(ctx, NULL, dest, attr, pp, conn);
                rmdir(dest);
            }
        }
    }

    if (lstat(new, &dstat) == -1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Can't stat new file '%s' - another agent has picked it up?. (stat: %s)",
             new, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

    if ((S_ISREG(dstat.st_mode)) && (dstat.st_size != sstat.st_size))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
             "New file '%s' seems to have been corrupted in transit, destination %d and source %d, aborting.", new,
             (int) dstat.st_size, (int) sstat.st_size);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);

        if (backupok)
        {
            rename(backup, dest);    /* ignore failure of this call, as there is nothing more we can do */
        }

        return false;
    }

    if (attr.copy.verify)
    {
        Log(LOG_LEVEL_VERBOSE, "Final verification of transmission ...");

        if (CompareFileHashes(source, new, &sstat, &dstat, attr.copy, conn))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                 "New file '%s' seems to have been corrupted in transit, aborting.", new);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);

            if (backupok)
            {
                rename(backup, dest);
            }

            return false;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "New file '%s' transmitted correctly - verified", new);
        }
    }

#ifdef __APPLE__
    if (rsrcfork)
    {                           /* Can't just "mv" the resource fork, unfortunately */
        rsrcrd = safe_open(new, O_RDONLY | O_BINARY);
        rsrcwd = safe_open(dest, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0600);

        if (rsrcrd == -1 || rsrcwd == -1)
        {
            Log(LOG_LEVEL_INFO, "Open of Darwin resource fork rsrcrd/rsrcwd failed. (open: %s)", GetErrorStr());
            close(rsrcrd);
            close(rsrcwd);
            return (false);
        }

        rsrcbuf = xmalloc(CF_BUFSIZE);

        rsrcbytesr = 0;

        while (1)
        {
            rsrcbytesr = read(rsrcrd, rsrcbuf, CF_BUFSIZE);

            if (rsrcbytesr == -1)
            {                   /* Ck error */
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    Log(LOG_LEVEL_INFO, "Read of Darwin resource fork rsrcrd failed. (read: %s)", GetErrorStr());
                    close(rsrcrd);
                    close(rsrcwd);
                    free(rsrcbuf);
                    return (false);
                }
            }

            else if (rsrcbytesr == 0)
            {
                /* Reached EOF */
                close(rsrcrd);
                close(rsrcwd);
                free(rsrcbuf);

                unlink(new);    /* Go ahead and unlink .cfnew */
                break;
            }

            rsrcbytesl = rsrcbytesr;
            rsrcbytesw = 0;

            while (rsrcbytesl > 0)
            {
                rsrcbytesw += write(rsrcwd, rsrcbuf, rsrcbytesl);

                if (rsrcbytesw == -1)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        Log(LOG_LEVEL_INFO, "Write of Darwin resource fork rsrcwd failed.");
                        close(rsrcrd);
                        close(rsrcwd);
                        free(rsrcbuf);
                        return (false);
                    }
                }
                rsrcbytesl = rsrcbytesr - rsrcbytesw;
            }
        }
    }
    else
    {
#endif

        if (rename(new, dest) == -1)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                 "Could not install copy file as '%s', directory in the way?. (rename: %s)",
                 dest, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);

            if (backupok)
            {
                rename(backup, dest);        /* ignore failure */
            }

            return false;
        }

#ifdef __APPLE__
    }
#endif

    if ((!discardbackup) && backupisdir)
    {
        Log(LOG_LEVEL_INFO, "Cannot move a directory to repository, leaving at '%s'", backup);
    }
    else if ((!discardbackup) && (ArchiveToRepository(backup, attr)))
    {
        unlink(backup);
    }

    if (attr.copy.stealth)
    {
#ifdef HAVE_UTIME_H
        timebuf.actime = sstat.st_atime;
        timebuf.modtime = sstat.st_mtime;
        utime(dest, &timebuf);
#endif
    }

    return true;
}

static bool TransformFile(EvalContext *ctx, char *file, Attributes attr, const Promise *pp, PromiseResult *result)
{
    FILE *pop = NULL;
    int transRetcode = 0;

    if (attr.transformer == NULL || file == NULL)
    {
        return false;
    }

    Buffer *command = BufferNew();
    ExpandScalar(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, attr.transformer, command);
    Log(LOG_LEVEL_INFO, "Transforming '%s' ", BufferData(command));

    if (!IsExecutable(CommandArg0(BufferData(command))))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Transformer '%s' for file '%s' failed", attr.transformer, file);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        BufferDestroy(command);
        return false;
    }

    if (!DONTDO)
    {
        CfLock thislock = AcquireLock(ctx, BufferData(command), VUQNAME, CFSTARTTIME, attr.transaction, pp, false);

        if (thislock.lock == NULL)
        {
            BufferDestroy(command);
            return false;
        }

        if ((pop = cf_popen(BufferData(command), "r", true)) == NULL)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Transformer '%s' for file '%s' failed", attr.transformer, file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            YieldCurrentLock(thislock);
            BufferDestroy(command);
            return false;
        }

        size_t line_size = CF_BUFSIZE;
        char *line = xmalloc(line_size);

        for (;;)
        {
            ssize_t res = CfReadLine(&line, &line_size, pop);
            if (res == -1)
            {
                if (!feof(pop))
                {
                    cf_pclose(pop);
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Transformer '%s' for file '%s' failed", attr.transformer, file);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                    YieldCurrentLock(thislock);
                    free(line);
                    BufferDestroy(command);
                    return false;
                }
                else
                {
                    break;
                }
            }

            Log(LOG_LEVEL_INFO, "%s", line);
        }

        free(line);

        transRetcode = cf_pclose(pop);

        if (VerifyCommandRetcode(ctx, transRetcode, attr, pp, result))
        {
            Log(LOG_LEVEL_INFO, "Transformer '%s' => '%s' seemed to work ok", file, BufferData(command));
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Transformer '%s' => '%s' returned error", file, BufferData(command));
        }

        YieldCurrentLock(thislock);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Need to transform file '%s' with '%s'", file, BufferData(command));
    }

    BufferDestroy(command);
    return true;
}

static PromiseResult VerifyName(EvalContext *ctx, char *path, struct stat *sb, Attributes attr, const Promise *pp)
{
    mode_t newperm;
    struct stat dsb;

    if (lstat(path, &dsb) == -1)
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, attr, "File object named '%s' is not there (promise kept)", path);
        return PROMISE_RESULT_NOOP;
    }
    else
    {
        if (attr.rename.disable)
        {
            Log(LOG_LEVEL_WARNING, "File object '%s' exists, contrary to promise", path);
        }
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (attr.rename.newname)
    {
        if (DONTDO)
        {
            Log(LOG_LEVEL_INFO, "File '%s' should be renamed to '%s' to keep promise", path, attr.rename.newname);
            return PROMISE_RESULT_NOOP;
        }
        else
        {
            if (!FileInRepository(attr.rename.newname))
            {
                if (rename(path, attr.rename.newname) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Error occurred while renaming '%s'. (rename: %s)",
                         path, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Renaming file '%s' to '%s'", path, attr.rename.newname);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, attr,
                     "Rename to same destination twice? Would overwrite saved copy - aborting");
                result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            }
        }

        return result;
    }

    if (S_ISLNK(dsb.st_mode))
    {
        if (attr.rename.disable)
        {
            if (!DONTDO)
            {
                if (unlink(path) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Unable to unlink '%s'. (unlink: %s)",
                         path, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Disabling symbolic link '%s' by deleting it", path);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }
            }
            else
            {
                Log(LOG_LEVEL_INFO, "Need to disable link '%s' to keep promise", path);
            }

            return result;
        }
    }

/* Normal disable - has priority */

    if (attr.rename.disable)
    {
        char newname[CF_BUFSIZE];

        if (attr.transaction.action == cfa_warn)
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "'%s' '%s' should be renamed",
                 S_ISDIR(sb->st_mode) ? "Directory" : "File", path);
            result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            return result;
        }

        if (attr.rename.newname && strlen(attr.rename.newname) > 0)
        {
            if (IsAbsPath(attr.rename.newname))
            {
                strlcpy(path, attr.rename.newname, CF_BUFSIZE);
            }
            else
            {
                strcpy(newname, path);
                ChopLastNode(newname);

                if (!PathAppend(newname, sizeof(newname),
                                attr.rename.newname, FILE_SEPARATOR))
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                         "Internal buffer limit in rename operation,"
                         " destination: '%s' + '%s'",
                         newname, attr.rename.newname);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    return result;
                }
            }
        }
        else
        {
            strcpy(newname, path);

            if (attr.rename.disable_suffix)
            {
                if (!JoinSuffix(newname, sizeof(newname), attr.rename.disable_suffix))
                {
                    return result;
                }
            }
            else
            {
                if (!JoinSuffix(newname, sizeof(newname), ".cfdisabled"))
                {
                    return result;
                }
            }
        }

        if ((attr.rename.plus != CF_SAMEMODE) && (attr.rename.minus != CF_SAMEMODE))
        {
            newperm = (sb->st_mode & 07777);
            newperm |= attr.rename.plus;
            newperm &= ~(attr.rename.minus);
        }
        else
        {
            newperm = (mode_t) 0600;
        }

        if (DONTDO)
        {
            Log(LOG_LEVEL_INFO, "File '%s' should be renamed to '%s' to keep promise", path, newname);
            return result;
        }
        else
        {
            safe_chmod(path, newperm);

            if (!FileInRepository(newname))
            {
                if (rename(path, newname) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Error occurred while renaming '%s'. (rename: %s)",
                         path, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    return result;
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Disabling/renaming file '%s' to '%s' with mode %04jo", path,
                         newname, (uintmax_t)newperm);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }

                if (ArchiveToRepository(newname, attr))
                {
                    unlink(newname);
                }
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, attr,
                     "Disable required twice? Would overwrite saved copy - changing permissions only");
                result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            }
        }

        return result;
    }

    if (attr.rename.rotate == 0)
    {
        if (attr.transaction.action == cfa_warn)
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "File '%s' should be truncated", path);
            result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
        }
        else if (!DONTDO)
        {
            TruncateFile(path);
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Truncating (emptying) '%s'", path);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            Log(LOG_LEVEL_ERR, " * File '%s' needs emptying", path);
        }
        return result;
    }

    if (attr.rename.rotate > 0)
    {
        if (attr.transaction.action == cfa_warn)
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "File '%s' should be rotated", path);
            result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
        }
        else if (!DONTDO)
        {
            RotateFiles(path, attr.rename.rotate);
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Rotating files '%s' in %d fifo", path, attr.rename.rotate);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "File '%s' needs rotating", path);
        }

        return result;
    }

    return result;
}

static PromiseResult VerifyDelete(EvalContext *ctx,
                                  const char *path, const struct stat *sb,
                                  Attributes attr, const Promise *pp)
{
    const char *lastnode = ReadLastNode(path);
    Log(LOG_LEVEL_VERBOSE, "Verifying file deletions for '%s'", path);

    if (DONTDO)
    {
        Log(LOG_LEVEL_INFO, "Promise requires deletion of file object '%s'",
            path);
        return PROMISE_RESULT_NOOP;
    }

    switch (attr.transaction.action)
    {
    case cfa_warn:

        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr,
             "%s '%s' should be deleted",
             S_ISDIR(sb->st_mode) ? "Directory" : "File", path);
        return PROMISE_RESULT_WARN;
        break;

    case cfa_fix:

        if (!S_ISDIR(sb->st_mode))                      /* file,symlink */
        {
            int ret = unlink(lastnode);
            if (ret == -1)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                     "Couldn't unlink '%s' tidying. (unlink: %s)",
                     path, GetErrorStr());
                return PROMISE_RESULT_FAIL;
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr,
                     "Deleted file '%s'", path);
                return PROMISE_RESULT_CHANGE;
            }
        }
        else                                               /* directory */
        {
            if (!attr.delete.rmdirs)
            {
                Log(LOG_LEVEL_VERBOSE, "Keeping directory '%s' "
                    "since \"rmdirs\" attribute was not specified",
                    path);
                return PROMISE_RESULT_NOOP;
            }

            if (attr.havedepthsearch && strcmp(path, pp->promiser) == 0)
            {
                Log(LOG_LEVEL_DEBUG,
                    "Skipping deletion of parent directory for recursive promise '%s', "
                    "you must specify separate promise for deleting",
                    path);
                return PROMISE_RESULT_NOOP;
            }

            int ret = rmdir(lastnode);
            if (ret == -1 && errno != EEXIST && errno != ENOTEMPTY)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                     "Delete directory '%s' failed (rmdir: %s)",
                     path, GetErrorStr());
                return PROMISE_RESULT_FAIL;
            }
            else if (ret == -1 &&
                     (errno == EEXIST || errno == ENOTEMPTY))
            {
                /* It's never allowed to delete non-empty directories, they
                 * are silently skipped. */
                Log(LOG_LEVEL_VERBOSE,
                    "Delete directory '%s' not empty, skipping", path);
                return PROMISE_RESULT_NOOP;
            }
            else
            {
                assert(ret != -1);
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr,
                     "Deleted directory '%s'", path);
                return PROMISE_RESULT_CHANGE;
            }
        }
        break;

    default:
        ProgrammingError("Unhandled file action in switch: %d",
                         attr.transaction.action);
    }

    assert(false);                                          /* Unreachable! */
    return PROMISE_RESULT_NOOP;
}

static PromiseResult TouchFile(EvalContext *ctx, char *path, Attributes attr, const Promise *pp)
{
    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!DONTDO && attr.transaction.action == cfa_fix)
    {
        if (utime(path, NULL) != -1)
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Touched (updated time stamps) for path '%s'", path);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                 "Touch '%s' failed to update timestamps. (utime: %s)", path, GetErrorStr());
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr,
             "Need to touch (update time stamps) for '%s', but only a warning was promised!", path);
        result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
    }

    return result;
}

PromiseResult VerifyFileAttributes(EvalContext *ctx, const char *file, struct stat *dstat, Attributes attr, const Promise *pp)
{
    PromiseResult result = PROMISE_RESULT_NOOP;

#ifndef __MINGW32__
    mode_t newperm = dstat->st_mode, maskvalue;

# if defined HAVE_CHFLAGS
    u_long newflags;
# endif

    maskvalue = umask(0);       /* This makes the DEFAULT modes absolute */

    newperm = (dstat->st_mode & 07777);

    if ((attr.perms.plus != CF_SAMEMODE) && (attr.perms.minus != CF_SAMEMODE))
    {
        newperm |= attr.perms.plus;
        newperm &= ~(attr.perms.minus);
        /* directories must have x set if r set, regardless  */

        if (S_ISDIR(dstat->st_mode))
        {
            if (attr.perms.rxdirs)
            {
                Log(LOG_LEVEL_DEBUG, "Directory...fixing x bits");

                if (newperm & S_IRUSR)
                {
                    newperm |= S_IXUSR;
                }

                if (newperm & S_IRGRP)
                {
                    newperm |= S_IXGRP;
                }

                if (newperm & S_IROTH)
                {
                    newperm |= S_IXOTH;
                }
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "NB: rxdirs is set to false - x for r bits not checked");
            }
        }
    }

    result = PromiseResultUpdate(result, VerifySetUidGid(ctx, file, dstat, newperm, pp, attr));

# ifdef __APPLE__
    if (VerifyFinderType(ctx, file, attr, pp, &result))
    {
        /* nop */
    }
# endif
#endif

    if (VerifyOwner(ctx, file, pp, attr, dstat, &result))
    {
        /* nop */
    }

#ifdef __MINGW32__
    if (NovaWin_FileExists(file) && !NovaWin_IsDir(file))
#else
    if (attr.havechange && S_ISREG(dstat->st_mode))
#endif
    {
        result = PromiseResultUpdate(result, VerifyFileIntegrity(ctx, file, attr, pp));
    }

    if (attr.havechange)
    {
        VerifyFileChanges(file, dstat, attr, pp);
    }

#ifndef __MINGW32__
    if (S_ISLNK(dstat->st_mode))        /* No point in checking permission on a link */
    {
        KillGhostLink(ctx, file, attr, pp, &result);
        umask(maskvalue);
        return result;
    }
#endif


#ifndef __MINGW32__
    result = PromiseResultUpdate(result, VerifySetUidGid(ctx, file, dstat, dstat->st_mode, pp, attr));

    if ((newperm & 07777) == (dstat->st_mode & 07777))  /* file okay */
    {
        Log(LOG_LEVEL_DEBUG, "File okay, newperm '%jo', stat '%jo'",
            (uintmax_t) (newperm & 07777), (uintmax_t) (dstat->st_mode & 07777));
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, attr,
             "File permissions on '%s' as promised", file);
        result = PromiseResultUpdate(result, PROMISE_RESULT_NOOP);
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Trying to fix mode...newperm '%jo', stat '%jo'",
            (uintmax_t) (newperm & 07777), (uintmax_t) (dstat->st_mode & 07777));

        if (attr.transaction.action == cfa_warn || DONTDO)
        {

            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr,
                 "'%s' has permission %04jo - [should be %04jo]", file,
                 (uintmax_t)dstat->st_mode & 07777, (uintmax_t)newperm & 07777);
            result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
        }
        else if (attr.transaction.action == cfa_fix)
        {
            if (safe_chmod(file, newperm & 07777) == -1)
            {
                Log(LOG_LEVEL_ERR,
                    "chmod failed on '%s'. (chmod: %s)",
                    file, GetErrorStr());
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr,
                     "Object '%s' had permission %04jo, changed it to %04jo", file,
                     (uintmax_t)dstat->st_mode & 07777, (uintmax_t)newperm & 07777);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
        }
        else
        {
            ProgrammingError("Unhandled file action in switch: %d",
                             attr.transaction.action);
        }
    }

# if defined HAVE_CHFLAGS       /* BSD special flags */

    newflags = (dstat->st_flags & CHFLAGS_MASK);
    newflags |= attr.perms.plus_flags;
    newflags &= ~(attr.perms.minus_flags);

    if ((newflags & CHFLAGS_MASK) == (dstat->st_flags & CHFLAGS_MASK))  /* file okay */
    {
        Log(LOG_LEVEL_DEBUG, "BSD File okay, flags '%jx', current '%jx'",
                (uintmax_t) (newflags & CHFLAGS_MASK),
                (uintmax_t) (dstat->st_flags & CHFLAGS_MASK));
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "BSD Fixing '%s', newflags '%jx', flags '%jx'",
                file, (uintmax_t) (newflags & CHFLAGS_MASK),
                (uintmax_t) (dstat->st_flags & CHFLAGS_MASK));

        switch (attr.transaction.action)
        {
        case cfa_warn:

            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr,
                 "'%s' has flags %jo - [should be %jo]",
                 file, (uintmax_t) (dstat->st_mode & CHFLAGS_MASK),
                 (uintmax_t) (newflags & CHFLAGS_MASK));
            result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            break;

        case cfa_fix:

            if (!DONTDO)
            {
                if (chflags(file, newflags & CHFLAGS_MASK) == -1)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_DENIED, pp, attr,
                         "Failed setting BSD flags '%jx' on '%s'. (chflags: %s)",
                         (uintmax_t) newflags, file, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_DENIED);
                    break;
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr,
                         "'%s' had flags %jo, changed it to %jo", file,
                         (uintmax_t) (dstat->st_flags & CHFLAGS_MASK),
                         (uintmax_t) (newflags & CHFLAGS_MASK));
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }
            }

            break;

        default:
            ProgrammingError("Unhandled file action in switch: %d", attr.transaction.action);
        }
    }
# endif
#endif

    if (attr.acl.acl_entries)
    {
        result = PromiseResultUpdate(result, VerifyACL(ctx, file, attr, pp));
    }

    if (attr.touch)
    {
        if (utime(file, NULL) == -1)
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_DENIED, pp, attr, "Touching file '%s' failed. (utime: %s)",
                 file, GetErrorStr());
            result = PromiseResultUpdate(result, PROMISE_RESULT_DENIED);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Touching file '%s'", file);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
    }

#ifndef __MINGW32__
    umask(maskvalue);
#endif

    return result;
}

int DepthSearch(EvalContext *ctx, char *name, struct stat *sb, int rlevel, Attributes attr,
                const Promise *pp, dev_t rootdevice, PromiseResult *result)
{
    Dir *dirh;
    int goback;
    const struct dirent *dirp;
    struct stat lsb;
    Seq *db_file_set = NULL;
    Seq *selected_files = NULL;
    bool retval = true;

    if (!attr.havedepthsearch)  /* if the search is trivial, make sure that we are in the parent dir of the leaf */
    {
        char basedir[CF_BUFSIZE];

        Log(LOG_LEVEL_DEBUG, "Direct file reference '%s', no search implied", name);
        snprintf(basedir, sizeof(basedir), "%s", name);
        ChopLastNode(basedir);
        if (safe_chdir(basedir))
        {
            Log(LOG_LEVEL_ERR, "Failed to chdir into '%s'. (chdir: '%s')", basedir, GetErrorStr());
            return false;
        }
        if (!attr.haveselect || SelectLeaf(ctx, name, sb, attr.select))
        {
            VerifyFileLeaf(ctx, name, sb, attr, pp, result);
            return true;
        }
        else
        {
            return false;
        }
    }

    if (rlevel > CF_RECURSION_LIMIT)
    {
        Log(LOG_LEVEL_WARNING, "Very deep nesting of directories (>%d deep) for '%s' (Aborting files)", rlevel, name);
        return false;
    }

    if (!PushDirState(ctx, name, sb))
    {
        return false;
    }

    if ((dirh = DirOpen(".")) == NULL)
    {
        Log(LOG_LEVEL_INFO, "Could not open existing directory '%s'. (opendir: %s)", name, GetErrorStr());
        return false;
    }

    if (attr.havechange)
    {
        db_file_set = SeqNew(1, &free);
        if (!FileChangesGetDirectoryList(name, db_file_set))
        {
            SeqDestroy(db_file_set);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return false;
        }
        selected_files = SeqNew(1, &free);
    }

    char path[CF_BUFSIZE];

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (!ConsiderLocalFile(dirp->d_name, name))
        {
            continue;
        }

        memset(path, 0, sizeof(path));
        if (strlcpy(path, name, sizeof(path)-2) >= sizeof(path)-2)
        {
            Log(LOG_LEVEL_ERR, "Truncated filename %s while performing depth-first search", path);
            /* TODO return false? */
        }

        AddSlash(path);

        if (strlcat(path, dirp->d_name, sizeof(path)) >= sizeof(path))
        {
            Log(LOG_LEVEL_ERR, "Internal limit reached in DepthSearch(),"
                " path too long: '%s' + '%s'", path, dirp->d_name);
            retval = false;
            goto end;
        }

        if (lstat(dirp->d_name, &lsb) == -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Recurse was looking at '%s' when an error occurred. (lstat: %s)", path, GetErrorStr());
            continue;
        }

        if (S_ISLNK(lsb.st_mode))       /* should we ignore links? */
        {
            if (KillGhostLink(ctx, path, attr, pp, result))
            {
                continue;
            }
        }

        /* See if we are supposed to treat links to dirs as dirs and descend */

        if ((attr.recursion.travlinks) && (S_ISLNK(lsb.st_mode)))
        {
            if ((lsb.st_uid != 0) && (lsb.st_uid != getuid()))
            {
                Log(LOG_LEVEL_INFO,
                    "File '%s' is an untrusted link: cfengine will not follow it with a destructive operation", path);
                continue;
            }

            /* if so, hide the difference by replacing with actual object */

            if (stat(dirp->d_name, &lsb) == -1)
            {
                Log(LOG_LEVEL_ERR, "Recurse was working on '%s' when this failed. (stat: %s)", path, GetErrorStr());
                continue;
            }
        }

        if ((attr.recursion.xdev) && (DeviceBoundary(&lsb, rootdevice)))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping '%s' on different device - use xdev option to change this. (stat: %s)", path, GetErrorStr());
            continue;
        }

        if (S_ISDIR(lsb.st_mode))
        {
            if (SkipDirLinks(ctx, path, dirp->d_name, attr.recursion))
            {
                continue;
            }

            if ((attr.recursion.depth > 1) && (rlevel <= attr.recursion.depth))
            {
                Log(LOG_LEVEL_VERBOSE, "Entering '%s', level %d", path, rlevel);
                goback = DepthSearch(ctx, path, &lsb, rlevel + 1, attr, pp, rootdevice, result);
                if (!PopDirState(goback, name, sb, attr.recursion))
                {
                    FatalError(ctx, "Not safe to continue");
                }
            }
        }

        if (!attr.haveselect || SelectLeaf(ctx, path, &lsb, attr.select))
        {
            if (attr.havechange)
            {
                if (!SeqBinaryLookup(db_file_set, dirp->d_name, (SeqItemComparator)strcmp))
                {
                    // See comments in FileChangesCheckAndUpdateDirectory(),
                    // regarding this function call.
                    FileChangesLogNewFile(path, pp);
                }
                SeqAppend(selected_files, xstrdup(dirp->d_name));
            }

            VerifyFileLeaf(ctx, path, &lsb, attr, pp, result);
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "Skipping non-selected file '%s'", path);
        }
    }

    if (attr.havechange)
    {
        FileChangesCheckAndUpdateDirectory(name, selected_files, db_file_set,
                                           attr.change.update, pp, result);
    }

end:
    SeqDestroy(selected_files);
    SeqDestroy(db_file_set);
    DirClose(dirh);
    return retval;
}

static int PushDirState(EvalContext *ctx, char *name, struct stat *sb)
{
    if (safe_chdir(name) == -1)
    {
        Log(LOG_LEVEL_INFO, "Could not change to directory '%s', mode '%04jo' in tidy. (chdir: %s)",
            name, (uintmax_t)(sb->st_mode & 07777), GetErrorStr());
        return false;
    }

    if (!CheckLinkSecurity(sb, name))
    {
        FatalError(ctx, "Not safe to continue");
    }
    return true;
}

/**
 * @return true if safe for agent to continue
 */
static bool PopDirState(int goback, char *name, struct stat *sb, DirectoryRecursion r)
{
    if (goback && (r.travlinks))
    {
        if (safe_chdir(name) == -1)
        {
            Log(LOG_LEVEL_ERR, "Error in backing out of recursive travlink descent securely to '%s'. (chdir: %s)",
                name, GetErrorStr());
            return false;
        }

        if (!CheckLinkSecurity(sb, name))
        {
            return false;
        }
    }
    else if (goback)
    {
        if (safe_chdir("..") == -1)
        {
            Log(LOG_LEVEL_ERR, "Error in backing out of recursive descent securely to '%s'. (chdir: %s)",
                name, GetErrorStr());
            return false;
        }
    }

    return true;
}

/**
 * @return true if it is safe for the agent to continue execution
 */
static bool CheckLinkSecurity(struct stat *sb, char *name)
{
    struct stat security;

    Log(LOG_LEVEL_DEBUG, "Checking the inode and device to make sure we are where we think we are...");

    if (stat(".", &security) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not stat directory '%s' after entering. (stat: %s)",
            name, GetErrorStr());
        return true; // continue anyway
    }

    if ((sb->st_dev != security.st_dev) || (sb->st_ino != security.st_ino))
    {
        Log(LOG_LEVEL_ERR,
            "SERIOUS SECURITY ALERT: path race exploited in recursion to/from '%s'. Not safe for agent to continue - aborting",
              name);
        return false; // too dangerous
    }

    return true;
}

static PromiseResult VerifyCopiedFileAttributes(EvalContext *ctx, const char *src, const char *dest, struct stat *sstat,
                                                struct stat *dstat, Attributes attr, const Promise *pp)
{
#ifndef __MINGW32__
    mode_t newplus, newminus;
    uid_t save_uid;
    gid_t save_gid;

// If we get here, there is both a src and dest file

    save_uid = (attr.perms.owners)->uid;
    save_gid = (attr.perms.groups)->gid;

    if (attr.copy.preserve)
    {
        Log(LOG_LEVEL_VERBOSE, "Attempting to preserve file permissions from the source: %04jo",
              (uintmax_t)(sstat->st_mode & 07777));

        if ((attr.perms.owners)->uid == CF_SAME_OWNER)  /* Preserve uid and gid  */
        {
            (attr.perms.owners)->uid = sstat->st_uid;
        }

        if ((attr.perms.groups)->gid == CF_SAME_GROUP)
        {
            (attr.perms.groups)->gid = sstat->st_gid;
        }

// Will this preserve if no mode set?

        newplus = (sstat->st_mode & 07777);
        newminus = ~newplus & 07777;
        attr.perms.plus = newplus;
        attr.perms.minus = newminus;
    }
    else
    {
        if ((attr.perms.owners)->uid == CF_SAME_OWNER)  /* Preserve uid and gid  */
        {
            (attr.perms.owners)->uid = dstat->st_uid;
        }

        if ((attr.perms.groups)->gid == CF_SAME_GROUP)
        {
            (attr.perms.groups)->gid = dstat->st_gid;
        }

        if (attr.haveperms)
        {
            newplus = (dstat->st_mode & 07777) | attr.perms.plus;
            newminus = ~(newplus & ~(attr.perms.minus)) & 07777;
            attr.perms.plus = newplus;
            attr.perms.minus = newminus;
        }
    }
#endif
    PromiseResult result = VerifyFileAttributes(ctx, dest, dstat, attr, pp);

#ifndef __MINGW32__
    (attr.perms.owners)->uid = save_uid;
    (attr.perms.groups)->gid = save_gid;
#endif

    if (attr.copy.preserve && (attr.copy.servers == NULL
        || strcmp(RlistScalarValue(attr.copy.servers), "localhost") == 0))
    {
        if (!CopyFileExtendedAttributesDisk(src, dest))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Could not preserve extended attributes (ACLs and security contexts) on file '%s'", dest);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
    }

    return result;
}

static PromiseResult CopyFileSources(EvalContext *ctx, char *destination, Attributes attr, const Promise *pp, AgentConnection *conn)
{
    Buffer *source = BufferNew();
    // Expand this.promiser
    ExpandScalar(ctx,
                 PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name,
                 attr.copy.source, source);
    char vbuff[CF_BUFSIZE];
    struct stat ssb, dsb;
    struct timespec start;

    if (conn != NULL && (!conn->authenticated))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
             "No authenticated source '%s' in files.copy_from promise",
             BufferData(source));
        BufferDestroy(source);
        return PROMISE_RESULT_FAIL;
    }

    if (cf_stat(BufferData(source), &ssb, attr.copy, conn) == -1)
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, attr,
             "Can't stat file '%s' on '%s' in files.copy_from promise",
             BufferData(source), conn ? conn->remoteip : "localhost");
        BufferDestroy(source);
        return PROMISE_RESULT_FAIL;
    }

    start = BeginMeasure();

    strlcpy(vbuff, destination, CF_BUFSIZE - 3);

    if (S_ISDIR(ssb.st_mode))   /* could be depth_search */
    {
        AddSlash(vbuff);
        strcat(vbuff, ".");
    }

    if (!MakeParentDirectory(vbuff, attr.move_obstructions))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
             "Can't make directories for '%s' in files.copy_from promise",
             vbuff);
        BufferDestroy(source);
        return PROMISE_RESULT_FAIL;
    }

    CompressedArray *inode_cache = NULL;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (S_ISDIR(ssb.st_mode))   /* could be depth_search */
    {
        if (attr.copy.purge)
        {
            Log(LOG_LEVEL_VERBOSE, "Destination purging enabled");
        }

        Log(LOG_LEVEL_VERBOSE, "Entering directory '%s'", BufferData(source));

        result = PromiseResultUpdate(
            result, SourceSearchAndCopy(ctx, BufferData(source), destination,
                                        attr.recursion.depth, attr, pp,
                                        ssb.st_dev, &inode_cache, conn));

        if (stat(destination, &dsb) != -1)
        {
            if (attr.copy.check_root)
            {
                result = PromiseResultUpdate(
                    result, VerifyCopiedFileAttributes(ctx, BufferData(source),
                                                       destination, &ssb, &dsb,
                                                       attr, pp));
            }
        }
    }
    else
    {
        result = PromiseResultUpdate(
            result, VerifyCopy(ctx, BufferData(source), destination,
                               attr, pp, &inode_cache, conn));
    }

    DeleteCompressedArray(inode_cache);

    const char *mid = PromiseGetConstraintAsRval(pp, "measurement_class", RVAL_TYPE_SCALAR);
    if (mid)
    {
        char eventname[CF_BUFSIZE];
        snprintf(eventname, CF_BUFSIZE - 1, "Copy(%s:%s > %s)",
                 conn ? conn->this_server : "localhost",
                 BufferData(source), destination);

        EndMeasure(eventname, start);
    }
    else
    {
        EndMeasure(NULL, start);
    }

    BufferDestroy(source);
    return result;
}

/* Decide the protocol version the agent will use to connect: If the user has
 * specified a copy_from attribute then follow that one, else use the body
 * common control setting. */
static ProtocolVersion DecideProtocol(const EvalContext *ctx,
                                      ProtocolVersion copyfrom_setting)
{
    if (copyfrom_setting == CF_PROTOCOL_UNDEFINED)
    {
        /* TODO we would like to get the common control setting from
         * GenericAgentConfig. Given that we have only access to EvalContext here,
         * we get the raw string and reparse it every time. */
        const char *s = EvalContextVariableControlCommonGet(
            ctx, COMMON_CONTROL_PROTOCOL_VERSION);
        ProtocolVersion common_setting = ProtocolVersionParse(s);
        return common_setting;
    }
    else
    {
        return copyfrom_setting;
    }
}

static AgentConnection *FileCopyConnectionOpen(const EvalContext *ctx,
                                               const char *servername,
                                               FileCopy fc, bool background)
{
    ConnectionFlags flags = {
        .protocol_version = DecideProtocol(ctx, fc.protocol_version),
        .cache_connection = !background,
        .force_ipv4 = fc.force_ipv4,
        .trust_server = fc.trustkey
    };

    unsigned int conntimeout = fc.timeout;
    if (fc.timeout == CF_NOINT || fc.timeout < 0)
    {
        conntimeout = CONNTIMEOUT;
    }

    const char *port = (fc.port != NULL) ? fc.port : CFENGINE_PORT_STR;

    AgentConnection *conn = NULL;
    if (flags.cache_connection)
    {
        conn = ConnCache_FindIdleMarkBusy(servername, port, flags);

        if (conn != NULL)                 /* found idle connection in cache */
        {
            return conn;
        }
        else                    /* not found, open and cache new connection */
        {
            int err = 0;
            conn = ServerConnection(servername, port, conntimeout,
                                    flags, &err);

            /* WARNING: if cache already has non-idle connections to that
             * host, here we add more so that we connect in parallel. */

            if (conn == NULL)                           /* Couldn't connect */
            {
                /* Allocate and add to the cache as failure. */
                conn = NewAgentConn(servername, port, flags);
                conn->conn_info->status = CONNECTIONINFO_STATUS_NOT_ESTABLISHED;

                ConnCache_Add(conn, CONNCACHE_STATUS_OFFLINE);

                return NULL;
            }
            else
            {
                /* Success! Put it in the cache as busy. */
                ConnCache_Add(conn, CONNCACHE_STATUS_BUSY);
                return conn;
            }
        }
    }
    else
    {
        int err = 0;
        conn = ServerConnection(servername, port, conntimeout,
                                flags, &err);
        return conn;
    }
}

void FileCopyConnectionClose(AgentConnection *conn)
{
    if (conn->flags.cache_connection)
    {
        /* Mark the connection as available in the cache. */
        ConnCache_MarkNotBusy(conn);
    }
    else
    {
        DisconnectServer(conn);
    }
}

PromiseResult ScheduleCopyOperation(EvalContext *ctx, char *destination, Attributes attr, const Promise *pp)
{
    /* TODO currently parser allows body copy_from to have no source!
       See tests/acceptance/10_files/02_maintain/017.cf and
       https://cfengine.com/bugtracker/view.php?id=687 */
    /* assert(attr.copy.source != NULL); */
    if (attr.copy.source == NULL)
    {
        Log(LOG_LEVEL_INFO,
            "Body copy_from has no source! Maybe a typo in the policy?");
        return PROMISE_RESULT_FAIL;
    }

    Log(LOG_LEVEL_VERBOSE, "File '%s' copy_from '%s'",
        destination, attr.copy.source);

    /* Empty attr.copy.servers means copy from localhost. */
    bool copyfrom_localhost = (attr.copy.servers == NULL);
    AgentConnection *conn = NULL;
    Rlist *rp = attr.copy.servers;

    /* Iterate over all copy_from servers until connection succeeds. */
    while (rp != NULL && conn == NULL)
    {
        const char *servername = RlistScalarValue(rp);

        if (strcmp(servername, "localhost") == 0)
        {
            copyfrom_localhost = true;
            break;
        }

        conn = FileCopyConnectionOpen(ctx, servername, attr.copy,
                                      attr.transaction.background);
        if (conn == NULL)
        {
            Log(LOG_LEVEL_INFO, "Unable to establish connection to '%s'",
                servername);
        }

        rp = rp->next;
    }

    if (!copyfrom_localhost && conn == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
             "No suitable server found");
        PromiseRef(LOG_LEVEL_INFO, pp);
        return PROMISE_RESULT_FAIL;
    }

    /* (conn == NULL) means local copy. */
    PromiseResult result = CopyFileSources(ctx, destination, attr, pp, conn);

    if (conn != NULL)
    {
        FileCopyConnectionClose(conn);
    }

    return result;
}

PromiseResult ScheduleLinkOperation(EvalContext *ctx, char *destination, char *source, Attributes attr, const Promise *pp)
{
    const char *lastnode;

    lastnode = ReadLastNode(destination);
    PromiseResult result = PROMISE_RESULT_NOOP;

    if (MatchRlistItem(ctx, attr.link.copy_patterns, lastnode))
    {
        Log(LOG_LEVEL_VERBOSE, "Link '%s' matches copy_patterns", destination);
        CompressedArray *inode_cache = NULL;
        result = PromiseResultUpdate(result, VerifyCopy(ctx, attr.link.source, destination, attr, pp, &inode_cache, NULL));
        DeleteCompressedArray(inode_cache);
        return result;
    }

    switch (attr.link.link_type)
    {
    case FILE_LINK_TYPE_SYMLINK:
        result = VerifyLink(ctx, destination, source, attr, pp);
        break;
    case FILE_LINK_TYPE_HARDLINK:
        result = VerifyHardLink(ctx, destination, source, attr, pp);
        break;
    case FILE_LINK_TYPE_RELATIVE:
        result = VerifyRelativeLink(ctx, destination, source, attr, pp);
        break;
    case FILE_LINK_TYPE_ABSOLUTE:
        result = VerifyAbsoluteLink(ctx, destination, source, attr, pp);
        break;
    default:
        Log(LOG_LEVEL_ERR, "Unknown link type - should not happen.");
        break;
    }

    return result;
}

PromiseResult ScheduleLinkChildrenOperation(EvalContext *ctx, char *destination, char *source, int recurse, Attributes attr,
                                            const Promise *pp)
{
    Dir *dirh;
    const struct dirent *dirp;
    char promiserpath[CF_BUFSIZE], sourcepath[CF_BUFSIZE];
    struct stat lsb;
    int ret;

    if ((ret = lstat(destination, &lsb)) != -1)
    {
        if (attr.move_obstructions && S_ISLNK(lsb.st_mode))
        {
            unlink(destination);
        }
        else if (!S_ISDIR(lsb.st_mode))
        {
            Log(LOG_LEVEL_ERR, "Cannot promise to link multiple files to children of '%s' as it is not a directory!",
                  destination);
            return PROMISE_RESULT_NOOP;
        }
    }

    snprintf(promiserpath, sizeof(promiserpath), "%s/.", destination);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if ((ret == -1 || !S_ISDIR(lsb.st_mode)) && !CfCreateFile(ctx, promiserpath, pp, attr, &result))
    {
        Log(LOG_LEVEL_ERR, "Cannot promise to link multiple files to children of '%s' as it is not a directory!",
              destination);
        return result;
    }

    if ((dirh = DirOpen(source)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
             "Can't open source of children to link '%s'. (opendir: %s)",
             attr.link.source, GetErrorStr());
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        return result;
    }

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (!ConsiderLocalFile(dirp->d_name, source))
        {
            continue;
        }

        /* Assemble pathnames */

        strlcpy(promiserpath, destination, sizeof(promiserpath));
        AddSlash(promiserpath);

        if (strlcat(promiserpath, dirp->d_name, sizeof(promiserpath))
            >= sizeof(promiserpath))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, attr,
                 "Internal buffer limit while verifying child links,"
                 " promiser: '%s' + '%s'",
                 promiserpath, dirp->d_name);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            DirClose(dirh);
            return result;
        }

        strlcpy(sourcepath, source, sizeof(sourcepath));
        AddSlash(sourcepath);

        if (strlcat(sourcepath, dirp->d_name, sizeof(sourcepath))
            >= sizeof(sourcepath))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, attr,
                 "Internal buffer limit while verifying child links,"
                 " source filename: '%s' + '%s'",
                 sourcepath, dirp->d_name);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            DirClose(dirh);
            return result;
        }

        if ((lstat(promiserpath, &lsb) != -1) && !S_ISLNK(lsb.st_mode) && !S_ISDIR(lsb.st_mode))
        {
            if (attr.link.when_linking_children == cfa_override)
            {
                attr.move_obstructions = true;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Have promised not to disturb existing content belonging to '%s'", promiserpath);
                continue;
            }
        }

        if ((attr.recursion.depth > recurse) && (lstat(sourcepath, &lsb) != -1) && S_ISDIR(lsb.st_mode))
        {
            result = PromiseResultUpdate(result, ScheduleLinkChildrenOperation(ctx, promiserpath, sourcepath, recurse + 1, attr, pp));
        }
        else
        {
            result = PromiseResultUpdate(result, ScheduleLinkOperation(ctx, promiserpath, sourcepath, attr, pp));
        }
    }

    DirClose(dirh);
    return result;
}

static PromiseResult VerifyFileIntegrity(EvalContext *ctx, const char *file, Attributes attr, const Promise *pp)
{
    unsigned char digest1[EVP_MAX_MD_SIZE + 1];
    unsigned char digest2[EVP_MAX_MD_SIZE + 1];
    int changed = false, one, two;

    if ((attr.change.report_changes != FILE_CHANGE_REPORT_CONTENT_CHANGE) && (attr.change.report_changes != FILE_CHANGE_REPORT_ALL))
    {
        return PROMISE_RESULT_NOOP;
    }

    memset(digest1, 0, EVP_MAX_MD_SIZE + 1);
    memset(digest2, 0, EVP_MAX_MD_SIZE + 1);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (attr.change.hash == HASH_METHOD_BEST)
    {
        if (!DONTDO)
        {
            HashFile(file, digest1, HASH_METHOD_MD5);
            HashFile(file, digest2, HASH_METHOD_SHA1);

            one = FileChangesCheckAndUpdateHash(ctx, file, digest1, HASH_METHOD_MD5, attr, pp, &result);
            two = FileChangesCheckAndUpdateHash(ctx, file, digest2, HASH_METHOD_SHA1, attr, pp, &result);

            if (one || two)
            {
                changed = true;
            }
        }
    }
    else
    {
        if (!DONTDO)
        {
            HashFile(file, digest1, attr.change.hash);

            if (FileChangesCheckAndUpdateHash(ctx, file, digest1, attr.change.hash, attr, pp, &result))
            {
                changed = true;
            }
        }
    }

    if (changed)
    {
        EvalContextHeapPersistentSave(ctx, "checksum_alerts", CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
        EvalContextClassPutSoft(ctx, "checksum_alerts", CONTEXT_SCOPE_NAMESPACE, "");
        FileChangesLogChange(file, FILE_STATE_CONTENT_CHANGED, "Content changed", pp);
    }

    if (attr.change.report_diffs)
    {
        char destination[CF_BUFSIZE];
        if (!GetRepositoryPath(file, attr, destination))
        {
            destination[0] = '\0';
        }
        LogFileChange(ctx, file, changed, attr, pp, &CopyRegularFile, destination, &DeleteCompressedArray);
    }

    return result;
}

static int CompareForFileCopy(char *sourcefile, char *destfile, struct stat *ssb, struct stat *dsb, FileCopy fc, AgentConnection *conn)
{
    int ok_to_copy;

    switch (fc.compare)
    {
    case FILE_COMPARATOR_CHECKSUM:
    case FILE_COMPARATOR_HASH:

        if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
        {
            ok_to_copy = CompareFileHashes(sourcefile, destfile, ssb, dsb, fc, conn);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Checksum comparison replaced by ctime: files not regular");
            ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);
        }

        if (ok_to_copy)
        {
            Log(LOG_LEVEL_VERBOSE, "Image file '%s' has a wrong digest/checksum, should be copy of '%s'", destfile,
                  sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_BINARY:

        if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
        {
            ok_to_copy = CompareBinaryFiles(sourcefile, destfile, ssb, dsb, fc, conn);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Byte comparison replaced by ctime: files not regular");
            ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);
        }

        if (ok_to_copy)
        {
            Log(LOG_LEVEL_VERBOSE, "Image file %s has a wrong binary checksum, should be copy of '%s'", destfile,
                  sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_MTIME:

        ok_to_copy = (dsb->st_mtime < ssb->st_mtime);

        if (ok_to_copy)
        {
            Log(LOG_LEVEL_VERBOSE, "Image file '%s' out of date, should be copy of '%s'", destfile, sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_ATIME:

        ok_to_copy = (dsb->st_ctime < ssb->st_ctime) ||
            (dsb->st_mtime < ssb->st_mtime) || (CompareBinaryFiles(sourcefile, destfile, ssb, dsb, fc, conn));

        if (ok_to_copy)
        {
            Log(LOG_LEVEL_VERBOSE, "Image file '%s' seems out of date, should be copy of '%s'", destfile, sourcefile);
            return ok_to_copy;
        }
        break;

    default:
        ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);

        if (ok_to_copy)
        {
            Log(LOG_LEVEL_VERBOSE, "Image file '%s' out of date, should be copy of '%s'", destfile, sourcefile);
            return ok_to_copy;
        }
        break;
    }

    return false;
}

static void FileAutoDefine(EvalContext *ctx, char *destfile)
{
    char context[CF_MAXVARSIZE];

    snprintf(context, CF_MAXVARSIZE, "auto_%s", CanonifyName(destfile));
    EvalContextClassPutSoft(ctx, context, CONTEXT_SCOPE_NAMESPACE, "source=promise");
    Log(LOG_LEVEL_INFO, "Auto defining class '%s'", context);
}

#ifndef __MINGW32__
static void LoadSetxid(void)
{
    assert(!VSETXIDLIST);

    char filename[CF_BUFSIZE];
    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", GetLogDir(), VSYSNAME.nodename);
    MapName(filename);

    VSETXIDLIST = RawLoadItemList(filename);
}

static void SaveSetxid(bool modified)
{
    if (!VSETXIDLIST)
    {
        return;
    }

    if (modified)
    {
        char filename[CF_BUFSIZE];
        snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", GetLogDir(), VSYSNAME.nodename);
        MapName(filename);

        PurgeItemList(&VSETXIDLIST, "SETUID/SETGID");

        Item *current = RawLoadItemList(filename);
        if (!ListsCompare(VSETXIDLIST, current))
        {
            RawSaveItemList(VSETXIDLIST, filename, NewLineMode_Unix);
        }
    }

    DeleteItemList(VSETXIDLIST);
    VSETXIDLIST = NULL;
}

static bool IsInSetxidList(const char *file)
{
    if (!VSETXIDLIST)
    {
       LoadSetxid();
    }

    return IsItemIn(VSETXIDLIST, file);
}

static PromiseResult VerifySetUidGid(EvalContext *ctx, const char *file, struct stat *dstat, mode_t newperm,
                                     const Promise *pp, Attributes attr)
{
    int amroot = true;
    PromiseResult result = PROMISE_RESULT_NOOP;
    bool setxid_modified = false;


    if (!IsPrivileged())
    {
        amroot = false;
    }

    if ((dstat->st_uid == 0) && (dstat->st_mode & S_ISUID))
    {
        if (newperm & S_ISUID)
        {
            if (!IsInSetxidList(file))
            {
                if (amroot)
                {
                    cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "NEW SETUID root PROGRAM '%s'", file);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
                }

                PrependItem(&VSETXIDLIST, file, NULL);
                setxid_modified = true;
            }
        }
        else
        {
            switch (attr.transaction.action)
            {
            case cfa_fix:

                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Removing setuid (root) flag from '%s'", file);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                break;

            case cfa_warn:

                if (amroot)
                {
                    cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "Need to remove setuid (root) flag on '%s'", file);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
                }
                break;
            }
        }
    }

    if (dstat->st_uid == 0 && (dstat->st_mode & S_ISGID))
    {
        if (newperm & S_ISGID)
        {
            if (S_ISDIR(dstat->st_mode))
            {
                /* setgid directory */
            }
            else if (!IsInSetxidList(file))
            {
                if (amroot)
                {
                    cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "NEW SETGID root PROGRAM '%s'", file);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
                }

                PrependItem(&VSETXIDLIST, file, NULL);
                setxid_modified = true;
            }
        }
        else
        {
            switch (attr.transaction.action)
            {
            case cfa_fix:

                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Removing setgid (root) flag from '%s'", file);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                break;

            case cfa_warn:

                cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "Need to remove setgid (root) flag on '%s'", file);
                result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
                break;

            default:
                break;
            }
        }
    }

    SaveSetxid(setxid_modified);

    return result;
}
#endif

#ifdef __APPLE__

static int VerifyFinderType(EvalContext *ctx, const char *file, Attributes a, const Promise *pp, PromiseResult *result)
{                               /* Code modeled after hfstar's extract.c */
    typedef struct
    {
        long fdType;
        long fdCreator;
        short fdFlags;
        short fdLocationV;
        short fdLocationH;
        short fdFldr;
        short fdIconID;
        short fdUnused[3];
        char fdScript;
        char fdXFlags;
        short fdComment;
        long fdPutAway;
    }
    FInfo;

    struct attrlist attrs;
    struct
    {
        long ssize;
        struct timespec created;
        struct timespec modified;
        struct timespec changed;
        struct timespec backup;
        FInfo fi;
    }
    fndrInfo;
    int retval;

    if (a.perms.findertype == NULL)
    {
        return 0;
    }

    Log(LOG_LEVEL_DEBUG, "VerifyFinderType of '%s' for '%s'", file, a.perms.findertype);

    if (strncmp(a.perms.findertype, "*", CF_BUFSIZE) == 0 || strncmp(a.perms.findertype, "", CF_BUFSIZE) == 0)
    {
        return 0;
    }

    attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrs.reserved = 0;
    attrs.commonattr = ATTR_CMN_CRTIME | ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME | ATTR_CMN_BKUPTIME | ATTR_CMN_FNDRINFO;
    attrs.volattr = 0;
    attrs.dirattr = 0;
    attrs.fileattr = 0;
    attrs.forkattr = 0;

    memset(&fndrInfo, 0, sizeof(fndrInfo));

    getattrlist(file, &attrs, &fndrInfo, sizeof(fndrInfo), 0);

    if (fndrInfo.fi.fdType != *(long *) a.perms.findertype)
    {
        fndrInfo.fi.fdType = *(long *) a.perms.findertype;

        switch (a.transaction.action)
        {
        case cfa_fix:

            if (DONTDO)
            {
                Log(LOG_LEVEL_INFO, "Promised to set Finder Type code of '%s' to '%s'", file, a.perms.findertype);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                return 0;
            }

            /* setattrlist does not take back in the long ssize */
            retval = setattrlist(file, &attrs, &fndrInfo.created, 4 * sizeof(struct timespec) + sizeof(FInfo), 0);

            Log(LOG_LEVEL_DEBUG, "CheckFinderType setattrlist returned '%d'", retval);

            if (retval >= 0)
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Setting Finder Type code of '%s' to '%s'", file, a.perms.findertype);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Setting Finder Type code of '%s' to '%s' failed", file,
                     a.perms.findertype);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            }

            return retval;

        case cfa_warn:
            Log(LOG_LEVEL_WARNING, "Darwin FinderType does not match -- not fixing.");
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
            return 0;

        default:
            return 0;
        }
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Finder Type code of '%s' to '%s' is as promised", file, a.perms.findertype);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_NOOP);
        return 0;
    }
}

#endif

static void TruncateFile(char *name)
{
    struct stat statbuf;
    int fd;

    if (stat(name, &statbuf) == -1)
    {
        Log(LOG_LEVEL_DEBUG, "Didn't find '%s' to truncate", name);
    }
    else
    {
        if ((fd = safe_creat(name, 000)) == -1)      /* dummy mode ignored */
        {
            Log(LOG_LEVEL_ERR, "Failed to create or truncate file '%s'. (creat: %s)", name, GetErrorStr());
        }
        else
        {
            close(fd);
        }
    }
}

static void RegisterAHardLink(int i, char *value, Attributes attr, CompressedArray **inode_cache)
{
    if (!FixCompressedArrayValue(i, value, inode_cache))
    {
        /* Not root hard link, remove to preserve consistency */
        if (DONTDO)
        {
            Log(LOG_LEVEL_VERBOSE, "Need to remove old hard link '%s' to preserve structure", value);
        }
        else
        {
            if (attr.transaction.action == cfa_warn)
            {
                Log(LOG_LEVEL_WARNING, "Need to remove old hard link '%s' to preserve structure", value);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Removing old hard link '%s' to preserve structure", value);
                unlink(value);
            }
        }
    }
}

static int cf_stat(const char *file, struct stat *buf, FileCopy fc, AgentConnection *conn)
{
    if (!file)
    {
        return -1;
    }

    if (conn == NULL)
    {
        return stat(file, buf);
    }
    else
    {
        assert(fc.servers != NULL &&
               strcmp(RlistScalarValue(fc.servers), "localhost") != 0);

        return cf_remote_stat(conn, fc.encrypt, file, buf, "file");
    }
}

#ifndef __MINGW32__

static int cf_readlink(EvalContext *ctx, char *sourcefile, char *linkbuf, int buffsize,
                       Attributes attr, const Promise *pp, AgentConnection *conn, PromiseResult *result)
 /* wrapper for network access */
{
    memset(linkbuf, 0, buffsize);

    if (conn == NULL)
    {
        return readlink(sourcefile, linkbuf, buffsize - 1);
    }
    assert(attr.copy.servers &&
           strcmp(RlistScalarValue(attr.copy.servers), "localhost"));

    const Stat *sp = StatCacheLookup(conn, sourcefile,
                                     RlistScalarValue(attr.copy.servers));

    if (sp)
    {
        if (sp->cf_readlink != NULL)
        {
            if (strlen(sp->cf_readlink) + 1 > buffsize)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "readlink value is too large in cfreadlink");
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                Log(LOG_LEVEL_ERR, "Contained '%s'", sp->cf_readlink);
                return -1;
            }
            else
            {
                memset(linkbuf, 0, buffsize);
                strcpy(linkbuf, sp->cf_readlink);
                return 0;
            }
        }
    }

    return -1;
}

#endif /* !__MINGW32__ */

static int SkipDirLinks(EvalContext *ctx, char *path, const char *lastnode, DirectoryRecursion r)
{
    if (r.exclude_dirs)
    {
        if ((MatchRlistItem(ctx, r.exclude_dirs, path)) || (MatchRlistItem(ctx, r.exclude_dirs, lastnode)))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping matched excluded directory '%s'", path);
            return true;
        }
    }

    if (r.include_dirs)
    {
        if (!((MatchRlistItem(ctx, r.include_dirs, path)) || (MatchRlistItem(ctx, r.include_dirs, lastnode))))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping matched non-included directory '%s'", path);
            return true;
        }
    }

    return false;
}

#ifndef __MINGW32__

bool VerifyOwner(EvalContext *ctx, const char *file, const Promise *pp, Attributes attr, struct stat *sb, PromiseResult *result)
{
    struct passwd *pw;
    struct group *gp;
    UidList *ulp;
    GidList *glp;

    /* The groups to change ownership to, using lchown(uid,gid). */
    uid_t uid = CF_UNKNOWN_OWNER;                       /* just init values */
    gid_t gid = CF_UNKNOWN_GROUP;

    /* SKIP if file is already owned by anyone of the promised owners. */
    for (ulp = attr.perms.owners; ulp != NULL; ulp = ulp->next)
    {
        if (ulp->uid == CF_SAME_OWNER || sb->st_uid == ulp->uid)        /* "same" matches anything */
        {
            uid = CF_SAME_OWNER;      /* chown(-1) doesn't change ownership */
            break;
        }
    }

    if (uid != CF_SAME_OWNER)
    {
        /* Change ownership to the first known user in the promised list. */
        for (ulp = attr.perms.owners; ulp != NULL; ulp = ulp->next)
        {
            if (ulp->uid != CF_UNKNOWN_OWNER)
            {
                uid = ulp->uid;
                break;
            }
        }
        if (ulp == NULL)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                 "None of the promised owners for '%s' exist -- see INFO logs for more",
                 file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            uid = CF_SAME_OWNER;      /* chown(-1) doesn't change ownership */
        }
    }
    assert(uid != CF_UNKNOWN_OWNER);

    /* SKIP if file is already group owned by anyone of the promised groups. */
    for (glp = attr.perms.groups; glp != NULL; glp = glp->next)
    {
        if (glp->gid == CF_SAME_GROUP || sb->st_gid == glp->gid)
        {
            gid = CF_SAME_GROUP;      /* chown(-1) doesn't change ownership */
            break;
        }
    }

    /* Change group ownership to the first known group in the promised list. */
    if (gid != CF_SAME_GROUP)
    {
        for (glp = attr.perms.groups; glp != NULL; glp = glp->next)
        {
            if (glp->gid != CF_UNKNOWN_GROUP)
            {
                gid = glp->gid;
                break;
            }
        }
        if (glp == NULL)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
                 "None of the promised groups for '%s' exist -- see INFO logs for more",
                 file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            gid = CF_SAME_GROUP;      /* chown(-1) doesn't change ownership */
        }
    }
    assert(gid != CF_UNKNOWN_GROUP);

    if (uid == CF_SAME_OWNER &&
        gid == CF_SAME_GROUP)
    {
        /* User and group as promised, skip completely. */
        return false;
    }
    else
    {
        switch (attr.transaction.action)
        {
        case cfa_fix:

            if (uid != CF_SAME_OWNER)
            {
                Log(LOG_LEVEL_DEBUG, "Change owner to uid '%ju' if possible",
                    (uintmax_t) uid);
            }

            if (gid != CF_SAME_GROUP)
            {
                Log(LOG_LEVEL_DEBUG, "Change group to gid '%ju' if possible",
                    (uintmax_t) gid);
            }

            if (!DONTDO && S_ISLNK(sb->st_mode))
            {
# ifdef HAVE_LCHOWN
                Log(LOG_LEVEL_DEBUG, "Using lchown function");
                if (safe_lchown(file, uid, gid) == -1)
                {
                    Log(LOG_LEVEL_INFO, "Cannot set ownership on link '%s'. (lchown: %s)", file, GetErrorStr());
                }
                else
                {
                    return true;
                }
# endif
            }
            else if (!DONTDO)
            {
                if (uid != CF_SAME_OWNER)
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr,
                         "Owner of '%s' was %ju, setting to %ju",
                         file, (uintmax_t) sb->st_uid, (uintmax_t) uid);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                }

                if (gid != CF_SAME_GROUP)
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr,
                         "Group of '%s' was %ju, setting to %ju",
                         file, (uintmax_t)sb->st_gid, (uintmax_t)gid);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                }

                if (!S_ISLNK(sb->st_mode))
                {
                    if (safe_chown(file, uid, gid) == -1)
                    {
                        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_DENIED, pp, attr, "Cannot set ownership on file '%s'. (chown: %s)",
                             file, GetErrorStr());
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_DENIED);
                    }
                    else
                    {
                        return true;
                    }
                }
            }
            break;

        case cfa_warn:

            if ((pw = getpwuid(sb->st_uid)) == NULL)
            {
                Log(LOG_LEVEL_WARNING, "File '%s' is not owned by anybody in the passwd database", file);
                Log(LOG_LEVEL_WARNING, "(uid = %ju,gid = %ju)", (uintmax_t)sb->st_uid, (uintmax_t)sb->st_gid);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                break;
            }

            if ((gp = getgrgid(sb->st_gid)) == NULL)
            {
                cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "File '%s' is not owned by any group in group database",
                     file);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                break;
            }

            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "File '%s' is owned by '%s', group '%s'", file, pw->pw_name,
                 gp->gr_name);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
            break;
        }
    }

    return false;
}

#endif /* !__MINGW32__ */

static void VerifyFileChanges(const char *file, struct stat *sb, Attributes attr, const Promise *pp)
{
    if ((attr.change.report_changes != FILE_CHANGE_REPORT_STATS_CHANGE) && (attr.change.report_changes != FILE_CHANGE_REPORT_ALL))
    {
        return;
    }

    FileChangesCheckAndUpdateStats(file, sb, attr.change.update, pp);
}

bool CfCreateFile(EvalContext *ctx, char *file, const Promise *pp, Attributes attr, PromiseResult *result)
{
    if (!IsAbsoluteFileName(file))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr,
             "Cannot create a relative filename '%s' - has no invariant meaning. (creat: %s)", file, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

    /* If name ends in /., or depthsearch is set, then this is a directory */
    bool is_dir = attr.havedepthsearch || (strcmp(".", ReadLastNode(file)) == 0);
    if (is_dir)
    {
        Log(LOG_LEVEL_DEBUG, "File object '%s' seems to be a directory", file);

        if (!DONTDO && attr.transaction.action != cfa_warn)
        {
            if (!MakeParentDirectory(file, attr.move_obstructions))
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Error creating directories for '%s'. (creat: %s)",
                     file, GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                return false;
            }

            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Created directory '%s'", file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "Warning promised, need to create directory '%s'", file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
            return false;
        }
    }
    else if (attr.file_type && !strncmp(attr.file_type, "fifo", 5))
    {
#ifndef _WIN32
        if (!DONTDO)
        {
            mode_t saveumask = umask(0);
            mode_t filemode = 0600;

            if (PromiseGetConstraintAsRval(pp, "mode", RVAL_TYPE_SCALAR) == NULL)
            {
                /* Relying on umask is risky */
                filemode = 0600;
                Log(LOG_LEVEL_VERBOSE, "No mode was set, choose plain file default %04jo", (uintmax_t)filemode);
            }
            else
            {
                filemode = attr.perms.plus & ~(attr.perms.minus);
            }

            MakeParentDirectory(file, attr.move_obstructions);

            char errormsg[CF_BUFSIZE];
            if (!mkfifo(file, filemode))
            {
                snprintf(errormsg, sizeof(errormsg), "(mkfifo: %s)", GetErrorStr());
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Error creating file '%s', mode '%04jo'. %s",
                     file, (uintmax_t)filemode, errormsg);
                umask(saveumask);
                return false;
            }

            umask(saveumask);
            return true;
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, attr, "Warning promised, need to create fifo '%s'", file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
            return false;
        }
#else
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "Operation not supported on Windows");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return false;
#endif
    }
    else
    {
        if (!DONTDO && attr.transaction.action != cfa_warn)
        {
            mode_t saveumask = umask(0);
            mode_t filemode = 0600;     /* Decide the mode for filecreation */

            if (PromiseGetConstraintAsRval(pp, "mode", RVAL_TYPE_SCALAR) == NULL)
            {
                /* Relying on umask is risky */
                filemode = 0600;
                Log(LOG_LEVEL_VERBOSE, "No mode was set, choose plain file default %04jo", (uintmax_t)filemode);
            }
            else
            {
                filemode = attr.perms.plus & ~(attr.perms.minus);
            }

            MakeParentDirectory(file, attr.move_obstructions);

            int fd = safe_open(file, O_WRONLY | O_CREAT | O_EXCL, filemode);
            if (fd == -1)
            {
                char errormsg[CF_BUFSIZE];
                if (errno == EEXIST)
                {
                    snprintf(errormsg, sizeof(errormsg), "(open: '%s'). "
                             "Most likely a dangling symlink is in the way. "
                             "Refusing to create the target file of dangling symlink (security risk).",
                             GetErrorStr());
                }
                else
                {
                    snprintf(errormsg, sizeof(errormsg), "(open: %s)", GetErrorStr());
                }
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Error creating file '%s', mode '%04jo'. %s",
                     file, (uintmax_t)filemode, errormsg);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                umask(saveumask);
                return false;
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, attr, "Created file '%s', mode %04jo", file, (uintmax_t)filemode);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                close(fd);
                umask(saveumask);
                return true;
            }
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, attr, "Warning promised, need to create file '%s'", file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
            return false;
        }
    }

    return true;
}

static int DeviceBoundary(struct stat *sb, dev_t rootdevice)
{
    if (sb->st_dev == rootdevice)
    {
        return false;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Device change from %jd to %jd", (intmax_t) rootdevice, (intmax_t) sb->st_dev);
        return true;
    }
}
