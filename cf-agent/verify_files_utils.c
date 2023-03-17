/*
  Copyright 2023 Northern.tech AS

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
#include <hash.h>
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
#include <changes_chroot.h>     /* PrepareChangesChroot(), RecordFileChangedInChroot() */
#include <unix.h>               /* GetGroupName(), GetUserName() */

#include <cf-windows-functions.h>

#define CF_RECURSION_LIMIT 100

static const Rlist *AUTO_DEFINE_LIST = NULL; /* GLOBAL_P */

static Item *VSETXIDLIST = NULL;

const Rlist *SINGLE_COPY_LIST = NULL; /* GLOBAL_P */
StringSet *SINGLE_COPY_CACHE = NULL; /* GLOBAL_X */

static bool TransformFile(EvalContext *ctx, char *file, const Attributes *attr, const Promise *pp, PromiseResult *result);
static PromiseResult VerifyName(EvalContext *ctx, char *path, const struct stat *sb, const Attributes *attr, const Promise *pp);
static PromiseResult VerifyDelete(EvalContext *ctx,
                                  const char *path, const struct stat *sb,
                                  const Attributes *attr, const Promise *pp);
static PromiseResult VerifyCopy(EvalContext *ctx, const char *source, char *destination, const Attributes *attr, const Promise *pp,
                                CompressedArray **inode_cache, AgentConnection *conn);
static PromiseResult TouchFile(EvalContext *ctx, char *path, const Attributes *attr, const Promise *pp);
static PromiseResult VerifyFileAttributes(EvalContext *ctx, const char *file, const struct stat *dstat, const Attributes *attr, const Promise *pp);
static bool PushDirState(EvalContext *ctx, const Promise *pp, const Attributes *attr, char *name, const struct stat *sb, PromiseResult *result);
static bool PopDirState(EvalContext *ctx, const Promise *pp, const Attributes *attr, int goback, char *name, const struct stat *sb,
                        DirectoryRecursion r, PromiseResult *result);
static bool CheckLinkSecurity(const struct stat *sb, const char *name);
static bool CompareForFileCopy(char *sourcefile, char *destfile, const struct stat *ssb, const struct stat *dsb, const FileCopy *fc, AgentConnection *conn);
static void FileAutoDefine(EvalContext *ctx, char *destfile);
static void TruncateFile(const char *name);
static void RegisterAHardLink(int i, const char *value, EvalContext *ctx, const Promise *pp,
                              const Attributes *attr, PromiseResult *result,
                              CompressedArray **inode_cache);
static PromiseResult VerifyCopiedFileAttributes(EvalContext *ctx, const char *src, const char *dest, const struct stat *sstat, const struct stat *dstat, const Attributes *attr, const Promise *pp);
static int cf_stat(const char *file, struct stat *buf, const FileCopy *fc, AgentConnection *conn);
#ifndef __MINGW32__
static int cf_readlink(EvalContext *ctx, const char *sourcefile, char *linkbuf, size_t buffsize, const Attributes *attr, const Promise *pp, AgentConnection *conn, PromiseResult *result);
#endif
static bool SkipDirLinks(EvalContext *ctx, const char *path, const char *lastnode, DirectoryRecursion r);
static bool DeviceBoundary(const struct stat *sb, dev_t rootdevice);
static PromiseResult LinkCopy(EvalContext *ctx, char *sourcefile, char *destfile, const struct stat *sb, const Attributes *attr,
                              const Promise *pp, CompressedArray **inode_cache, AgentConnection *conn);

#ifndef __MINGW32__
static void LoadSetxid(void);
static void SaveSetxid(bool modified);
static PromiseResult VerifySetUidGid(EvalContext *ctx, const char *file, const struct stat *dstat, mode_t newperm, const Promise *pp, const Attributes *attr);
#endif
#ifdef __APPLE__
static int VerifyFinderType(EvalContext *ctx, const char *file, const Attributes *a, const Promise *pp, PromiseResult *result);
#endif
static void VerifyFileChanges(EvalContext *ctx, const char *file, const struct stat *sb,
                              const Attributes *attr, const Promise *pp, PromiseResult *result);
static PromiseResult VerifyFileIntegrity(EvalContext *ctx, const char *file, const Attributes *attr, const Promise *pp);

extern Attributes GetExpandedAttributes(EvalContext *ctx, const Promise *pp, const Attributes *attr);
extern void ClearExpandedAttributes(Attributes *a);

void SetFileAutoDefineList(const Rlist *auto_define_list)
{
    AUTO_DEFINE_LIST = auto_define_list;
}

void VerifyFileLeaf(EvalContext *ctx, char *path, const struct stat *sb, ARG_UNUSED const Attributes *attr, const Promise *pp, PromiseResult *result)
{
    // FIXME: This function completely ignores it's attr argument
    assert(attr != NULL);

/* Here we can assume that we are in the parent directory of the leaf */

    Log(LOG_LEVEL_VERBOSE, "Handling file existence constraints on '%s'", path);

    /* Update this.promiser again, and overwrite common attributes (classes, action) accordingly */

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser", path, CF_DATA_TYPE_STRING, "source=promise");        // Parameters may only be scalars
    Attributes org_attr = GetFilesAttributes(ctx, pp);
    Attributes new_attr = GetExpandedAttributes(ctx, pp, &org_attr);

    if (new_attr.transformer != NULL)
    {
        if (!TransformFile(ctx, path, &new_attr, pp, result))
        {
            /* NOP, changes and/or failures are recorded by TransformFile() */
        }
    }
    else
    {
        if (new_attr.haverename)
        {
            *result = PromiseResultUpdate(*result, VerifyName(ctx, path, sb, &new_attr, pp));
        }

        if (new_attr.havedelete)
        {
            *result = PromiseResultUpdate(*result, VerifyDelete(ctx, path, sb, &new_attr, pp));
        }

        if (new_attr.touch)
        {
            *result = PromiseResultUpdate(*result, TouchFile(ctx, path, &new_attr, pp)); // intrinsically non-convergent op
        }
    }

    if (new_attr.haveperms || new_attr.havechange || new_attr.acl.acl_entries)
    {
        if (S_ISDIR(sb->st_mode) && new_attr.recursion.depth && !new_attr.recursion.include_basedir &&
            (strcmp(path, pp->promiser) == 0))
        {
            Log(LOG_LEVEL_VERBOSE, "Promise to skip base directory '%s'", path);
        }
        else
        {
            *result = PromiseResultUpdate(*result, VerifyFileAttributes(ctx, path, sb, &new_attr, pp));
        }
    }
    ClearExpandedAttributes(&new_attr);
}

/* Checks whether item matches a list of wildcards */
static bool MatchRlistItem(EvalContext *ctx, const Rlist *listofregex, const char *teststring)
{
    for (const Rlist *rp = listofregex; rp != NULL; rp = rp->next)
    {
        /* Avoid using regex if possible, due to memory leak */

        if (strcmp(teststring, RlistScalarValue(rp)) == 0)
        {
            return true;
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
                                char *destfile, const struct stat *ssb,
                                const Attributes *a, const Promise *pp,
                                CompressedArray **inode_cache,
                                AgentConnection *conn)
{
    assert(a != NULL);
    const char *lastnode;
    struct stat dsb;
    const mode_t srcmode = ssb->st_mode;

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
    if (a->copy.copy_links != NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "copy_from.copylink_patterns is ignored on Windows "
            "(source files cannot be symbolic links)");
    }
#endif /* __MINGW32__ */

    Attributes attr = *a; // TODO: Avoid this copy
    attr.link.when_no_file = cfa_force;

    if (strcmp(sourcefile, destfile) == 0 &&
        strcmp(server, "localhost") == 0)
    {
        RecordNoChange(ctx, pp, &attr,
                       "File copy promise loop: file/dir '%s' is its own source",
                       sourcefile);
        return PROMISE_RESULT_NOOP;
    }

    if (attr.haveselect && !SelectLeaf(ctx, sourcefile, ssb, &(attr.select)))
    {
        RecordNoChange(ctx, pp, &attr, "Skipping non-selected file '%s'", sourcefile);
        return PROMISE_RESULT_NOOP;
    }

    if ((SINGLE_COPY_CACHE != NULL) && StringSetContains(SINGLE_COPY_CACHE, destfile))
    {
        RecordNoChange(ctx, pp, &attr, "Skipping single-copied file '%s'", destfile);
        return PROMISE_RESULT_NOOP;
    }

    if (attr.copy.link_type != FILE_LINK_TYPE_NONE)
    {
        lastnode = ReadLastNode(sourcefile);

        if (MatchRlistItem(ctx, attr.copy.link_instead, lastnode))
        {
            if (MatchRlistItem(ctx, attr.copy.copy_links, lastnode))
            {
                RecordNoChange(ctx, pp, &attr,
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
                return LinkCopy(ctx, sourcefile, destfile, ssb,
                                &attr, pp, inode_cache, conn);
#endif
            }
        }
    }

    const char *changes_destfile = destfile;
    if (ChrootChanges())
    {
        changes_destfile = ToChangesChroot(destfile);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    bool destfile_exists = (lstat(changes_destfile, &dsb) != -1);
    if (destfile_exists)
    {
        if ((S_ISLNK(dsb.st_mode) && attr.copy.link_type == FILE_LINK_TYPE_NONE)
            || (S_ISLNK(dsb.st_mode) && !S_ISLNK(srcmode)))
        {
            if (!S_ISLNK(srcmode) &&
                attr.copy.type_check &&
                attr.copy.link_type != FILE_LINK_TYPE_NONE)
            {
                RecordFailure(ctx, pp, &attr,
                              "File image exists but destination type is silly "
                              "(file/dir/link doesn't match)");
                PromiseRef(LOG_LEVEL_ERR, pp);
                return PROMISE_RESULT_FAIL;
            }

            if (!MakingChanges(ctx, pp, &attr, NULL,
                              "remove old symbolic link '%s' to make way for copy",
                               destfile))
            {
                return PROMISE_RESULT_WARN;
            }
            else
            {
                if (unlink(changes_destfile) == -1)
                {
                    RecordFailure(ctx, pp, &attr,
                                  "Couldn't remove link '%s'. (unlink: %s)",
                                  destfile, GetErrorStr());
                    return PROMISE_RESULT_FAIL;
                }
                RecordChange(ctx, pp, &attr,
                             "Removing old symbolic link '%s' to make way for copy",
                             destfile);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                destfile_exists = false;
            }
        }
    }
    else
    {
        bool dir_created = false;
        if (!MakeParentDirectoryForPromise(ctx, pp, &attr, &result, destfile,
                                           true, &dir_created, DEFAULTMODE))
        {
            return result;
        }
        if (dir_created)
        {
            RecordChange(ctx, pp, &attr, "Created parent directory for '%s'", destfile);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
    }

    if (attr.copy.min_size != (size_t) CF_NOINT)
    {
        if (((size_t) ssb->st_size < attr.copy.min_size)
            || ((size_t) ssb->st_size > attr.copy.max_size))
        {
            RecordNoChange(ctx, pp, &attr,
                           "Source file '%s' size is not in the permitted safety range, skipping",
                           sourcefile);
            return result;
        }
    }

    if (!destfile_exists)
    {
        if (S_ISREG(srcmode) ||
            (S_ISLNK(srcmode) && attr.copy.link_type == FILE_LINK_TYPE_NONE))
        {
            if (!MakingChanges(ctx, pp, &attr, &result, "copy '%s' to '%s'", destfile, sourcefile))
            {
                return result;
            }

            Log(LOG_LEVEL_VERBOSE,
                "'%s' wasn't at destination, copying from '%s:%s'",
                destfile, server, sourcefile);

            if (S_ISLNK(srcmode) && attr.copy.link_type != FILE_LINK_TYPE_NONE)
            {
                Log(LOG_LEVEL_VERBOSE, "'%s' is a symbolic link", sourcefile);
                result = PromiseResultUpdate(result,
                                             LinkCopy(ctx, sourcefile, destfile, ssb,
                                                      &attr, pp, inode_cache, conn));
            }
            else if (CopyRegularFile(ctx, sourcefile, destfile, ssb, &attr,
                                     pp, inode_cache, conn, &result))
            {
                if (stat(ToChangesPath(destfile), &dsb) == -1)
                {
                    Log(LOG_LEVEL_ERR,
                        "Can't stat destination file '%s'. (stat: %s)",
                        destfile, GetErrorStr());
                }
                else
                {
                    result = PromiseResultUpdate(result,
                                                 VerifyCopiedFileAttributes(ctx, sourcefile,
                                                                            destfile, ssb,
                                                                            &dsb, &attr, pp));
                }

                RecordChange(ctx, pp, &attr, "Updated file '%s' from '%s:%s'",
                             destfile, server, sourcefile);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);

                if (SINGLE_COPY_LIST)
                {
                    StringSetAdd(SINGLE_COPY_CACHE, xstrdup(destfile));
                }

                if (MatchRlistItem(ctx, AUTO_DEFINE_LIST, destfile))
                {
                    FileAutoDefine(ctx, destfile);
                }
            }
            else
            {
                RecordFailure(ctx, pp, &attr, "Copy from '%s:%s' failed",
                              server, sourcefile);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            }

            return result;
        }

        if (S_ISFIFO(srcmode))
        {
#ifdef HAVE_MKFIFO
            if (!MakingChanges(ctx, pp, &attr, &result, "create FIFO '%s'", destfile))
            {
                return result;
            }
            else if (mkfifo(ToChangesPath(destfile), srcmode) != 0)
            {
                RecordFailure(ctx, pp, &attr, "Cannot create fifo '%s'. (mkfifo: %s)",
                              destfile, GetErrorStr());
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                return result;
            }

            RecordChange(ctx, pp, &attr, "Created fifo '%s'", destfile);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
#else
            RecordWarning(ctx, pp, &attr, "Should create FIFO '%s', but FIFO creation not supported",
                          destfile);
            result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            return result;
#endif
        }
        else
        {
#ifndef __MINGW32__                   // only regular files on windows
            if (S_ISBLK(srcmode) || S_ISCHR(srcmode) || S_ISSOCK(srcmode))
            {
                if (!MakingChanges(ctx, pp, &attr, &result, "make special file/device '%s'", destfile))
                {
                    return result;
                }
                else if (mknod(ToChangesPath(destfile),
                               srcmode, ssb->st_rdev))
                {
                    RecordFailure(ctx, pp, &attr, "Cannot create special file '%s'. (mknod: %s)",
                                  destfile, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    return result;
                }
                RecordChange(ctx, pp, &attr, "Created special file/device '%s'.", destfile);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
#endif /* !__MINGW32__ */
        }

        if ((S_ISLNK(srcmode)) && (attr.copy.link_type != FILE_LINK_TYPE_NONE))
        {
            result = PromiseResultUpdate(result,
                                         LinkCopy(ctx, sourcefile, destfile, ssb,
                                                  &attr, pp, inode_cache, conn));
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Destination file '%s' already exists",
            destfile);

        if (attr.copy.compare == FILE_COMPARATOR_EXISTS)
        {
            RecordNoChange(ctx, pp, &attr, "Existence only is promised, no copying required");
            return result;
        }

        bool should_copy = (attr.copy.force_update ||
                            CompareForFileCopy(sourcefile, destfile, ssb,
                                               &dsb, &attr.copy, conn));

        if (attr.copy.type_check &&
            attr.copy.link_type != FILE_LINK_TYPE_NONE)
        {
            // Mask mode with S_IFMT to extract and compare file types
            if ((dsb.st_mode & S_IFMT) != (srcmode & S_IFMT))
            {
                RecordFailure(ctx, pp, &attr,
                              "Promised file copy %s exists but type mismatch with source '%s'",
                              destfile, sourcefile);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                return result;
            }
        }

        if (should_copy || S_ISLNK(srcmode))                     /* Always check links */
        {
            if (S_ISREG(srcmode) ||
                attr.copy.link_type == FILE_LINK_TYPE_NONE)
            {
                if (!MakingChanges(ctx, pp, &attr, &result, "update file '%s' from '%s' on '%s'",
                                   destfile, sourcefile, server))
                {
                    return result;
                }

                if (MatchRlistItem(ctx, AUTO_DEFINE_LIST, destfile))
                {
                    FileAutoDefine(ctx, destfile);
                }

                if (CopyRegularFile(ctx, sourcefile, destfile, ssb, &attr,
                                    pp, inode_cache, conn, &result))
                {
                    if (stat(ToChangesPath(destfile), &dsb) == -1)
                    {
                        RecordInterruption(ctx, pp, &attr,
                                           "Can't stat destination '%s'. (stat: %s)",
                                           destfile, GetErrorStr());
                        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                    }
                    else
                    {
                        RecordChange(ctx, pp, &attr, "Updated '%s' from source '%s' on '%s'",
                                     destfile, sourcefile, server);
                        result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                        result = PromiseResultUpdate(result,
                                                     VerifyCopiedFileAttributes(ctx, sourcefile,
                                                                                destfile, ssb,
                                                                                &dsb, &attr, pp));
                    }

                    if (RlistIsInListOfRegex(SINGLE_COPY_LIST, destfile))
                    {
                        StringSetAdd(SINGLE_COPY_CACHE, xstrdup(destfile));
                    }
                }
                else
                {
                    RecordFailure(ctx, pp, &attr, "Was not able to copy '%s' on '%s' to '%s'",
                                  sourcefile, server, destfile);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }

                return result;
            }
            else if (S_ISLNK(srcmode))
            {
                result = PromiseResultUpdate(result,
                                             LinkCopy(ctx, sourcefile, destfile, ssb,
                                                      &attr, pp, inode_cache, conn));
            }
        }
        else
        {
            result = PromiseResultUpdate(result,
                                         VerifyCopiedFileAttributes(ctx, sourcefile, destfile,
                                                                    ssb, &dsb, &attr, pp));

            /* Now we have to check for single copy, even though nothing was
               copied otherwise we can get oscillations between multipe
               versions if type is based on a checksum */

            if (RlistIsInListOfRegex(SINGLE_COPY_LIST, destfile))
            {
                StringSetAdd(SINGLE_COPY_CACHE, xstrdup(destfile));
            }

            RecordNoChange(ctx, pp, &attr, "File '%s' is an up to date copy of source",
                           destfile);
        }
    }

    return result;
}

static PromiseResult PurgeLocalFiles(EvalContext *ctx, Item *filelist, const char *localdir, const Attributes *attr,
                                     const Promise *pp, AgentConnection *conn)
{
    assert(attr != NULL);
    Dir *dirh;
    struct stat sb;
    const struct dirent *dirp;
    char filename[CF_BUFSIZE] = { 0 };

    if (strlen(localdir) < 2)
    {
        RecordFailure(ctx, pp, attr, "Purge of '%s' denied - too dangerous!", localdir);
        return PROMISE_RESULT_FAIL;
    }

    /* If we purge with no authentication we wipe out EVERYTHING ! */

    if (conn && (!conn->authenticated))
    {
        RecordDenial(ctx, pp, attr,
                     "Not purge local files '%s' - no authenticated contact with a source",
                     localdir);
        return PROMISE_RESULT_DENIED;
    }

    if (!attr->havedepthsearch)
    {
        RecordNoChange(ctx, pp, attr,
                       "No depth search when copying '%s' so purging does not apply",
                       localdir);
        return PROMISE_RESULT_NOOP;
    }

/* chdir to minimize the risk of race exploits during copy (which is inherently dangerous) */

    const char *changes_localdir = localdir;
    if (ChrootChanges())
    {
        changes_localdir = ToChangesChroot(localdir);
    }

    if (safe_chdir(changes_localdir) == -1)
    {
        RecordFailure(ctx, pp, attr,
                      "Can't chdir to local directory '%s'. (chdir: %s)",
                      localdir, GetErrorStr());
        return PROMISE_RESULT_FAIL;
    }

    if ((dirh = DirOpen(".")) == NULL)
    {
        RecordFailure(ctx, pp, attr,
                      "Can't open local directory '%s'. (opendir: %s)",
                      localdir, GetErrorStr());
        return PROMISE_RESULT_FAIL;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (!ConsiderLocalFile(dirp->d_name, changes_localdir))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping '%s'", dirp->d_name);
            continue;
        }

        if (!IsItemIn(filelist, dirp->d_name))
        {
            strncpy(filename, localdir, CF_BUFSIZE - 2);

            AddSlash(filename);

            if (strlcat(filename, dirp->d_name, CF_BUFSIZE) >= CF_BUFSIZE)
            {
                RecordFailure(ctx, pp, attr,
                              "Path name '%s%s' is too long in PurgeLocalFiles",
                              filename, dirp->d_name);
                continue;
            }

            const char *changes_filename = filename;
            if (ChrootChanges())
            {
                changes_filename = ToChangesChroot(filename);
            }
            if (MakingChanges(ctx, pp, attr, &result,
                              "purge '%s' from copy dest directory", filename))
            {
                if (lstat(changes_filename, &sb) == -1)
                {
                    RecordInterruption(ctx, pp, attr,
                                       "Couldn't stat '%s' while purging. (lstat: %s)",
                                       filename, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                }
                else if (S_ISDIR(sb.st_mode))
                {
                    if (!DeleteDirectoryTree(changes_filename))
                    {
                        RecordFailure(ctx, pp, attr,
                                      "Unable to purge directory tree '%s'", filename);
                        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    }
                    else if (rmdir(changes_filename) == -1)
                    {
                        if (errno != ENOENT)
                        {
                            RecordFailure(ctx, pp, attr,
                                          "Unable to purge directory '%s'", filename);
                            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                        }
                    }
                    else
                    {
                        RecordChange(ctx, pp, attr,
                                     "Purged directory '%s' in copy dest directory", filename);
                        result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                    }
                }
                else if (unlink(changes_filename) == -1)
                {
                    RecordFailure(ctx, pp, attr, "Couldn't delete '%s' while purging", filename);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }
                else
                {
                    RecordChange(ctx, pp, attr, "Purged '%s' copy dest directory", filename);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }
            }
        }
    }

    DirClose(dirh);

    return result;
}

static PromiseResult SourceSearchAndCopy(EvalContext *ctx, const char *from, char *to, int maxrecurse, const Attributes *attr,
                                         const Promise *pp, dev_t rootdevice, CompressedArray **inode_cache, AgentConnection *conn)
{
    /* TODO overflow check all these str*cpy()s in here! */
    Item *namecache = NULL;

    if (maxrecurse == 0)        /* reached depth limit */
    {
        RecordFailure(ctx, pp, attr, "Maximum recursion level reached at '%s'", from);
        return PROMISE_RESULT_FAIL;
    }

    if (strlen(from) == 0)      /* Check for root dir */
    {
        from = "/";
    }

    /* Check that dest dir exists before starting */

    char newto[CF_BUFSIZE];
    strlcpy(newto, to, sizeof(newto) - 10);
    AddSlash(newto);
    strcat(newto, "dummy");

    PromiseResult result = PROMISE_RESULT_NOOP;
    struct stat tostat;

    bool dir_created = false;
    if (!MakeParentDirectoryForPromise(ctx, pp, attr, &result, newto,
                                       attr->move_obstructions, &dir_created,
                                       DEFAULTMODE))
    {
        return result;
    }
    if (dir_created)
    {
        RecordChange(ctx, pp, attr, "Created parent directory for '%s'", to);
        result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
    }
    /* remove "/dummy" from 'newto' */
    ChopLastNode(newto);

    /* XXX: really delete slash from 'to'??? */
    DeleteSlash(to);

    const char *changes_newto = newto;
    if (ChrootChanges())
    {
        changes_newto = ToChangesChroot(newto);
    }

    /* Set aside symlinks */
    if (lstat(changes_newto, &tostat) != 0)
    {
        RecordFailure(ctx, pp, attr, "Unable to stat newly created directory '%s'. (lstat: %s)",
                      to, GetErrorStr());
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        return result;
    }

    if (S_ISLNK(tostat.st_mode))
    {
        char backup[CF_BUFSIZE];
        mode_t mask;

        if (!attr->move_obstructions)
        {
            RecordFailure(ctx, pp, attr,
                          "Path '%s' is a symlink. Unable to move it aside without move_obstructions is set",
                          to);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            return result;
        }

        strcpy(backup, changes_newto);
        DeleteSlash(to);        /* FIXME: really delete slash from *to*??? */
        strcat(backup, ".cf-moved");

        if (MakingChanges(ctx, pp, attr, &result, "backup '%s'", to))
        {
            if (rename(changes_newto, backup) == -1)
            {
                Log(LOG_LEVEL_ERR, "Unable to backup old '%s'", to);
                unlink(to);
            }
            else
            {
                RecordChange(ctx, pp, attr, "Backed up '%s'", to);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
        }

        if (MakingChanges(ctx, pp, attr, &result, "create directory '%s'", to))
        {
            mask = umask(0);
            if (mkdir(changes_newto, DEFAULTMODE) == -1)
            {
                RecordFailure(ctx, pp, attr,
                              "Failed to make directory '%s'. (mkdir: %s)", to, GetErrorStr());
                umask(mask);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                return result;
            }
            else
            {
                RecordChange(ctx, pp, attr, "Created directory for '%s'", to);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
            umask(mask);
        }
    }

    /* (conn == NULL) means copy is from localhost. */
    const char *changes_from = from;
    bool local_changed_from = false;
    if ((conn == NULL) && ChrootChanges())
    {
        const char *chrooted_from = ToChangesChroot(from);
        struct stat sb;
        if (lstat(changes_from, &sb) != -1)
        {
            changes_from = chrooted_from;
            local_changed_from = true;
        }
    }

    /* Send OPENDIR command. */
    AbstractDir *dirh;
    if ((dirh = AbstractDirOpen(changes_from, &(attr->copy), conn)) == NULL)
    {
        RecordInterruption(ctx, pp, attr, "Can't open directory '%s' for copying", from);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    /* No backslashes over the network. */
    const char sep = (conn != NULL) ? '/' : FILE_SEPARATOR;

    struct stat sb, dsb;
    char newfrom[CF_BUFSIZE];
    const struct dirent *dirp;
    for (dirp = AbstractDirRead(dirh); dirp != NULL; dirp = AbstractDirRead(dirh))
    {
        /* This sends 1st STAT command. */
        if (!ConsiderAbstractFile(dirp->d_name, from, &(attr->copy), conn))
        {
            if (conn != NULL &&
                conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
            {
                RecordInterruption(ctx, pp, attr,
                                   "Connection error when checking '%s'", dirp->d_name);
                AbstractDirClose(dirh);
                result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                return result;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Skipping '%s'", dirp->d_name);
                continue;
            }
        }

        if (attr->copy.purge)
        {
            /* Append this file to the list of files not to purge */
            AppendItem(&namecache, dirp->d_name, NULL);
        }

        /* Assemble pathnames. TODO check overflow. */
        strlcpy(newfrom, from, sizeof(newfrom));
        strlcpy(newto, to, sizeof(newto));

        if (!PathAppend(newfrom, sizeof(newfrom), dirp->d_name, sep))
        {
            RecordFailure(ctx, pp, attr, "Internal limit reached in SourceSearchAndCopy(),"
                          " source path too long: '%s' + '%s'",
                          newfrom, dirp->d_name);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            AbstractDirClose(dirh);
            return result;
        }

        const char *changes_newfrom = newfrom;
        if (ChrootChanges() && local_changed_from)
        {
            changes_newfrom = ToChangesChroot(newfrom);
        }

        /* This issues a 2nd STAT command, hopefully served from cache. */

        if ((attr->recursion.travlinks) || (attr->copy.link_type == FILE_LINK_TYPE_NONE))
        {
            /* No point in checking if there are untrusted symlinks here,
               since this is from a trusted source, by definition */

            if (cf_stat(changes_newfrom, &sb, &(attr->copy), conn) == -1)
            {
                Log(LOG_LEVEL_VERBOSE, "Can't stat '%s'. (cf_stat: %s)", newfrom, GetErrorStr());
                if (conn != NULL &&
                    conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
                {
                    RecordInterruption(ctx, pp, attr, "Connection error when checking '%s'", newfrom);
                    AbstractDirClose(dirh);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                    return result;
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "Skipping '%s'", newfrom);
                    continue;
                }
            }
        }
        else
        {
            if (cf_lstat(changes_newfrom, &sb, &(attr->copy), conn) == -1)
            {
                Log(LOG_LEVEL_VERBOSE, "Can't stat '%s'. (cf_stat: %s)", newfrom, GetErrorStr());
                if (conn != NULL &&
                    conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
                {
                    RecordInterruption(ctx, pp, attr, "Connection error when checking '%s'", newfrom);
                    AbstractDirClose(dirh);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                    return result;
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "Skipping '%s'", newfrom);
                    continue;
                }
            }
        }

        /* If "collapse_destination_dir" is set, we skip subdirectories, which
         * means we are not creating them in the destination folder. */

        if (!attr->copy.collapse ||
            (attr->copy.collapse && !S_ISDIR(sb.st_mode)))
        {
            if (!PathAppend(newto, sizeof(newto), dirp->d_name,
                            FILE_SEPARATOR))
            {
                RecordFailure(ctx, pp, attr,
                              "Internal limit reached in SourceSearchAndCopy(),"
                              " dest path too long: '%s' + '%s'",
                              newto, dirp->d_name);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                AbstractDirClose(dirh);
                return result;
            }
        }
        if (ChrootChanges())
        {
            /* Need to refresh 'changes_newto' because 'newto' could have been
             * changed above and also the above call of ToChangesChroot() for
             * 'changes_newfrom' modifies the internal buffer returned by
             * ToChangesChroot(). */
            changes_newto = ToChangesChroot(newto);
        }

        if ((attr->recursion.xdev) && (DeviceBoundary(&sb, rootdevice)))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping '%s' on different device", newfrom);
            continue;
        }

        if (S_ISDIR(sb.st_mode))
        {
            if (attr->recursion.travlinks)
            {
                Log(LOG_LEVEL_VERBOSE, "Traversing directory links during copy is too dangerous, pruned");
                continue;
            }

            if (SkipDirLinks(ctx, newfrom, dirp->d_name, attr->recursion))
            {
                continue;
            }

            memset(&dsb, 0, sizeof(struct stat));

            /* Only copy dirs if we are tracking subdirs */

            if ((!attr->copy.collapse) && (stat(newto, &dsb) == -1))
            {
                if (mkdir(changes_newto, 0700) == -1)
                {
                    RecordInterruption(ctx, pp, attr, "Can't make directory '%s'. (mkdir: %s)",
                                       newto, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                    /* XXX: return result?!?!?! */
                    continue;
                }
                else
                {
                    RecordChange(ctx, pp, attr, "Created directory '%s'", newto);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }

                if (stat(changes_newto, &dsb) == -1)
                {
                    RecordInterruption(ctx, pp, attr,
                                       "Can't stat local copy '%s' - failed to establish directory. (stat: %s)",
                                       newto, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                    /* XXX: return result?!?!?! */
                    continue;
                }
            }

            Log(LOG_LEVEL_VERBOSE, "Entering '%s'", newto);

            if (!attr->copy.collapse)
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
            RecordInterruption(ctx, pp, attr, "connection error");
            AbstractDirClose(dirh);
            return PROMISE_RESULT_INTERRUPTED;
        }
    }

    if (attr->copy.purge)
    {
        PurgeLocalFiles(ctx, namecache, to, attr, pp, conn);
        DeleteItemList(namecache);
    }

    AbstractDirClose(dirh);

    return result;
}

static PromiseResult VerifyCopy(EvalContext *ctx,
                                const char *source, char *destination,
                                const Attributes *attr, const Promise *pp,
                                CompressedArray **inode_cache,
                                AgentConnection *conn)
{
    assert(attr != NULL);

    /* (conn == NULL) means copy is from localhost. */
    const char *changes_source = source;
    bool local_changed_src = false;
    if ((conn == NULL) && ChrootChanges())
    {
        const char *chrooted_source = ToChangesChroot(source);
        if (access(chrooted_source, F_OK) == 0)
        {
            local_changed_src = true;
            changes_source = chrooted_source;
        }
    }

    int found;
    struct stat ssb;

    if (attr->copy.link_type == FILE_LINK_TYPE_NONE)
    {
        Log(LOG_LEVEL_DEBUG, "Treating links as files for '%s'", source);
        found = cf_stat(changes_source, &ssb, &(attr->copy), conn);
    }
    else
    {
        found = cf_lstat(changes_source, &ssb, &(attr->copy), conn);
    }

    if (found == -1)
    {
        RecordFailure(ctx, pp, attr, "Can't stat '%s' in verify copy", source);
        return PROMISE_RESULT_FAIL;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (ssb.st_nlink > 1)      /* Preserve hard link structure when copying */
    {
        RegisterAHardLink(ssb.st_ino, ToChangesPath(destination),
                          ctx, pp, attr, &result, inode_cache);
    }

    if (S_ISDIR(ssb.st_mode))
    {
        char sourcedir[CF_BUFSIZE];
        strcpy(sourcedir, source);
        AddSlash(sourcedir);

        const char *changes_sourcedir = sourcedir;
        if (local_changed_src)
        {
            changes_sourcedir = ToChangesChroot(sourcedir);
        }

        AbstractDir *dirh;
        if ((dirh = AbstractDirOpen(changes_sourcedir, &(attr->copy), conn)) == NULL)
        {
            RecordFailure(ctx, pp, attr, "Can't open directory '%s'. (opendir: %s)",
                          sourcedir, GetErrorStr());
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            return result;
        }

        /* Now check any overrides */

        char destdir[CF_BUFSIZE];
        strcpy(destdir, destination);
        AddSlash(destdir);

        const char *changes_destdir = destdir;
        if (ChrootChanges())
        {
            changes_destdir = ToChangesChroot(destdir);
        }

        struct stat dsb;
        if (stat(changes_destdir, &dsb) == -1)
        {
            RecordFailure(ctx, pp, attr, "Can't stat directory '%s'. (stat: %s)",
                          destdir, GetErrorStr());
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
        else
        {
            result = PromiseResultUpdate(result,
                                         VerifyCopiedFileAttributes(ctx, sourcedir, destdir,
                                                                    &ssb, &dsb, attr, pp));
        }

        /* No backslashes over the network. */
        const char sep = (conn != NULL) ? '/' : FILE_SEPARATOR;

        for (const struct dirent *dirp = AbstractDirRead(dirh);
             dirp != NULL;
             dirp = AbstractDirRead(dirh))
        {
            if (!ConsiderAbstractFile(dirp->d_name, sourcedir,
                                      &(attr->copy), conn))
            {
                if (conn != NULL &&
                    conn->conn_info->status != CONNECTIONINFO_STATUS_ESTABLISHED)
                {
                    RecordInterruption(ctx, pp, attr,
                                       "Connection error when checking '%s'", dirp->d_name);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                    return result;
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "Skipping '%s'", dirp->d_name);
                }
            }

            char sourcefile[CF_BUFSIZE];
            strcpy(sourcefile, sourcedir);

            if (!PathAppend(sourcefile, sizeof(sourcefile), dirp->d_name,
                            sep))
            {
                /* TODO return FAIL */
                FatalError(ctx, "VerifyCopy sourcefile buffer limit");
            }

            char destfile[CF_BUFSIZE];
            strcpy(destfile, destdir);

            if (!PathAppend(destfile, sizeof(destfile), dirp->d_name,
                            FILE_SEPARATOR))
            {
                /* TODO return FAIL */
                FatalError(ctx, "VerifyCopy destfile buffer limit");
            }

            const char *changes_sourcefile = sourcefile;
            if (local_changed_src)
            {
                changes_sourcefile = ToChangesChroot(sourcefile);
            }
            if (attr->copy.link_type == FILE_LINK_TYPE_NONE)
            {
                if (cf_stat(changes_sourcefile, &ssb, &(attr->copy), conn) == -1)
                {
                    RecordFailure(ctx, pp, attr,
                                  "Can't stat source file (notlinked) '%s'. (stat: %s)",
                                  sourcefile, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    return result;
                }
            }
            else
            {
                if (cf_lstat(changes_sourcefile, &ssb, &(attr->copy), conn) == -1)
                {
                    RecordFailure(ctx, pp, attr, "Can't stat source file '%s'. (lstat: %s)",
                                  sourcefile, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    return result;
                }
            }

            result = PromiseResultUpdate(result, CfCopyFile(ctx, sourcefile, destfile, &ssb,
                                                            attr, pp, inode_cache, conn));
        }

        AbstractDirClose(dirh);
        return result;
    }
    else
    {
        /* TODO: are these copies necessary??? */
        char sourcefile[CF_BUFSIZE];
        char destfile[CF_BUFSIZE];
        strcpy(sourcefile, source);
        strcpy(destfile, destination);

        return CfCopyFile(ctx, sourcefile, destfile, &ssb,
                          attr, pp, inode_cache, conn);
    }
}

static PromiseResult LinkCopy(EvalContext *ctx, char *sourcefile, char *destfile, const struct stat *sb, const Attributes *attr, const Promise *pp,
                              CompressedArray **inode_cache, AgentConnection *conn)
/* Link the file to the source, instead of copying */
#ifdef __MINGW32__
{
    RecordFailure(ctx, pp, attr,
                  "Can't link '%s' to '%s' (Windows does not support symbolic links)",
                  sourcefile, destfile);
    return PROMISE_RESULT_FAIL;
}
#else                           /* !__MINGW32__ */
{
    assert(attr != NULL);
    char linkbuf[CF_BUFSIZE - 1];
    const char *lastnode;
    PromiseResult result = PROMISE_RESULT_NOOP;

    linkbuf[0] = '\0';

    /* (conn == NULL) means copy is from localhost. */
    const char *changes_sourcefile = sourcefile;
    bool local_changed_src = false;
    if ((conn == NULL) && ChrootChanges())
    {
        changes_sourcefile = ToChangesChroot(sourcefile);
        struct stat src_sb;
        local_changed_src = (lstat(changes_sourcefile, &src_sb) != -1);
    }
    char chrooted_linkbuf[sizeof(linkbuf)];
    chrooted_linkbuf[0] = '\0';

    if (S_ISLNK(sb->st_mode))
    {
        if (local_changed_src)
        {
            if (cf_readlink(ctx, changes_sourcefile, chrooted_linkbuf, sizeof(chrooted_linkbuf),
                            attr, pp, conn, &result) == -1)
            {
                RecordFailure(ctx, pp, attr, "Can't read link '%s'", sourcefile);
                return PROMISE_RESULT_FAIL;
            }
            if (IsAbsoluteFileName(chrooted_linkbuf))
            {
                strlcpy(linkbuf, ToNormalRoot(chrooted_linkbuf), sizeof(linkbuf));
            }
            else
            {
                strlcpy(linkbuf, chrooted_linkbuf, sizeof(linkbuf));
            }
        }
        else
        {
            if (cf_readlink(ctx, sourcefile, linkbuf, sizeof(linkbuf),
                            attr, pp, conn, &result) == -1)
            {
                RecordFailure(ctx, pp, attr, "Can't read link '%s'", sourcefile);
                return PROMISE_RESULT_FAIL;
            }
        }

        Log(LOG_LEVEL_VERBOSE, "Checking link from '%s' to '%s'", destfile, linkbuf);

        if ((attr->copy.link_type == FILE_LINK_TYPE_ABSOLUTE) && (!IsAbsoluteFileName(linkbuf)))        /* Not absolute path - must fix */
        {
            char vbuff[CF_BUFSIZE];

            if (local_changed_src)
            {
                strlcpy(vbuff, changes_sourcefile, CF_BUFSIZE);
                ChopLastNode(vbuff);
                AddSlash(vbuff);
                strncat(vbuff, chrooted_linkbuf, CF_BUFSIZE - 1);
                strlcpy(chrooted_linkbuf, vbuff, CF_BUFSIZE - 1);
            }
            else
            {
                strlcpy(vbuff, sourcefile, CF_BUFSIZE);
                ChopLastNode(vbuff);
                AddSlash(vbuff);
                strncat(vbuff, linkbuf, CF_BUFSIZE - 1);
                strlcpy(linkbuf, vbuff, CF_BUFSIZE - 1);
            }
        }
    }
    else
    {
        if (local_changed_src)
        {
            strlcpy(chrooted_linkbuf, changes_sourcefile, CF_BUFSIZE - 1);
        }
        else
        {
            strlcpy(linkbuf, sourcefile, CF_BUFSIZE - 1);
        }
    }

    lastnode = ReadLastNode(sourcefile);

    if (MatchRlistItem(ctx, attr->copy.copy_links, lastnode))
    {
        if (local_changed_src)
        {
            ExpandLinks(chrooted_linkbuf, changes_sourcefile, 0, CF_MAXLINKLEVEL);
            strlcpy(linkbuf, ToNormalRoot(chrooted_linkbuf), sizeof(linkbuf));
        }
        else
        {
            ExpandLinks(linkbuf, sourcefile, 0, CF_MAXLINKLEVEL);
        }
        Log(LOG_LEVEL_VERBOSE, "Link item in copy '%s' marked for copying from '%s' instead",
            sourcefile, linkbuf);

        struct stat ssb;
        if (local_changed_src)
        {
            stat(chrooted_linkbuf, &ssb);
        }
        else
        {
            stat(linkbuf, &ssb);
        }

        /* CfCopyFiles() does the chrooting (if necessary) on its second
         * argument so we need to always give it the original path. */
        return CfCopyFile(ctx, linkbuf, destfile, &ssb, attr, pp, inode_cache, conn);
    }

    int status;
    switch (attr->copy.link_type)
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
        ProgrammingError("Unhandled link type in switch: %d", attr->copy.link_type);
    }

    if ((status == PROMISE_RESULT_CHANGE) || (status == PROMISE_RESULT_NOOP))
    {
        const char *changes_destfile = destfile;
        if (ChrootChanges())
        {
            changes_destfile = ToChangesChroot(destfile);
        }

        struct stat dsb;
        if (lstat(changes_destfile, &dsb) == -1)
        {
            RecordFailure(ctx, pp, attr, "Can't lstat '%s'. (lstat: %s)", destfile, GetErrorStr());
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            return result;
        }
        else
        {
            result = PromiseResultUpdate(result,
                                         VerifyCopiedFileAttributes(ctx, sourcefile, destfile,
                                                                    sb, &dsb, attr, pp));
        }
    }

    return result;
}
#endif /* !__MINGW32__ */

bool CopyRegularFile(EvalContext *ctx, const char *source, const char *dest, const struct stat *sstat,
                     const Attributes *attr, const Promise *pp, CompressedArray **inode_cache,
                     AgentConnection *conn, PromiseResult *result)
{
    assert(attr != NULL);
    assert(sstat != NULL);

    char new[CF_BUFSIZE], *linkable;
    int remote = false, backupisdir = false, backupok = false, discardbackup;

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

    discardbackup = ((attr->copy.backup == BACKUP_OPTION_NO_BACKUP) || (attr->copy.backup == BACKUP_OPTION_REPOSITORY_STORE));

    if (!MakingChanges(ctx, pp, attr, result, "copy '%s' to '%s'", source, dest))
    {
        return false;
    }

    /* Make an assoc array of inodes used to preserve hard links */

    linkable = CompressedArrayValue(*inode_cache, sstat->st_ino);

    /* If making changes in chroot, we need to make sure the target is in the
     * changes chroot before we create the hardlink. */
    if (ChrootChanges() && (linkable != NULL) && (sstat->st_nlink > 1))
    {
        PrepareChangesChroot(linkable);
    }

    if (sstat->st_nlink > 1)     /* Preserve hard links, if possible */
    {
        if ((linkable != NULL) && (strcmp(dest, linkable) != 0))
        {
            if (unlink(ToChangesPath(dest)) == 0)
            {
                RecordChange(ctx, pp, attr, "Removed '%s'", dest);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
            MakeHardLink(ctx, dest, linkable, attr, pp, result);
            return true;
        }
    }

    if (conn != NULL)
    {
        assert(attr->copy.servers && strcmp(RlistScalarValue(attr->copy.servers), "localhost"));
        Log(LOG_LEVEL_DEBUG, "This is a remote copy from server '%s'", RlistScalarValue(attr->copy.servers));
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
#endif
    {
        strlcpy(new, dest, CF_BUFSIZE);

        if (!JoinSuffix(new, sizeof(new), CF_NEW))
        {
            Log(LOG_LEVEL_ERR, "Unable to construct filename for copy");
            return false;
        }
    }

    struct stat dest_stat;
    int ret = stat(ToChangesPath(dest), &dest_stat);
    bool dest_exists = (ret == 0);

    if (remote)
    {
        if (conn->error)
        {
            RecordFailure(ctx, pp, attr, "Failed to copy file '%s' from '%s' (connection error)",
                          source, conn->remoteip);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return false;
        }

        if (!CopyRegularFileNet(source, ToChangesPath(new),
                                sstat->st_size, attr->copy.encrypt, conn))
        {
            RecordFailure(ctx, pp, attr, "Failed to copy file '%s' from '%s'",
                          source, conn->remoteip);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return false;
        }
        RecordChange(ctx, pp, attr, "Copied file '%s' from '%s' to '%s'",
                     source, conn->remoteip, new);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    }
    else
    {
        const char *changes_source = source;
        char *chrooted_source = NULL;
        if (ChrootChanges())
        {
            /* Need to create a copy because the next ToChangesChroot() call
             * will override the internal buffer. */
            chrooted_source = xstrdup(ToChangesChroot(source));
            struct stat sb;
            if (lstat(chrooted_source, &sb) != -1)
            {
                changes_source = chrooted_source;
            }
        }
        // If preserve is true, retain permissions of source file
        if (attr->copy.preserve)
        {
            if (!CopyRegularFileDisk(changes_source, ToChangesPath(new)))
            {
                RecordFailure(ctx, pp, attr, "Failed copying file '%s' to '%s'", source, new);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                return false;
            }
            RecordChange(ctx, pp, attr, "Copied file '%s' to '%s' (permissions preserved)",
                         source, new);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            // Never preserve SUID bit (0777)
            int mode = dest_exists ? (dest_stat.st_mode & 0777) : CF_PERMS_DEFAULT;
            if (!CopyRegularFileDiskPerms(changes_source,
                                          ToChangesPath(new),
                                          mode))
            {
                RecordFailure(ctx, pp, attr, "Failed copying file '%s' to '%s'", source, new);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                return false;
            }
            RecordChange(ctx, pp, attr, "Copied file '%s' to '%s' (mode '%jo')",
                         source, new, (uintmax_t) mode);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }
        free(chrooted_source);

#ifdef HAVE_UTIME_H
        if (attr->copy.stealth)
        {
            timebuf.actime = sstat->st_atime;
            timebuf.modtime = sstat->st_mtime;
            utime(source, &timebuf);
        }
#endif
    }

    Log(LOG_LEVEL_VERBOSE, "Copy of regular file succeeded '%s' to '%s'", source, new);

    char backup[CF_BUFSIZE];
    char chrooted_backup[CF_BUFSIZE];
    const char *changes_backup = backup;
    backup[0] = '\0';
    chrooted_backup[0] = '\0';

    /* XXX: Do RecordChange() for the changes done below? They are just "behind
     *      the scenes" changes the user maybe doesn't care about if they just
     *      work? */
    if (dest_exists)
    {
        if (!discardbackup)
        {
            char stamp[CF_BUFSIZE];
            time_t stampnow;

            Log(LOG_LEVEL_DEBUG, "Backup file '%s'", source);

            strlcpy(backup, dest, CF_BUFSIZE);
            if (attr->copy.backup == BACKUP_OPTION_TIMESTAMP)
            {
                stampnow = time((time_t *) NULL);
                snprintf(stamp, CF_BUFSIZE - 1, "_%lu_%s",
                         CFSTARTTIME, CanonifyName(ctime(&stampnow)));

                if (!JoinSuffix(backup, sizeof(backup), stamp))
                {
                    RecordFailure(ctx, pp, attr,
                                  "Failed to determine file name for the backup of '%s'",
                                  dest);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                    return false;
                }
            }

            if (!JoinSuffix(backup, sizeof(backup), CF_SAVED))
            {
                RecordFailure(ctx, pp, attr,
                              "Failed to determine file name for the backup of '%s'",
                              dest);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                return false;
            }

            if (ChrootChanges())
            {
                strlcpy(chrooted_backup, ToChangesChroot(backup), CF_BUFSIZE);
                changes_backup = chrooted_backup;
            }

            Log(LOG_LEVEL_VERBOSE, "Backup for '%s' is '%s'", dest, backup);

            /* Now in case of multiple copies of same object,
             * try to avoid overwriting original backup */

            if (lstat(changes_backup, &dest_stat) != -1)
            {
                /* if there is a dir in the way */
                if (S_ISDIR(dest_stat.st_mode))
                {
                    backupisdir = true;

                    /* PurgeLocalFiles() does ToChangesChroot() internally (if
                     * needed). */
                    PurgeLocalFiles(ctx, NULL, backup, attr, pp, conn);
                    if (rmdir(changes_backup) == 0)
                    {
                        RecordChange(ctx, pp, attr, "Removed old backup directory '%s'", backup);
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                    }
                    else
                    {
                        RecordFailure(ctx, pp, attr,
                                      "Failed to remove old backup directory '%s'", backup);
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                    }
                }

                if (unlink(changes_backup) == 0)
                {
                    RecordChange(ctx, pp, attr, "Removed old backup '%s'", backup);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                }
                else
                {
                    RecordFailure(ctx, pp, attr,
                                  "Failed to remove old backup '%s'", backup);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                }
            }

            if (rename(dest, changes_backup) == 0)
            {
                RecordChange(ctx, pp, attr, "Backed up '%s' as '%s'", dest, backup);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                RecordFailure(ctx, pp, attr, "Failed to back up '%s' as '%s'", dest, backup);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            }

            /* Did the rename() succeed? NFS-safe */
            backupok = (lstat(changes_backup, &dest_stat) != -1);
        }
        else
        {
            /* Discarding backup */

            /* Mainly important if there is a dir in the way */
            if (S_ISDIR(dest_stat.st_mode))
            {
                /* PurgeLocalFiles does ToChangesChroot(dest) internally. */
                PurgeLocalFiles(ctx, NULL, dest, attr, pp, conn);

                if (rmdir(ToChangesPath(dest)) == 0)
                {
                    RecordChange(ctx, pp, attr, "Removed directory '%s'", dest);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                }
                else
                {
                    RecordFailure(ctx, pp, attr,
                                  "Failed to remove directory '%s'", dest);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                }
            }
        }
    }

    const char *changes_new = new;
    char *chrooted_new = NULL;
    if (ChrootChanges())
    {
        chrooted_new = xstrdup(ToChangesChroot(new));
        changes_new = chrooted_new;
    }
    const char *changes_dest = dest;
    if (ChrootChanges())
    {
        changes_dest = ToChangesChroot(dest);
    }

    struct stat new_stat;
    if (lstat(changes_new, &new_stat) == -1)
    {
        RecordFailure(ctx, pp, attr,
                      "Can't stat new file '%s' - another agent has picked it up?. (stat: %s)",
                      new, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        free(chrooted_new);
        return false;
    }

    if ((S_ISREG(new_stat.st_mode)) && (new_stat.st_size != sstat->st_size))
    {
        RecordFailure(ctx, pp, attr,
                      "New file '%s' seems to have been corrupted in transit,"
                      " destination size (%zd) differs from the source size (%zd), aborting.",
                      new, (size_t) new_stat.st_size, (size_t) sstat->st_size);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);

        if (backupok && (rename(changes_backup, changes_dest) == 0))
        {
            RecordChange(ctx, pp, attr, "Restored '%s' from '%s'", dest, backup);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            RecordFailure(ctx, pp, attr, "Could not restore '%s' as '%s'", dest, backup);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        }

        free(chrooted_new);
        return false;
    }

    if (attr->copy.verify)
    {
        Log(LOG_LEVEL_VERBOSE, "Final verification of transmission ...");

        if (CompareFileHashes(source, changes_new, sstat, &new_stat, &(attr->copy), conn))
        {
            RecordFailure(ctx, pp, attr,
                          "New file '%s' seems to have been corrupted in transit, aborting.", new);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);

            if (backupok && (rename(changes_backup, changes_dest) == 0))
            {
                RecordChange(ctx, pp, attr, "Restored '%s' from '%s'", dest, backup);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                RecordFailure(ctx, pp, attr, "Could not restore '%s' as '%s'", dest, backup);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            }

            free(chrooted_new);
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
        rsrcrd = safe_open(changes_new, O_RDONLY | O_BINARY);
        rsrcwd = safe_open(changes_dest, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC);

        if (rsrcrd == -1 || rsrcwd == -1)
        {
            Log(LOG_LEVEL_INFO, "Open of Darwin resource fork rsrcrd/rsrcwd failed. (open: %s)", GetErrorStr());
            close(rsrcrd);
            close(rsrcwd);
            free(chrooted_new);
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
                    RecordFailure(ctx, pp, attr,
                                  "Read of Darwin resource fork rsrcrd '%s' failed (read: %s)",
                                  new, GetErrorStr());
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                    close(rsrcrd);
                    close(rsrcwd);
                    free(rsrcbuf);
                    free(chrooted_new);
                    return (false);
                }
            }

            else if (rsrcbytesr == 0)
            {
                /* Reached EOF */
                close(rsrcrd);
                close(rsrcwd);
                free(rsrcbuf);

                if (unlink(changes_new) == 0)
                {
                    RecordChange(ctx, pp, attr, "Moved resource fork '%s' to '%s'", new, dest);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                }
                else
                {
                    RecordFailure(ctx, pp, attr, "Failed to remove resource fork '%s'", new);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                    /* not a fatal failure, just record it and keep moving */
                }
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
                        RecordFailure(ctx, pp, attr,
                                      "Write of Darwin resource fork rsrcwd '%s' failed (write: %s)",
                                      dest, GetErrorStr());
                        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                        close(rsrcrd);
                        close(rsrcwd);
                        free(rsrcbuf);
                        free(chrooted_new);
                        return (false);
                    }
                }
                rsrcbytesl = rsrcbytesr - rsrcbytesw;
            }
        }
    }
    else
#endif
    {
        if (rename(changes_new, changes_dest) == 0)
        {
            RecordChange(ctx, pp, attr, "Moved '%s' to '%s'", new, dest);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            RecordFailure(ctx, pp, attr,
                          "Could not install copy file as '%s', directory in the way?. (rename: %s)",
                          dest, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);

            if (backupok && (rename(changes_backup, changes_dest) == 0))
            {
                RecordChange(ctx, pp, attr, "Restored '%s' from '%s'", dest, backup);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                RecordFailure(ctx, pp, attr, "Could not restore '%s' as '%s'", dest, backup);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            }

            free(chrooted_new);
            return false;
        }
    }
    free(chrooted_new);

    if ((!discardbackup) && backupisdir)
    {
        Log(LOG_LEVEL_INFO, "Cannot move a directory to repository, leaving at '%s'", backup);
    }
    else if ((!discardbackup) && (ArchiveToRepository(changes_backup, attr)))
    {
        RecordChange(ctx, pp, attr, "Archived backup '%s'", backup);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

        if (unlink(changes_backup) != 0)
        {
            RecordFailure(ctx, pp, attr, "Failed to clean backup '%s' after archiving", backup);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        }
    }

#ifdef HAVE_UTIME_H
    if (attr->copy.stealth)
    {
        timebuf.actime = sstat->st_atime;
        timebuf.modtime = sstat->st_mtime;
        utime(dest, &timebuf);
    }
#endif

    return true;
}

static bool TransformFile(EvalContext *ctx, char *file, const Attributes *attr, const Promise *pp, PromiseResult *result)
{
    assert(attr != NULL);
    FILE *pop = NULL;
    int transRetcode = 0;

    if (attr->transformer == NULL || file == NULL)
    {
        return false;
    }

    Buffer *command = BufferNew();
    ExpandScalar(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, attr->transformer, command);

    if (!IsExecutable(CommandArg0(BufferData(command))))
    {
        RecordFailure(ctx, pp, attr, "Transformer '%s' for file '%s' failed", attr->transformer, file);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        BufferDestroy(command);
        return false;
    }

    const char * const command_str = BufferData(command);
    if (MakingChanges(ctx, pp, attr, result, "transform file '%s' with '%s'", file, command_str))
    {
        CfLock thislock = AcquireLock(ctx, command_str, VUQNAME, CFSTARTTIME, attr->transaction.ifelapsed, attr->transaction.expireafter, pp, false);

        if (thislock.lock == NULL)
        {
            BufferDestroy(command);
            return false;
        }

        const char *changes_command = command_str;
        char *chrooted_command = NULL;
        if (ChrootChanges())
        {
            size_t command_len = BufferSize(command);
            chrooted_command = xmalloc(command_len + PATH_MAX + 1);
            strncpy(chrooted_command, command_str, command_len + 1);
            ssize_t ret = StringReplace(chrooted_command, command_len + PATH_MAX + 1,
                                        file, ToChangesChroot(file));
            if ((ret <= 0) || ((size_t) ret == command_len + PATH_MAX + 1))
            {
                RecordFailure(ctx, pp, attr,
                              "Failed to replace path '%s' in transformer command '%s' for changes chroot",
                              file, command_str);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                YieldCurrentLock(thislock);
                BufferDestroy(command);
                free(chrooted_command);
                return false;
            }
            else
            {
                changes_command = chrooted_command;
            }
        }

        Log(LOG_LEVEL_INFO, "Transforming '%s' with '%s'", file, command_str);
        if ((pop = cf_popen(changes_command, "r", true)) == NULL)
        {
            RecordFailure(ctx, pp, attr, "Transformer '%s' for file '%s' failed", attr->transformer, file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            YieldCurrentLock(thislock);
            BufferDestroy(command);
            free(chrooted_command);
            return false;
        }
        free(chrooted_command);

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
                    RecordFailure(ctx, pp, attr, "Transformer '%s' for file '%s' failed", attr->transformer, file);
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
        RecordChange(ctx, pp, attr, "Transformed '%s' with '%s' ", file, command_str);
        free(line);

        transRetcode = cf_pclose(pop);

        if (VerifyCommandRetcode(ctx, transRetcode, attr, pp, result))
        {
            Log(LOG_LEVEL_INFO, "Transformer '%s' => '%s' seemed to work ok", file, command_str);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Transformer '%s' => '%s' returned error", file, command_str);
        }

        YieldCurrentLock(thislock);
    }

    BufferDestroy(command);
    return true;
}

static PromiseResult VerifyName(EvalContext *ctx, char *path, const struct stat *sb, const Attributes *attr, const Promise *pp)
{
    assert(attr != NULL);
    mode_t newperm;
    struct stat dsb;

    const char *changes_path = path;
    char *chrooted_path = NULL;
    if (ChrootChanges())
    {
        chrooted_path = xstrdup(ToChangesChroot(path));
        changes_path = chrooted_path;
    }

    if (lstat(changes_path, &dsb) == -1)
    {
        RecordNoChange(ctx, pp, attr, "File object named '%s' is not there (promise kept)", path);
        free(chrooted_path);
        return PROMISE_RESULT_NOOP;
    }
    else
    {
        if (attr->rename.disable)
        {
            /* XXX: Why should attr->rename.disable imply that the file doesn't exist? */
            Log(LOG_LEVEL_WARNING, "File object '%s' exists, contrary to promise", path);
        }
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (attr->rename.newname != NULL)
    {
        char newname[CF_BUFSIZE];
        if (IsAbsoluteFileName(attr->rename.newname))
        {
            size_t copied = strlcpy(newname, attr->rename.newname, sizeof(newname));
            if (copied > sizeof(newname))
            {
                RecordFailure(ctx, pp, attr,
                              "Internal buffer limit in rename operation, new name too long: '%s'",
                              attr->rename.newname);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                free(chrooted_path);
                return result;
            }
        }
        else
        {
            strncpy(newname, path, sizeof(newname));
            ChopLastNode(newname);

            if (!PathAppend(newname, sizeof(newname),
                            attr->rename.newname, FILE_SEPARATOR))
            {
                RecordFailure(ctx, pp, attr,
                              "Internal buffer limit in rename operation, destination: '%s' + '%s'",
                              newname, attr->rename.newname);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                free(chrooted_path);
                return result;
            }
        }

        const char *changes_newname = newname;
        if (ChrootChanges())
        {
            changes_newname = ToChangesChroot(newname);
        }

        if (MakingChanges(ctx, pp, attr, &result, "rename file '%s' to '%s'",
                          path, newname))
        {
            if (!FileInRepository(newname))
            {
                if (rename(changes_path, changes_newname) == -1)
                {
                    RecordFailure(ctx, pp, attr, "Error occurred while renaming '%s'. (rename: %s)",
                                  path, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }
                else
                {
                    if (ChrootChanges())
                    {
                        RecordFileRenamedInChroot(path, newname);
                    }
                    RecordChange(ctx, pp, attr, "Renamed file '%s' to '%s'", path, newname);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }
            }
            else
            {
                RecordWarning(ctx, pp, attr,
                              "Renaming '%s' to same destination twice would overwrite saved copy - aborting",
                              path);
                result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            }
        }

        free(chrooted_path);
        return result;
    }

    if (S_ISLNK(dsb.st_mode))
    {
        if (attr->rename.disable)
        {
            if (MakingChanges(ctx, pp, attr, &result, "disable link '%s'", path))
            {
                if (unlink(changes_path) == -1)
                {
                    RecordFailure(ctx, pp, attr, "Unable to unlink '%s'. (unlink: %s)",
                                  path, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }
                else
                {
                    RecordChange(ctx, pp, attr, "Disabled symbolic link '%s' by deleting it", path);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }
            }

            free(chrooted_path);
            return result;
        }
    }

/* Normal disable - has priority */

    if (attr->rename.disable)
    {
        char newname[CF_BUFSIZE];

        if (!MakingChanges(ctx, pp, attr, &result, "rename '%s' '%s'",
                           S_ISDIR(sb->st_mode) ? "directory" : "file", path))
        {
            free(chrooted_path);
            return result;
        }

        if (attr->rename.newname && strlen(attr->rename.newname) > 0)
        {
            if (IsAbsoluteFileName(attr->rename.newname))
            {
                strlcpy(newname, attr->rename.newname, CF_BUFSIZE);
            }
            else
            {
                strcpy(newname, path);
                ChopLastNode(newname);

                if (!PathAppend(newname, sizeof(newname),
                                attr->rename.newname, FILE_SEPARATOR))
                {
                    RecordFailure(ctx, pp, attr,
                                  "Internal buffer limit in rename operation, destination: '%s' + '%s'",
                                  newname, attr->rename.newname);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    free(chrooted_path);
                    return result;
                }
            }
        }
        else
        {
            strcpy(newname, path);

            if (attr->rename.disable_suffix)
            {
                if (!JoinSuffix(newname, sizeof(newname), attr->rename.disable_suffix))
                {
                    RecordFailure(ctx, pp, attr,
                                  "Failed to append disable suffix '%s' in rename operation for '%s'",
                                  attr->rename.disable_suffix, path);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    free(chrooted_path);
                    return result;
                }
            }
            else
            {
                if (!JoinSuffix(newname, sizeof(newname), ".cfdisabled"))
                {
                    RecordFailure(ctx, pp, attr,
                                  "Failed to append disable suffix '.cfdisabled' in rename operation for '%s'",
                                  path);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    free(chrooted_path);
                    return result;
                }
            }
        }

        const char *changes_newname = newname;
        if (ChrootChanges())
        {
            changes_newname = ToChangesChroot(newname);
        }

        if ((attr->rename.plus != CF_SAMEMODE) && (attr->rename.minus != CF_SAMEMODE))
        {
            newperm = (sb->st_mode & 07777);
            newperm |= attr->rename.plus;
            newperm &= ~(attr->rename.minus);
        }
        else
        {
            newperm = (mode_t) 0600;
        }

        if (MakingChanges(ctx, pp, attr, &result, "rename file '%s' to '%s'", path, newname))
        {
            if (safe_chmod(changes_path, newperm) == 0)
            {
                RecordChange(ctx, pp, attr, "Changed permissions of '%s' to 'mode %04jo'",
                             path, (uintmax_t)newperm);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                RecordFailure(ctx, pp, attr, "Failed to change permissions of '%s' to 'mode %04jo' (%s)",
                              path, (uintmax_t)newperm, GetErrorStr());
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            }

            if (!FileInRepository(newname))
            {
                if (rename(changes_path, changes_newname) == -1)
                {
                    RecordFailure(ctx, pp, attr, "Error occurred while renaming '%s'. (rename: %s)",
                                  path, GetErrorStr());
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    free(chrooted_path);
                    return result;
                }
                else
                {
                    if (ChrootChanges())
                    {
                        RecordFileRenamedInChroot(path, newname);
                    }
                    RecordChange(ctx, pp, attr, "Disabled file '%s' by renaming to '%s' with mode %04jo",
                                 path, newname, (uintmax_t)newperm);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }

                if (ArchiveToRepository(newname, attr))
                {
                    /* TODO: ArchiveToRepository() does
                     *       Log(LOG_LEVEL_INFO, "Moved '%s' to repository location '%s'", file, destination)
                     *       but we should do LogChange() instead. (maybe just add 'char **destination'
                     *       argument to the function?) */
                    unlink(changes_newname);
                }
            }
            else
            {
                RecordWarning(ctx, pp, attr,
                              "Disable required twice? Would overwrite saved copy - changing permissions only");
                result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            }
        }

        free(chrooted_path);
        return result;
    }

    if (attr->rename.rotate == 0)
    {
        if (MakingChanges(ctx, pp, attr, &result, "truncate '%s'", path))
        {
            TruncateFile(changes_path);
            RecordChange(ctx, pp, attr, "Truncated '%s'", path);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
        free(chrooted_path);
        return result;
    }

    if (attr->rename.rotate > 0)
    {
        if (MakingChanges(ctx, pp, attr, &result, "rotate file '%s' in %d fifo", path, attr->rename.rotate))
        {
            RotateFiles(changes_path, attr->rename.rotate);
            RecordChange(ctx, pp, attr, "Rotated file '%s' in %d fifo", path, attr->rename.rotate);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }

        free(chrooted_path);
        return result;
    }

    free(chrooted_path);
    return result;
}

static PromiseResult VerifyDelete(EvalContext *ctx,
                                  const char *path, const struct stat *sb,
                                  const Attributes *attr, const Promise *pp)
{
    assert(attr != NULL);
    Log(LOG_LEVEL_VERBOSE, "Verifying file deletions for '%s'", path);

    const char *changes_path = path;
    if (ChrootChanges())
    {
        changes_path = ToChangesChroot(path);
    }

    const char *lastnode = ReadLastNode(changes_path);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (MakingChanges(ctx, pp, attr, &result, "delete %s '%s'",
                       S_ISDIR(sb->st_mode) ? "directory" : "file", path))
    {
        if (!S_ISDIR(sb->st_mode))                      /* file,symlink */
        {
            int ret = unlink(lastnode);
            if (ret == -1)
            {
                RecordFailure(ctx, pp, attr, "Couldn't unlink '%s' tidying. (unlink: %s)",
                              path, GetErrorStr());
                return PROMISE_RESULT_FAIL;
            }
            else
            {
                RecordChange(ctx, pp, attr, "Deleted file '%s'", path);
                return PROMISE_RESULT_CHANGE;
            }
        }
        else                                               /* directory */
        {
            if (!attr->delete.rmdirs)
            {
                RecordNoChange(ctx, pp, attr,
                               "Keeping directory '%s' since 'rmdirs' attribute was not specified",
                               path);
                return PROMISE_RESULT_NOOP;
            }

            if (attr->havedepthsearch && strcmp(path, pp->promiser) == 0)
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
                RecordFailure(ctx, pp, attr, "Delete directory '%s' failed (rmdir: %s)",
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
                RecordChange(ctx, pp, attr, "Deleted directory '%s'", path);
                return PROMISE_RESULT_CHANGE;
            }
        }
    }

    return result;
}

static PromiseResult TouchFile(EvalContext *ctx, char *path, const Attributes *attr, const Promise *pp)
{
    PromiseResult result = PROMISE_RESULT_NOOP;
    if (MakingChanges(ctx, pp, attr, &result, "update time stamps for '%s'", path))
    {
        if (utime(ToChangesPath(path), NULL) != -1)
        {
            RecordChange(ctx, pp, attr, "Touched (updated time stamps) for path '%s'", path);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            RecordFailure(ctx, pp, attr, "Touch '%s' failed to update timestamps. (utime: %s)",
                          path, GetErrorStr());
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
    }

    return result;
}

static inline char *GetFileTypeDescription(const struct stat *const stat_buf,
                                           const char *const filename)
{
    /* Use lstat to check if this is a symlink, and don't worry about race
     * conditions as the returned string is only used for a log message. */
    struct stat sb;
    const bool is_link = (lstat(filename, &sb) == 0 && S_ISLNK(sb.st_mode));

    switch (stat_buf->st_mode & S_IFMT)
    {
        case S_IFREG:
            return (is_link) ? "Symbolic link to regular file"
                             : "Regular file";
        case S_IFDIR:
            return (is_link) ? "Symbolic link to directory"
                             : "Directory";
        case S_IFSOCK:
            return (is_link) ? "Symbolic link to socket"
                             : "Socket";
        case S_IFIFO:
            return (is_link) ? "Symbolic link to named pipe"
                             : "Named pipe";
        case S_IFBLK:
            return (is_link) ? "Symbolic link to block device"
                             : "Block device";
        case S_IFCHR:
            return (is_link) ? "Symbolic link to character device"
                             : "Character device";
        default:
            return (is_link) ? "Symbolic link to object"
                             : "Object";
    }
}

static PromiseResult VerifyFileAttributes(EvalContext *ctx, const char *file, const struct stat *dstat, const Attributes *attr, const Promise *pp)
{
    PromiseResult result = PROMISE_RESULT_NOOP;

#ifndef __MINGW32__
    mode_t newperm = dstat->st_mode, maskvalue;

# if defined HAVE_CHFLAGS
    u_long newflags;
# endif

    maskvalue = umask(0);       /* This makes the DEFAULT modes absolute */

    newperm = (dstat->st_mode & 07777);

    if ((attr->perms.plus != CF_SAMEMODE) && (attr->perms.minus != CF_SAMEMODE))
    {
        newperm |= attr->perms.plus;
        newperm &= ~(attr->perms.minus);
        /* directories must have x set if r set, regardless  */

        if (S_ISDIR(dstat->st_mode))
        {
            if (attr->perms.rxdirs)
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
    if (attr->havechange && S_ISREG(dstat->st_mode))
#endif
    {
        result = PromiseResultUpdate(result, VerifyFileIntegrity(ctx, file, attr, pp));
    }

    if (attr->havechange)
    {
        VerifyFileChanges(ctx, file, dstat, attr, pp, &result);
    }

#ifndef __MINGW32__
    if (S_ISLNK(dstat->st_mode))        /* No point in checking permission on a link */
    {
        KillGhostLink(ctx, file, attr, pp, &result);
        umask(maskvalue);
        return result;
    }
#endif

    const char *changes_file = file;

#ifndef __MINGW32__
    result = PromiseResultUpdate(result, VerifySetUidGid(ctx, file, dstat, dstat->st_mode, pp, attr));

    if (ChrootChanges())
    {
        changes_file = ToChangesChroot(file);
    }

    if ((newperm & 07777) == (dstat->st_mode & 07777))  /* file okay */
    {
        Log(LOG_LEVEL_DEBUG, "File okay, newperm '%jo', stat '%jo'",
            (uintmax_t) (newperm & 07777), (uintmax_t) (dstat->st_mode & 07777));
        RecordNoChange(ctx, pp, attr, "File permissions on '%s' as promised", file);
        result = PromiseResultUpdate(result, PROMISE_RESULT_NOOP);
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Trying to fix mode...newperm '%jo', stat '%jo'",
            (uintmax_t) (newperm & 07777), (uintmax_t) (dstat->st_mode & 07777));

        if (MakingChanges(ctx, pp, attr, &result, "change permissions of '%s' from %04jo to %04jo",
                          file, (uintmax_t)dstat->st_mode & 07777, (uintmax_t)newperm & 07777))
        {
            if (safe_chmod(changes_file, newperm & 07777) == -1)
            {
                RecordFailure(ctx, pp, attr, "Failed to change permissions of '%s'. (chmod: %s)",
                              file, GetErrorStr());
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            }
            else
            {
                const char *const object = GetFileTypeDescription(dstat, file);
                RecordChange(ctx, pp, attr,
                             "%s '%s' had permissions %04jo, changed it to %04jo",
                             object, file, (uintmax_t)dstat->st_mode & 07777,
                             (uintmax_t)newperm & 07777);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
        }
    }

# if defined HAVE_CHFLAGS       /* BSD special flags */

    newflags = (dstat->st_flags & CHFLAGS_MASK);
    newflags |= attr->perms.plus_flags;
    newflags &= ~(attr->perms.minus_flags);

    if ((newflags & CHFLAGS_MASK) == (dstat->st_flags & CHFLAGS_MASK))
    {
        RecordNoChange(ctx, pp, attr,
                       "BSD flags of '%s' are as promised ('%jx')",
                       file, (uintmax_t) (dstat->st_flags & CHFLAGS_MASK));
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "BSD Fixing '%s', newflags '%jx', flags '%jx'",
                file, (uintmax_t) (newflags & CHFLAGS_MASK),
                (uintmax_t) (dstat->st_flags & CHFLAGS_MASK));

        if (MakingChanges(ctx, pp, attr, &result,
                          "change BSD flags of '%s' from %jo to %jo",
                          file, (uintmax_t) (dstat->st_mode & CHFLAGS_MASK),
                          (uintmax_t) (newflags & CHFLAGS_MASK)))
        {
            if (chflags(changes_file, newflags & CHFLAGS_MASK) == -1)
            {
                RecordDenial(ctx, pp, attr,
                             "Failed setting BSD flags '%jx' on '%s'. (chflags: %s)",
                             (uintmax_t) newflags, file, GetErrorStr());
                result = PromiseResultUpdate(result, PROMISE_RESULT_DENIED);
            }
            else
            {
                RecordChange(ctx, pp, attr, "'%s' had flags %jo, changed it to %jo",
                             file,
                             (uintmax_t) (dstat->st_flags & CHFLAGS_MASK),
                             (uintmax_t) (newflags & CHFLAGS_MASK));
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
        }
    }
# endif
#endif

    if (attr->acl.acl_entries)
    {
        result = PromiseResultUpdate(result, VerifyACL(ctx, file, attr, pp));
    }

    /* Need to refresh 'changes_file' here because VerifyACL() above modifies
     * the internal buffer used by ToChangesChroot(). */
    if (ChrootChanges())
    {
        changes_file = ToChangesChroot(file);
    }

    if (attr->touch)
    {
        if (utime(changes_file, NULL) == -1)
        {
            RecordDenial(ctx, pp, attr, "Updating timestamps on '%s' failed. (utime: %s)",
                         file, GetErrorStr());
            result = PromiseResultUpdate(result, PROMISE_RESULT_DENIED);
        }
        else
        {
            RecordChange(ctx, pp, attr, "Updated timestamps on '%s'", file);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
    }

#ifndef __MINGW32__
    umask(maskvalue);
#endif

    return result;
}

bool DepthSearch(EvalContext *ctx, char *name, const struct stat *sb, int rlevel, const Attributes *attr,
                const Promise *pp, dev_t rootdevice, PromiseResult *result)
{
    assert(attr != NULL);
    Dir *dirh;
    int goback;
    const struct dirent *dirp;
    struct stat lsb;
    Seq *db_file_set = NULL;
    Seq *selected_files = NULL;
    bool retval = true;

    if (!attr->havedepthsearch)  /* if the search is trivial, make sure that we are in the parent dir of the leaf */
    {
        char basedir[CF_BUFSIZE];

        Log(LOG_LEVEL_DEBUG, "Direct file reference '%s', no search implied", name);
        strlcpy(basedir, ToChangesPath(name), sizeof(basedir));
        ChopLastNode(basedir);
        if (safe_chdir(basedir))
        {
            Log(LOG_LEVEL_ERR, "Failed to chdir into '%s'. (chdir: '%s')", basedir, GetErrorStr());
            return false;
        }
        if (!attr->haveselect || SelectLeaf(ctx, name, sb, &(attr->select)))
        {
            /* Renames are handled separately. */
            if ((EVAL_MODE == EVAL_MODE_SIMULATE_MANIFEST_FULL) && !attr->haverename)
            {
                RecordFileEvaluatedInChroot(name);
            }

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
        RecordWarning(ctx, pp, attr,
                      "Very deep nesting of directories (>%d deep) for '%s' (Aborting files)",
                      rlevel, name);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

    if (!PushDirState(ctx, pp, attr, name, sb, result))
    {
        /* PushDirState() reports errors and updates 'result' in case of failures */
        return false;
    }

    if ((dirh = DirOpen(".")) == NULL)
    {
        Log(LOG_LEVEL_INFO, "Could not open existing directory '%s'. (opendir: %s)", name, GetErrorStr());
        return false;
    }

    if (attr->havechange)
    {
        db_file_set = SeqNew(1, &free);
        if (!FileChangesGetDirectoryList(name, db_file_set))
        {
            RecordFailure(ctx, pp, attr,
                          "Failed to get directory listing for recording file changes in '%s'", name);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            SeqDestroy(db_file_set);
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

        size_t total_len = strlcpy(path, name, sizeof(path));
        if ((total_len >= sizeof(path)) || (JoinPaths(path, sizeof(path), dirp->d_name) == NULL))
        {
            RecordFailure(ctx, pp, attr,
                          "Internal limit reached in DepthSearch(), path too long: '%s' + '%s'",
                          path, dirp->d_name);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
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
                if (ChrootChanges())
                {
                    RecordFileChangedInChroot(path);
                }
                continue;
            }
        }

        /* See if we are supposed to treat links to dirs as dirs and descend */

        if ((attr->recursion.travlinks) && (S_ISLNK(lsb.st_mode)))
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
                RecordFailure(ctx, pp, attr,
                              "Recurse was working on '%s' when this failed. (stat: %s)",
                              path, GetErrorStr());
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                continue;
            }
        }

        if ((attr->recursion.xdev) && (DeviceBoundary(&lsb, rootdevice)))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping '%s' on different device - use xdev option to change this. (stat: %s)", path, GetErrorStr());
            continue;
        }

        if (S_ISDIR(lsb.st_mode))
        {
            if (SkipDirLinks(ctx, path, dirp->d_name, attr->recursion))
            {
                continue;
            }

            if ((attr->recursion.depth > 1) && (rlevel <= attr->recursion.depth))
            {
                Log(LOG_LEVEL_VERBOSE, "Entering '%s', level %d", path, rlevel);
                goback = DepthSearch(ctx, path, &lsb, rlevel + 1, attr, pp, rootdevice, result);
                if (!PopDirState(ctx, pp, attr, goback, name, sb, attr->recursion, result))
                {
                    FatalError(ctx, "Not safe to continue");
                }
            }
        }

        if (!attr->haveselect || SelectLeaf(ctx, path, &lsb, &(attr->select)))
        {
            if (attr->havechange)
            {
                if (!SeqBinaryLookup(db_file_set, dirp->d_name, StrCmpWrapper))
                {
                    // See comments in FileChangesCheckAndUpdateDirectory(),
                    // regarding this function call.
                    FileChangesLogNewFile(path, pp);
                }
                SeqAppend(selected_files, xstrdup(dirp->d_name));
            }

            VerifyFileLeaf(ctx, path, &lsb, attr, pp, result);

            /* Renames are handled separately. */
            if ((EVAL_MODE == EVAL_MODE_SIMULATE_MANIFEST_FULL) && !attr->haverename)
            {
                RecordFileEvaluatedInChroot(path);
            }
            if (ChrootChanges() && (*result == PROMISE_RESULT_CHANGE))
            {
                RecordFileChangedInChroot(path);
            }
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "Skipping non-selected file '%s'", path);
        }
    }

    if (attr->havechange)
    {
        FileChangesCheckAndUpdateDirectory(ctx, attr, name, selected_files, db_file_set,
                                           attr->change.update, pp, result);
    }

end:
    SeqDestroy(selected_files);
    SeqDestroy(db_file_set);
    DirClose(dirh);
    return retval;
}

static bool PushDirState(EvalContext *ctx, const Promise *pp, const Attributes *attr, char *name, const struct stat *sb, PromiseResult *result)
{
    const char *changes_name = (ToChangesPath(name));
    if (safe_chdir(changes_name) == -1)
    {
        RecordFailure(ctx, pp, attr, "Could not change to directory '%s' (mode '%04jo', chdir: %s)",
                      name, (uintmax_t)(sb->st_mode & 07777), GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

    if (!CheckLinkSecurity(sb, changes_name))
    {
        FatalError(ctx, "Not safe to continue");
    }
    return true;
}

/**
 * @return true if safe for agent to continue
 */
static bool PopDirState(EvalContext *ctx, const Promise *pp, const Attributes *attr,
                        int goback, char *name, const struct stat *sb, DirectoryRecursion r,
                        PromiseResult *result)
{
    const char *changes_name = (ToChangesPath(name));
    if (goback && (r.travlinks))
    {
        if (safe_chdir(changes_name) == -1)
        {
            RecordFailure(ctx, pp, attr,
                          "Error in backing out of recursive travlink descent securely to '%s'. (chdir: %s)",
                          name, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return false;
        }

        if (!CheckLinkSecurity(sb, changes_name))
        {
            return false;
        }
    }
    else if (goback)
    {
        if (safe_chdir("..") == -1)
        {
            RecordFailure(ctx, pp, attr,
                          "Error in backing out of recursive descent securely to '%s'. (chdir: %s)",
                          name, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return false;
        }
    }

    return true;
}

/**
 * @return true if it is safe for the agent to continue execution
 */
static bool CheckLinkSecurity(const struct stat *sb, const char *name)
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

static PromiseResult VerifyCopiedFileAttributes(EvalContext *ctx, const char *src, const char *dest, const struct stat *sstat,
                                                const struct stat *dstat, const Attributes *a, const Promise *pp)
{
    assert(a != NULL);
    Attributes attr = *a; // TODO: try to remove this copy
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
    PromiseResult result = VerifyFileAttributes(ctx, dest, dstat, &attr, pp);

#ifndef __MINGW32__
    (attr.perms.owners)->uid = save_uid;
    (attr.perms.groups)->gid = save_gid;
#endif

    const char *changes_src = src;
    char *chrooted_src = NULL;
    if (ChrootChanges())
    {
        /* Need to create a copy because the second call will override the
         * internal buffer used by ToChangesChroot(). */
        chrooted_src = xstrdup(ToChangesChroot(src));
        changes_src = chrooted_src;
    }
    const char *changes_dest = dest;
    if (ChrootChanges())
    {
        changes_dest = ToChangesChroot(dest);
    }

    if (attr.copy.preserve &&
        (   attr.copy.servers == NULL
         || strcmp(RlistScalarValue(attr.copy.servers), "localhost") == 0))
    {
        bool change = false;
        if (!CopyFileExtendedAttributesDisk(changes_src, changes_dest, &change))
        {
            RecordFailure(ctx, pp, &attr,
                          "Could not preserve extended attributes"
                          " (ACLs and security contexts) on file '%s'",
                          dest);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
        else if (change)
        {
            RecordChange(ctx, pp, &attr,
                         "Copied extended attributes from '%s' to '%s'",
                         src, dest);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
    }
    free(chrooted_src);

    return result;
}

static PromiseResult CopyFileSources(EvalContext *ctx, char *destination, const Attributes *attr, const Promise *pp, AgentConnection *conn)
{
    assert(attr != NULL);
    Buffer *source_buf = BufferNew();
    // Expand this.promiser
    ExpandScalar(ctx,
                 PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name,
                 attr->copy.source, source_buf);
    char vbuff[CF_BUFSIZE];
    struct stat ssb, dsb;
    struct timespec start;

    const char *source = BufferData(source_buf);
    const char *changes_source = source;
    if (ChrootChanges())
    {
        changes_source = ToChangesChroot(changes_source);
    }
    if (conn != NULL && (!conn->authenticated))
    {
        RecordFailure(ctx, pp, attr, "Source '%s' not authenticated in copy_from for '%s'",
                      source, destination);
        BufferDestroy(source_buf);
        return PROMISE_RESULT_FAIL;
    }

    if (cf_stat(changes_source, &ssb, &(attr->copy), conn) == -1)
    {
        if (attr->copy.missing_ok)
        {
            RecordNoChange(ctx, pp, attr,
                           "Can't stat file '%s' on '%s' but promise is kept because of"
                           " 'missing_ok' in files.copy_from promise",
                           source, conn ? conn->remoteip : "localhost");
            BufferDestroy(source_buf);
            return PROMISE_RESULT_NOOP;
        }
        else
        {
            RecordFailure(ctx, pp, attr,
                          "Can't stat file '%s' on '%s' in files.copy_from promise",
                          source, conn ? conn->remoteip : "localhost");
            BufferDestroy(source_buf);
            return PROMISE_RESULT_FAIL;
        }
    }

    start = BeginMeasure();

    strlcpy(vbuff, destination, CF_BUFSIZE - 3);

    if (S_ISDIR(ssb.st_mode))   /* could be depth_search */
    {
        AddSlash(vbuff);
        strcat(vbuff, ".");
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    bool dir_created = false;
    if (!MakeParentDirectoryForPromise(ctx, pp, attr, &result, vbuff,
                                       attr->move_obstructions, &dir_created,
                                       DEFAULTMODE))
    {
        BufferDestroy(source_buf);
        return result;
    }
    if (dir_created)
    {
        RecordChange(ctx, pp, attr, "Created parent directory for '%s'", destination);
        result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
    }

    CompressedArray *inode_cache = NULL;
    if (S_ISDIR(ssb.st_mode))   /* could be depth_search */
    {
        if (attr->copy.purge)
        {
            Log(LOG_LEVEL_VERBOSE, "Destination purging enabled");
        }

        Log(LOG_LEVEL_VERBOSE, "Entering directory '%s'", source);

        result = PromiseResultUpdate(
            result, SourceSearchAndCopy(ctx, source, destination,
                                        attr->recursion.depth, attr, pp,
                                        ssb.st_dev, &inode_cache, conn));

        if (stat(ToChangesPath(destination), &dsb) != -1)
        {
            if (attr->copy.check_root)
            {
                result = PromiseResultUpdate(
                    result, VerifyCopiedFileAttributes(ctx, source,
                                                       destination, &ssb, &dsb,
                                                       attr, pp));
            }
        }
    }
    else
    {
        result = PromiseResultUpdate(
            result, VerifyCopy(ctx, source, destination,
                               attr, pp, &inode_cache, conn));
    }

    DeleteCompressedArray(inode_cache);

    const char *mid = PromiseGetConstraintAsRval(pp, "measurement_class", RVAL_TYPE_SCALAR);
    if (mid)
    {
        char eventname[CF_BUFSIZE];
        snprintf(eventname, CF_BUFSIZE - 1, "Copy(%s:%s > %s)",
                 conn ? conn->this_server : "localhost",
                 source, destination);

        EndMeasure(eventname, start);
    }
    else
    {
        EndMeasure(NULL, start);
    }

    BufferDestroy(source_buf);
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
                                               const FileCopy *fc, bool background)
{
    ConnectionFlags flags = {
        .protocol_version = DecideProtocol(ctx, fc->protocol_version),
        .cache_connection = !background,
        .force_ipv4 = fc->force_ipv4,
        .trust_server = fc->trustkey,
        .off_the_record = false
    };

    unsigned int conntimeout = fc->timeout;
    if (fc->timeout == CF_NOINT || fc->timeout < 0)
    {
        conntimeout = CONNTIMEOUT;
    }

    const char *port = (fc->port != NULL) ? fc->port : CFENGINE_PORT_STR;

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
            conn = ServerConnection(servername, port, EvalContextGetRestrictKeys(ctx), conntimeout,
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
        conn = ServerConnection(servername, port, EvalContextGetRestrictKeys(ctx), conntimeout,
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

PromiseResult ScheduleCopyOperation(EvalContext *ctx, char *destination, const Attributes *attr, const Promise *pp)
{
    assert(attr != NULL);
    /* TODO currently parser allows body copy_from to have no source!
       See tests/acceptance/10_files/02_maintain/017.cf and
       https://cfengine.com/bugtracker/view.php?id=687 */
    if (attr->copy.source == NULL)
    {
        RecordFailure(ctx, pp, attr, "Body copy_from for '%s' has no source", destination);
        return PROMISE_RESULT_FAIL;
    }

    Log(LOG_LEVEL_VERBOSE, "File '%s' copy_from '%s'",
        destination, attr->copy.source);

    /* Empty attr->copy.servers means copy from localhost. */
    bool copyfrom_localhost = (attr->copy.servers == NULL);
    AgentConnection *conn = NULL;
    Rlist *rp = attr->copy.servers;

    /* Iterate over all copy_from servers until connection succeeds. */
    while (rp != NULL && conn == NULL)
    {
        const char *servername = RlistScalarValue(rp);

        if (strcmp(servername, "localhost") == 0)
        {
            copyfrom_localhost = true;
            break;
        }

        conn = FileCopyConnectionOpen(ctx, servername, &(attr->copy),
                                      attr->transaction.background);
        if (conn == NULL)
        {
            Log(LOG_LEVEL_INFO, "Unable to establish connection to '%s'",
                servername);
        }

        rp = rp->next;
    }

    if (!copyfrom_localhost && conn == NULL)
    {
        RecordFailure(ctx, pp, attr, "No suitable server found for '%s'", destination);
        PromiseRef(LOG_LEVEL_ERR, pp);
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

PromiseResult ScheduleLinkOperation(EvalContext *ctx, char *destination, char *source, const Attributes *attr, const Promise *pp)
{
    assert(attr != NULL);
    const char *lastnode;

    lastnode = ReadLastNode(destination);
    PromiseResult result = PROMISE_RESULT_NOOP;

    if (MatchRlistItem(ctx, attr->link.copy_patterns, lastnode))
    {
        Log(LOG_LEVEL_VERBOSE, "Link '%s' matches copy_patterns", destination);
        CompressedArray *inode_cache = NULL;
        result = PromiseResultUpdate(result, VerifyCopy(ctx, attr->link.source, destination, attr, pp, &inode_cache, NULL));
        DeleteCompressedArray(inode_cache);
        return result;
    }

    switch (attr->link.link_type)
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
        assert(false);
        Log(LOG_LEVEL_ERR, "Unknown link type - should not happen.");
        break;
    }

    return result;
}

PromiseResult ScheduleLinkChildrenOperation(EvalContext *ctx, char *destination, char *source, int recurse, const Attributes *a,
                                            const Promise *pp)
{
    assert(a != NULL);
    Attributes attr = *a; // TODO: Remove this copy
    Dir *dirh;
    const struct dirent *dirp;
    char promiserpath[CF_BUFSIZE], sourcepath[CF_BUFSIZE];
    struct stat lsb;
    int ret;

    const char *changes_destination = destination;
    if (ChrootChanges())
    {
        changes_destination = ToChangesChroot(destination);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if ((ret = lstat(changes_destination, &lsb)) != -1)
    {
        if (attr.move_obstructions && S_ISLNK(lsb.st_mode))
        {
            if (unlink(changes_destination) == 0)
            {
                RecordChange(ctx, pp, a, "Removed obstructing link '%s'", destination);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                RecordFailure(ctx, pp, a, "Failed to remove obstructing link '%s'", destination);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            }
        }
        else if (!S_ISDIR(lsb.st_mode))
        {
            RecordFailure(ctx, pp, a,
                          "Cannot promise to link files to children of '%s' as it is not a directory",
                          destination);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            return result;
        }
    }

    snprintf(promiserpath, sizeof(promiserpath), "%s/.", destination);

    if ((ret == -1) && !CfCreateFile(ctx, promiserpath, pp, &attr, &result))
    {
        RecordFailure(ctx, pp, a,
                      "Failed to create directory '%s' for linking files as its children",
                      destination);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        return result;
    }

    if (ChrootChanges())
    {
        /* If making changes in a chroot, we need to make sure 'source' is in
         * the chroot before we start. */
        PrepareChangesChroot(source);
    }

    if ((dirh = DirOpen(ToChangesPath(source))) == NULL)
    {
        RecordFailure(ctx, pp, a,
                      "Can't open source of children to link '%s'. (opendir: %s)",
                      attr.link.source, GetErrorStr());
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        return result;
    }

    for (dirp = DirRead(dirh); dirp != NULL; dirp = DirRead(dirh))
    {
        if (!ConsiderLocalFile(dirp->d_name, source))
        {
            Log(LOG_LEVEL_VERBOSE, "Skipping '%s'", dirp->d_name);
            continue;
        }

        /* Assemble pathnames */

        strlcpy(promiserpath, destination, sizeof(promiserpath));
        if (JoinPaths(promiserpath, sizeof(promiserpath), dirp->d_name) == NULL)
        {
            RecordInterruption(ctx, pp, a,
                              "Internal buffer limit while verifying child links,"
                              " promiser: '%s' + '%s'",
                              promiserpath, dirp->d_name);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            DirClose(dirh);
            return result;
        }

        strlcpy(sourcepath, source, sizeof(sourcepath));
        if (JoinPaths(sourcepath, sizeof(sourcepath), dirp->d_name) == NULL)
        {
            RecordInterruption(ctx, pp, a,
                               "Internal buffer limit while verifying child links,"
                               " source filename: '%s' + '%s'",
                               sourcepath, dirp->d_name);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            DirClose(dirh);
            return result;
        }

        if ((lstat(ToChangesPath(promiserpath), &lsb) != -1) && !S_ISLNK(lsb.st_mode) && !S_ISDIR(lsb.st_mode))
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

        if ((attr.recursion.depth > recurse) && (lstat(ToChangesPath(sourcepath), &lsb) != -1) && S_ISDIR(lsb.st_mode))
        {
            result = PromiseResultUpdate(result,
                                         ScheduleLinkChildrenOperation(ctx, promiserpath, sourcepath,
                                                                       recurse + 1, &attr, pp));
        }
        else
        {
            result = PromiseResultUpdate(result, ScheduleLinkOperation(ctx, promiserpath, sourcepath,
                                                                       &attr, pp));
        }
        if (ChrootChanges() && (result == PROMISE_RESULT_CHANGE))
        {
            RecordFileChangedInChroot(promiserpath);
        }
    }

    DirClose(dirh);
    return result;
}

static PromiseResult VerifyFileIntegrity(EvalContext *ctx, const char *file, const Attributes *attr, const Promise *pp)
{
    assert(attr != NULL);
    unsigned char digest1[EVP_MAX_MD_SIZE + 1];
    unsigned char digest2[EVP_MAX_MD_SIZE + 1];

    if ((attr->change.report_changes != FILE_CHANGE_REPORT_CONTENT_CHANGE) && (attr->change.report_changes != FILE_CHANGE_REPORT_ALL))
    {
        return PROMISE_RESULT_NOOP;
    }

    memset(digest1, 0, EVP_MAX_MD_SIZE + 1);
    memset(digest2, 0, EVP_MAX_MD_SIZE + 1);

    PromiseResult result = PROMISE_RESULT_NOOP;
    bool changed = false;
    if (attr->change.hash == HASH_METHOD_BEST)
    {
        HashFile(file, digest1, HASH_METHOD_MD5, false);
        HashFile(file, digest2, HASH_METHOD_SHA1, false);

        changed = (changed ||
                   FileChangesCheckAndUpdateHash(ctx, file, digest1, HASH_METHOD_MD5, attr, pp, &result));
        changed = (changed ||
                   FileChangesCheckAndUpdateHash(ctx, file, digest2, HASH_METHOD_SHA1, attr, pp, &result));
    }
    else
    {
        HashFile(file, digest1, attr->change.hash, false);

        changed = (changed ||
                   FileChangesCheckAndUpdateHash(ctx, file, digest1, attr->change.hash, attr, pp, &result));
    }

    if (changed && MakingInternalChanges(ctx, pp, attr, &result, "record integrity changes in '%s'", file))
    {
        EvalContextHeapPersistentSave(ctx, "checksum_alerts", CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE, "");
        EvalContextClassPutSoft(ctx, "checksum_alerts", CONTEXT_SCOPE_NAMESPACE, "");
        if (FileChangesLogChange(file, FILE_STATE_CONTENT_CHANGED, "Content changed", pp))
        {
            RecordChange(ctx, pp, attr, "Recorded integrity changes in '%s'", file);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
        else
        {
            RecordFailure(ctx, pp, attr, "Failed to record integrity changes in '%s'", file);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
    }

    if (attr->change.report_diffs && MakingInternalChanges(ctx, pp, attr, &result, "report diffs in '%s'", file))
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

static bool CompareForFileCopy(char *sourcefile, char *destfile, const struct stat *ssb, const struct stat *dsb, const FileCopy *fc, AgentConnection *conn)
{
    bool ok_to_copy;

    /* (conn == NULL) means copy is from localhost. */
    const char *changes_sourcefile = sourcefile;
    char *chrooted_sourcefile = NULL;
    if ((conn == NULL) && ChrootChanges())
    {
        /* Need to create a copy because the later call to ToChangesChroot()
         * overwrites the internal buffer. */
        chrooted_sourcefile = xstrdup(ToChangesChroot(sourcefile));
        struct stat sb;
        if (lstat(changes_sourcefile, &sb) != -1)
        {
            changes_sourcefile = chrooted_sourcefile;
        }
    }
    const char *changes_destfile = destfile;
    if (ChrootChanges())
    {
        changes_destfile = ToChangesChroot(destfile);
    }

    switch (fc->compare)
    {
    case FILE_COMPARATOR_CHECKSUM:
    case FILE_COMPARATOR_HASH:

        if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
        {
            ok_to_copy = CompareFileHashes(changes_sourcefile, changes_destfile, ssb, dsb, fc, conn);
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
            free(chrooted_sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_BINARY:

        if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
        {
            ok_to_copy = CompareBinaryFiles(changes_sourcefile, changes_destfile, ssb, dsb, fc, conn);
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
            free(chrooted_sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_MTIME:

        ok_to_copy = (dsb->st_mtime < ssb->st_mtime);

        if (ok_to_copy)
        {
            Log(LOG_LEVEL_VERBOSE, "Image file '%s' out of date, should be copy of '%s'", destfile, sourcefile);
            free(chrooted_sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_ATIME:

        ok_to_copy = (dsb->st_ctime < ssb->st_ctime) ||
            (dsb->st_mtime < ssb->st_mtime) || (CompareBinaryFiles(changes_sourcefile, changes_destfile, ssb, dsb, fc, conn));

        if (ok_to_copy)
        {
            Log(LOG_LEVEL_VERBOSE, "Image file '%s' seems out of date, should be copy of '%s'", destfile, sourcefile);
            free(chrooted_sourcefile);
            return ok_to_copy;
        }
        break;

    default:
        ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);

        if (ok_to_copy)
        {
            Log(LOG_LEVEL_VERBOSE, "Image file '%s' out of date, should be copy of '%s'", destfile, sourcefile);
            free(chrooted_sourcefile);
            return ok_to_copy;
        }
        break;
    }

    free(chrooted_sourcefile);
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
    ToLowerStrInplace(filename);
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
        ToLowerStrInplace(filename);
        MapName(filename);

        PurgeItemList(&VSETXIDLIST, "SETUID/SETGID");

        Item *current = RawLoadItemList(filename);
        if (!ListsCompare(VSETXIDLIST, current))
        {
            mode_t oldmode = umask(077); // This setxidlist file must only be accesible by root
            Log(LOG_LEVEL_DEBUG,
                "Updating setxidlist at '%s', umask was %o, will create setxidlist using umask 0077, file perms should be 0600.",
                filename,
                oldmode);
            RawSaveItemList(VSETXIDLIST, filename, NewLineMode_Unix);
            umask(oldmode);
            Log(LOG_LEVEL_DEBUG, "Restored umask to %o", oldmode);
        }
        DeleteItemList(current);
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

static PromiseResult VerifySetUidGid(EvalContext *ctx, const char *file, const struct stat *dstat, mode_t newperm,
                                     const Promise *pp, const Attributes *attr)
{
    assert(attr != NULL);
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
                    Log(LOG_LEVEL_NOTICE, "NEW SETUID root PROGRAM '%s' ", file);
                }

                PrependItem(&VSETXIDLIST, file, NULL);
                setxid_modified = true;
            }
        }
        else if (MakingChanges(ctx, pp, attr, &result, "remove setuid (root) flag from '%s'", file))
        {
            RecordChange(ctx, pp, attr, "Removed setuid (root) flag from '%s'", file);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
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
                    Log(LOG_LEVEL_NOTICE, "NEW SETGID root PROGRAM '%s' ", file);
                }

                PrependItem(&VSETXIDLIST, file, NULL);
                setxid_modified = true;
            }
        }
        else if (MakingChanges(ctx, pp, attr, &result, "remove setgid (root) flag from '%s'", file))
        {
            RecordChange(ctx, pp, attr, "Removed setgid (root) flag from '%s'", file);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
    }

    SaveSetxid(setxid_modified);

    return result;
}
#endif

#ifdef __APPLE__

static int VerifyFinderType(EvalContext *ctx, const char *file, const Attributes *a, const Promise *pp, PromiseResult *result)
{                               /* Code modeled after hfstar's extract.c */
    assert(a != NULL);
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

    const char *changes_file = file;
    if (ChrootChanges())
    {
        changes_file = ToChangesChroot(file);
    }

    if (a->perms.findertype == NULL)
    {
        return 0;
    }

    Log(LOG_LEVEL_DEBUG, "VerifyFinderType of '%s' for '%s'", file, a->perms.findertype);

    if (strncmp(a->perms.findertype, "*", CF_BUFSIZE) == 0 || strncmp(a->perms.findertype, "", CF_BUFSIZE) == 0)
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

    getattrlist(changes_file, &attrs, &fndrInfo, sizeof(fndrInfo), 0);

    if (fndrInfo.fi.fdType != *(long *) a->perms.findertype)
    {
        fndrInfo.fi.fdType = *(long *) a->perms.findertype;

        if (MakingChanges(ctx, pp, a, result,
                          "set Finder Type code of '%s' to '%s'", file, a->perms.findertype))
        {
            /* setattrlist does not take back in the long ssize */
            retval = setattrlist(changes_file, &attrs, &fndrInfo.created, 4 * sizeof(struct timespec) + sizeof(FInfo), 0);

            Log(LOG_LEVEL_DEBUG, "CheckFinderType setattrlist returned '%d'", retval);

            if (retval >= 0)
            {
                RecordChange(ctx, pp, a, "Set Finder Type code of '%s' to '%s'", file, a->perms.findertype);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                RecordFailure(ctx, pp, a, "Setting Finder Type code of '%s' to '%s' failed",
                              file, a->perms.findertype);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            }

            return retval;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        RecordNoChange(ctx, pp, a, "Finder Type code of '%s' to '%s' is as promised", file, a->perms.findertype);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_NOOP);
        return 0;
    }
}

#endif

static void TruncateFile(const char *name)
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
            Log(LOG_LEVEL_ERR, "Failed to create or truncate file '%s'. (create: %s)", name, GetErrorStr());
        }
        else
        {
            close(fd);
        }
    }
}

static void RegisterAHardLink(int i, const char *value, EvalContext *ctx, const Promise *pp,
                              const Attributes *attr, PromiseResult *result,
                              CompressedArray **inode_cache)
{
    if (!FixCompressedArrayValue(i, value, inode_cache))
    {
        /* Not root hard link, remove to preserve consistency */
        if (MakingChanges(ctx, pp, attr, result, "remove old hard link '%s' to preserve structure",
                          value))
        {
            RecordChange(ctx, pp, attr, "Removed old hard link '%s' to preserve structure", value);
            unlink(value);
        }
    }
}

static int cf_stat(const char *file, struct stat *buf, const FileCopy *fc, AgentConnection *conn)
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
        assert(fc->servers != NULL &&
               strcmp(RlistScalarValue(fc->servers), "localhost") != 0);

        return cf_remote_stat(conn, fc->encrypt, file, buf, "file");
    }
}

#ifndef __MINGW32__

static int cf_readlink(EvalContext *ctx, const char *sourcefile, char *linkbuf, size_t buffsize,
                       const Attributes *attr, const Promise *pp, AgentConnection *conn, PromiseResult *result)
 /* wrapper for network access */
{
    assert(buffsize > 0);
    assert(attr != NULL);
    memset(linkbuf, 0, buffsize);

    if (conn == NULL)
    {
        return readlink(sourcefile, linkbuf, buffsize - 1);
    }
    assert(attr->copy.servers &&
           strcmp(RlistScalarValue(attr->copy.servers), "localhost"));

    const Stat *sp = StatCacheLookup(conn, sourcefile,
                                     RlistScalarValue(attr->copy.servers));

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

static bool SkipDirLinks(EvalContext *ctx, const char *path, const char *lastnode, DirectoryRecursion r)
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

bool VerifyOwner(EvalContext *ctx, const char *file, const Promise *pp, const Attributes *attr, const struct stat *sb, PromiseResult *result)
{
    assert(sb != NULL);

    UidList *ulp;
    GidList *glp;

    /* The groups to change ownership to, using lchown(uid,gid). */
    uid_t uid = CF_UNKNOWN_OWNER;                       /* just init values */
    gid_t gid = CF_UNKNOWN_GROUP;

    /* SKIP if file is already owned by anyone of the promised owners. */
    for (ulp = attr->perms.owners; ulp != NULL; ulp = ulp->next)
    {
        if (ulp->uid == CF_SAME_OWNER)        /* "same" matches anything */
        {
            uid = CF_SAME_OWNER;
            break;
        }
        if (sb->st_uid == ulp->uid)
        {
            RecordNoChange(ctx, pp, attr, "Owner of '%s' as promised (%ju)", file, (uintmax_t) ulp->uid);
            uid = CF_SAME_OWNER;
            break;
        }
    }

    if (uid != CF_SAME_OWNER)
    {
        /* Change ownership to the first known user in the promised list. */
        for (ulp = attr->perms.owners; ulp != NULL; ulp = ulp->next)
        {
            if (ulp->uid != CF_UNKNOWN_OWNER)
            {
                uid = ulp->uid;
                break;
            }
        }
        if (ulp == NULL)
        {
            RecordFailure(ctx, pp, attr,
                          "None of the promised owners for '%s' exist -- see INFO logs for more",
                          file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            uid = CF_SAME_OWNER;      /* chown(-1) doesn't change ownership */
        }
    }
    assert(uid != CF_UNKNOWN_OWNER);

    /* SKIP if file is already group owned by anyone of the promised groups. */
    for (glp = attr->perms.groups; glp != NULL; glp = glp->next)
    {
        if (glp->gid == CF_SAME_GROUP)
        {
            gid = CF_SAME_GROUP;
            break;
        }
        if (sb->st_gid == glp->gid)
        {
            RecordNoChange(ctx, pp, attr, "Group of '%s' as promised (%ju)", file, (uintmax_t) glp->gid);
            gid = CF_SAME_GROUP;
            break;
        }
    }

    /* Change group ownership to the first known group in the promised list. */
    if (gid != CF_SAME_GROUP)
    {
        for (glp = attr->perms.groups; glp != NULL; glp = glp->next)
        {
            if (glp->gid != CF_UNKNOWN_GROUP)
            {
                gid = glp->gid;
                break;
            }
        }
        if (glp == NULL)
        {
            RecordFailure(ctx, pp, attr,
                          "None of the promised groups for '%s' exist -- see INFO logs for more",
                          file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            gid = CF_SAME_GROUP;      /* chown(-1) doesn't change ownership */
        }
    }
    assert(gid != CF_UNKNOWN_GROUP);

    if ((uid == CF_SAME_OWNER) && (gid == CF_SAME_GROUP))
    {
        /* Owner and group as promised or unspecified, nothing to do. */
        return false;
    }

    /* else */
    char name[CF_SMALLBUF];
    if (!GetUserName(sb->st_uid, name, sizeof(name), LOG_LEVEL_VERBOSE))
    {
        RecordWarning(ctx, pp, attr,
                      "File '%s' is not owned by anybody in the passwd database (uid = %ju)",
                      file, (uintmax_t)sb->st_uid);
    }

    if (!GetGroupName(sb->st_gid, name, sizeof(name), LOG_LEVEL_VERBOSE))
    {
        RecordWarning(ctx, pp, attr, "File '%s' is not owned by any group in group database  (gid = %ju)",
                      file, (uintmax_t)sb->st_gid);
    }

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

    if ((uid != CF_SAME_OWNER) && (gid != CF_SAME_GROUP) &&
        !MakingChanges(ctx, pp, attr, result, "change owner and group of '%s' to '%ju:%ju'",
                       file, (uintmax_t) uid, (uintmax_t) gid))
    {
        return false;
    }
    else if ((uid != CF_SAME_OWNER) &&
             !MakingChanges(ctx, pp, attr, result, "change owner of '%s' to '%ju'",
                            file, (uintmax_t) uid))
    {
        return false;
    }
    else if ((gid != CF_SAME_GROUP) &&
             !MakingChanges(ctx, pp, attr, result, "change group of '%s' to '%ju'",
                            file, (uintmax_t) gid))
    {
        return false;
    }

    const char *changes_file = file;
    if (ChrootChanges())
    {
        changes_file = ToChangesChroot(file);
    }

    if (S_ISLNK(sb->st_mode))
    {
# ifdef HAVE_LCHOWN
        Log(LOG_LEVEL_DEBUG, "Using lchown function");
        if (safe_lchown(changes_file, uid, gid) == -1)
        {
            RecordFailure(ctx, pp, attr, "Cannot set ownership on link '%s'. (lchown: %s)",
                          file, GetErrorStr());
        }
        else
        {
            if (uid != CF_SAME_OWNER)
            {
                RecordChange(ctx, pp, attr, "Owner of link '%s' was %ju, set to %ju",
                             file, (uintmax_t) sb->st_uid, (uintmax_t) uid);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }

            if (gid != CF_SAME_GROUP)
            {
                RecordChange(ctx, pp, attr, "Group of link '%s' was %ju, set to %ju",
                             file, (uintmax_t)sb->st_gid, (uintmax_t)gid);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
        }
# endif
    }
    else
    {
        if (safe_chown(changes_file, uid, gid) == -1)
        {
            RecordDenial(ctx, pp, attr, "Cannot set ownership on file '%s'. (chown: %s)",
                         file, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_DENIED);
        }
        else
        {
            if (uid != CF_SAME_OWNER)
            {
                RecordChange(ctx, pp, attr, "Owner of '%s' was %ju, set to %ju",
                             file, (uintmax_t) sb->st_uid, (uintmax_t) uid);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }

            if (gid != CF_SAME_GROUP)
            {
                RecordChange(ctx, pp, attr, "Group of '%s' was %ju, set to %ju",
                             file, (uintmax_t)sb->st_gid, (uintmax_t)gid);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
        }
    }
    return false;
}

#endif /* !__MINGW32__ */

static void VerifyFileChanges(EvalContext *ctx, const char *file, const struct stat *sb,
                              const Attributes *attr, const Promise *pp, PromiseResult *result)
{
    if ((attr->change.report_changes != FILE_CHANGE_REPORT_STATS_CHANGE) && (attr->change.report_changes != FILE_CHANGE_REPORT_ALL))
    {
        return;
    }

    FileChangesCheckAndUpdateStats(ctx, file, sb, attr->change.update, attr, pp, result);
}

bool CfCreateFile(EvalContext *ctx, char *file, const Promise *pp, const Attributes *attr, PromiseResult *result)
{
    assert(attr != NULL);
    if (!IsAbsoluteFileName(file))
    {
        RecordFailure(ctx, pp, attr,
                      "Cannot create a relative filename '%s' - has no invariant meaning. (create: %s)",
                      file, GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

    const char *changes_file = file;
    if (ChrootChanges())
    {
        changes_file = ToChangesChroot(file);
    }

    /* If name ends in /., or depthsearch is set, then this is a directory */
    bool is_dir = attr->havedepthsearch || (strcmp(".", ReadLastNode(file)) == 0);
    if (is_dir)
    {
        Log(LOG_LEVEL_DEBUG, "File object '%s' seems to be a directory", file);

        if (MakingChanges(ctx, pp, attr, result, "create directory '%s'", file))
        {
            mode_t filemode = DEFAULTMODE;
            if (PromiseGetConstraintAsRval(pp, "mode", RVAL_TYPE_SCALAR) == NULL)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "No mode was set, choosing directory default %04jo",
                    (uintmax_t) filemode);
            }
            else
            {
                filemode = attr->perms.plus & ~(attr->perms.minus);
            }

            bool dir_created = false;
            if (!MakeParentDirectoryForPromise(ctx, pp, attr, result, file,
                                               attr->move_obstructions,
                                               &dir_created, filemode))
            {
                return false;
            }
            if (dir_created)
            {
                RecordChange(ctx, pp, attr,
                             "Created directory '%s', mode %04jo", file,
                             (uintmax_t) filemode);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
        }
        else
        {
            return false;
        }
    }
    else if (attr->file_type && !strncmp(attr->file_type, "fifo", 5))
    {
#ifndef _WIN32
        mode_t filemode = 0600;
        if (PromiseGetConstraintAsRval(pp, "mode", RVAL_TYPE_SCALAR) == NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "No mode was set, choose plain file default %04jo", (uintmax_t)filemode);
        }
        else
        {
            filemode = attr->perms.plus & ~(attr->perms.minus);
        }

        bool dir_created = false;
        if (!MakeParentDirectoryForPromise(ctx, pp, attr, result, file,
                                           attr->move_obstructions,
                                           &dir_created, DEFAULTMODE))
        {
            return false;
        }
        if (dir_created)
        {
            RecordChange(ctx, pp, attr, "Created directory for '%s'", file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }

        if (MakingChanges(ctx, pp, attr, result, "create named pipe '%s', mode %04jo",
                          file, (uintmax_t) filemode))
        {
            mode_t saveumask = umask(0);
            char errormsg[CF_BUFSIZE];
            if (mkfifo(changes_file, filemode) != 0)
            {
                snprintf(errormsg, sizeof(errormsg), "(mkfifo: %s)", GetErrorStr());
                RecordFailure(ctx, pp, attr, "Error creating file '%s', mode '%04jo'. %s",
                              file, (uintmax_t)filemode, errormsg);
                umask(saveumask);
                return false;
            }
            RecordChange(ctx, pp, attr, "Created named pipe '%s', mode %04jo", file, (uintmax_t) filemode);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

            umask(saveumask);
            return true;
        }
        else
        {
            return false;
        }
#else
        RecordWarning(ctx, pp, attr, "Named pipe creation not supported on Windows");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return false;
#endif
    }
    else
    {
        mode_t filemode = 0600;     /* Decide the mode for filecreation */
        if (PromiseGetConstraintAsRval(pp, "mode", RVAL_TYPE_SCALAR) == NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "No mode was set, choose plain file default %04jo", (uintmax_t)filemode);
        }
        else
        {
            filemode = attr->perms.plus & ~(attr->perms.minus);
        }

        bool dir_created = false;
        if (!MakeParentDirectoryForPromise(ctx, pp, attr, result, file,
                                           attr->move_obstructions,
                                           &dir_created, DEFAULTMODE))
        {
            return false;
        }
        if (dir_created)
        {
            RecordChange(ctx, pp, attr, "Created directory for '%s'", file);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        }

        if (MakingChanges(ctx, pp, attr, result, "create file '%s', mode '%04jo'",
                          file, (uintmax_t)filemode))
        {
            mode_t saveumask = umask(0);

            int fd = safe_open_create_perms(changes_file, O_WRONLY | O_CREAT | O_EXCL, filemode);
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
                RecordFailure(ctx, pp, attr, "Error creating file '%s', mode '%04jo'. %s",
                              file, (uintmax_t)filemode, errormsg);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                umask(saveumask);
                return false;
            }
            else
            {
                RecordChange(ctx, pp, attr, "Created file '%s', mode %04jo", file, (uintmax_t)filemode);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                close(fd);
                umask(saveumask);
                return true;
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

static bool DeviceBoundary(const struct stat *sb, dev_t rootdevice)
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
