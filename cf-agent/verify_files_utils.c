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

#include "verify_files_utils.h"

#include "cfstream.h"
#include "dir.h"
#include "files_names.h"
#include "files_links.h"
#include "files_copy.h"
#include "files_properties.h"
#include "transaction.h"
#include "instrumentation.h"
#include "matching.h"
#include "files_interfaces.h"
#include "promises.h"
#include "files_operators.h"
#include "item_lib.h"
#include "client_code.h"
#include "logging.h"
#include "files_hashes.h"
#include "files_repository.h"
#include "expand.h"
#include "conversion.h"
#include "pipes.h"
#include "cf_acl.h"
#include "env_context.h"
#include "vars.h"
#include "exec_tools.h"
#include "comparray.h"
#include "string_lib.h"
#include "files_lib.h"
#include "rlist.h"
#include "policy.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

#define CF_RECURSION_LIMIT 100

static Rlist *AUTO_DEFINE_LIST;

Item *VSETUIDLIST;

Rlist *SINGLE_COPY_LIST = NULL;
static Rlist *SINGLE_COPY_CACHE = NULL;

static int TransformFile(char *file, Attributes attr, Promise *pp);
static void VerifyName(char *path, struct stat *sb, Attributes attr, Promise *pp);
static void VerifyDelete(char *path, struct stat *sb, Attributes attr, Promise *pp);
static void VerifyCopy(char *source, char *destination, Attributes attr, Promise *pp);
static void TouchFile(char *path, struct stat *sb, Attributes attr, Promise *pp);
static void VerifyFileAttributes(char *file, struct stat *dstat, Attributes attr, Promise *pp);
static int PushDirState(char *name, struct stat *sb);
static void PopDirState(int goback, char *name, struct stat *sb, Recursion r);
static void CheckLinkSecurity(struct stat *sb, char *name);
static int CompareForFileCopy(char *sourcefile, char *destfile, struct stat *ssb, struct stat *dsb, Attributes attr, Promise *pp);
static void FileAutoDefine(char *destfile, const char *namespace);
static void TruncateFile(char *name);
static void RegisterAHardLink(int i, char *value, Attributes attr, Promise *pp);
static void VerifyCopiedFileAttributes(char *file, struct stat *dstat, struct stat *sstat, Attributes attr, Promise *pp);
static int cf_stat(char *file, struct stat *buf, Attributes attr, Promise *pp);
static int cf_readlink(char *sourcefile, char *linkbuf, int buffsize, Attributes attr, Promise *pp);
static bool CopyRegularFileDiskReport(char *source, char *destination, Attributes attr, Promise *pp);
static int SkipDirLinks(char *path, const char *lastnode, Recursion r);
static int DeviceBoundary(struct stat *sb, Promise *pp);
static void LinkCopy(char *sourcefile, char *destfile, struct stat *sb, Attributes attr, Promise *pp);

#ifndef __MINGW32__
static void VerifySetUidGid(char *file, struct stat *dstat, mode_t newperm, Promise *pp, Attributes attr);
static int VerifyOwner(char *file, Promise *pp, Attributes attr, struct stat *sb);
#endif
#ifdef __APPLE__
static int VerifyFinderType(char *file, struct stat *statbuf, Attributes a, Promise *pp);
#endif
static void VerifyFileChanges(char *file, struct stat *sb, Attributes attr, Promise *pp);
static void VerifyFileIntegrity(char *file, Attributes attr, Promise *pp);

#ifndef HAVE_NOVA
static void LogFileChange(char *file, int change, Attributes a, Promise *pp)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Logging file differences requires version Nova or above");
}
#endif

void SetFileAutoDefineList(Rlist *auto_define_list)
{
    AUTO_DEFINE_LIST = auto_define_list;
}

int VerifyFileLeaf(char *path, struct stat *sb, Attributes attr, Promise *pp)
{
/* Here we can assume that we are in the parent directory of the leaf */

    if (!SelectLeaf(path, sb, attr, pp))
    {
        CfDebug("Skipping non-selected file %s\n", path);
        return false;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Handling file existence constraints on %s\n", path);

/* We still need to augment the scope of context "this" for commands */

    DeleteScalar("this", "promiser");
    NewScalar("this", "promiser", path, DATA_TYPE_STRING);        // Parameters may only be scalars

    if (attr.transformer != NULL)
    {
        if (!TransformFile(path, attr, pp))
        {
            /* NOP? */
        }
    }
    else
    {
        if (attr.haverename)
        {
            VerifyName(path, sb, attr, pp);
        }

        if (attr.havedelete)
        {
            VerifyDelete(path, sb, attr, pp);
        }

        if (attr.touch)
        {
            TouchFile(path, sb, attr, pp);      // intrinsically non-convergent op
        }
    }

    if (attr.haveperms || attr.havechange || attr.acl.acl_entries)
    {
        if (S_ISDIR(sb->st_mode) && attr.recursion.depth && !attr.recursion.include_basedir &&
            (strcmp(path, pp->promiser) == 0))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Promise to skip base directory %s\n", path);
        }
        else
        {
            VerifyFileAttributes(path, sb, attr, pp);
        }
    }

    DeleteScalar("this", "promiser");
    return true;
}

static void CfCopyFile(char *sourcefile, char *destfile, struct stat ssb, Attributes attr, Promise *pp)
{
    char *server;
    const char *lastnode;
    struct stat dsb;
    int found;
    mode_t srcmode = ssb.st_mode;

    CfDebug("CopyFile(%s,%s)\n", sourcefile, destfile);

#ifdef __MINGW32__
    if (attr.copy.copy_links != NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              "copy_from.copylink_patterns is ignored on Windows (source files cannot be symbolic links)");
    }
#endif /* __MINGW32__ */

    attr.link.when_no_file = cfa_force;

    if (attr.copy.servers)
    {
        server = (char *) attr.copy.servers->item;
    }
    else
    {
        server = NULL;
    }

    if ((strcmp(sourcefile, destfile) == 0) && server && (strcmp(server, "localhost") == 0))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", " !! File copy promise loop: file/dir %s is its own source", sourcefile);
        return;
    }

    if (!SelectLeaf(sourcefile, &ssb, attr, pp))
    {
        CfDebug("Skipping non-selected file %s\n", sourcefile);
        return;
    }

    if (RlistIsInListOfRegex(SINGLE_COPY_CACHE, destfile))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", " -> Skipping single-copied file %s\n", destfile);
        return;
    }

    if (attr.copy.link_type != FILE_LINK_TYPE_NONE)
    {
        lastnode = ReadLastNode(sourcefile);

        if (MatchRlistItem(attr.copy.link_instead, lastnode))
        {
            if (MatchRlistItem(attr.copy.copy_links, lastnode))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "",
                      "File %s matches both copylink_patterns and linkcopy_patterns - promise loop (skipping)!",
                      sourcefile);
                return;
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Copy item %s marked for linking\n", sourcefile);
#ifdef __MINGW32__
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Links are not yet supported on Windows - copying %s instead\n", sourcefile);
#else
                LinkCopy(sourcefile, destfile, &ssb, attr, pp);
                return;
#endif
            }
        }
    }

    found = lstat(destfile, &dsb);

    if (found != -1)
    {
        if (((S_ISLNK(dsb.st_mode)) && (attr.copy.link_type == FILE_LINK_TYPE_NONE))
            || ((S_ISLNK(dsb.st_mode)) && (!S_ISLNK(ssb.st_mode))))
        {
            if ((!S_ISLNK(ssb.st_mode)) && ((attr.copy.type_check) && (attr.copy.link_type != FILE_LINK_TYPE_NONE)))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr,
                     "file image exists but destination type is silly (file/dir/link doesn't match)\n");
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                return;
            }

            if (DONTDO)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Need to remove old symbolic link %s to make way for copy\n", destfile);
            }
            else
            {
                if (unlink(destfile) == -1)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "unlink", pp, attr, "Couldn't remove link %s", destfile);
                    return;
                }

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Removing old symbolic link %s to make way for copy\n", destfile);
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
        if ((ssb.st_size < attr.copy.min_size) || (ssb.st_size > attr.copy.max_size))
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, attr, " -> Source file %s size is not in the permitted safety range\n",
                 sourcefile);
            return;
        }
    }

    if (found == -1)
    {
        if (attr.transaction.action == cfa_warn)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! Image file \"%s\" is non-existent and should be a copy of %s\n",
                 destfile, sourcefile);
            return;
        }

        if ((S_ISREG(srcmode)) || ((S_ISLNK(srcmode)) && (attr.copy.link_type == FILE_LINK_TYPE_NONE)))
        {
            if (DONTDO)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> %s wasn't at destination (needs copying)", destfile);
                return;
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> %s wasn't at destination (copying)", destfile);

                if (server)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " -> Copying from %s:%s\n", server, sourcefile);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " -> Copying from localhost:%s\n", sourcefile);
                }
            }

            if ((S_ISLNK(srcmode)) && (attr.copy.link_type != FILE_LINK_TYPE_NONE))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> %s is a symbolic link\n", sourcefile);
                LinkCopy(sourcefile, destfile, &ssb, attr, pp);
            }
            else if (CopyRegularFile(sourcefile, destfile, ssb, dsb, attr, pp))
            {
                if (cfstat(destfile, &dsb) == -1)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "stat", "Can't stat destination file %s\n", destfile);
                }
                else
                {
                    VerifyCopiedFileAttributes(destfile, &dsb, &ssb, attr, pp);
                }

                if (server)
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, attr, " -> Updated file from %s:%s\n", server, sourcefile);
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, attr, " -> Updated file from localhost:%s\n", sourcefile);
                }

                if (SINGLE_COPY_LIST)
                {
                    RlistPrependScalarIdemp(&SINGLE_COPY_CACHE, destfile);
                }

                if (MatchRlistItem(AUTO_DEFINE_LIST, destfile))
                {
                    FileAutoDefine(destfile, pp->ns);
                }
            }
            else
            {
                if (server)
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, " !! Copy from %s:%s failed\n", server, sourcefile);
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, " !! Copy from localhost:%s failed\n", sourcefile);
                }
            }

            return;
        }

        if (S_ISFIFO(srcmode))
        {
#ifdef HAVE_MKFIFO
            if (DONTDO)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", "Need to make FIFO %s\n", destfile);
            }
            else if (mkfifo(destfile, srcmode))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "mkfifo", pp, attr, " !! Cannot create fifo `%s'", destfile);
                return;
            }

            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Created fifo %s", destfile);
#endif
        }
        else
        {
#ifndef __MINGW32__                   // only regular files on windows
            if (S_ISBLK(srcmode) || S_ISCHR(srcmode) || S_ISSOCK(srcmode))
            {
                if (DONTDO)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", "Make BLK/CHR/SOCK %s\n", destfile);
                }
                else if (mknod(destfile, srcmode, ssb.st_rdev))
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "mknod", pp, attr, " !! Cannot create special file `%s'", destfile);
                    return;
                }

                cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "mknod", pp, attr, " -> Created special file/device `%s'", destfile);
            }
#endif /* !__MINGW32__ */
        }

        if ((S_ISLNK(srcmode)) && (attr.copy.link_type != FILE_LINK_TYPE_NONE))
        {
            LinkCopy(sourcefile, destfile, &ssb, attr, pp);
        }
    }
    else
    {
        int ok_to_copy = false;

        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Destination file \"%s\" already exists\n", destfile);

        if (attr.copy.compare == FILE_COMPARATOR_EXISTS)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Existence only is promised, no copying required\n");
            return;
        }

        if (!attr.copy.force_update)
        {
            ok_to_copy = CompareForFileCopy(sourcefile, destfile, &ssb, &dsb, attr, pp);
        }
        else
        {
            ok_to_copy = true;
        }

        if ((attr.copy.type_check) && (attr.copy.link_type != FILE_LINK_TYPE_NONE))
        {
            if (((S_ISDIR(dsb.st_mode)) && (!S_ISDIR(ssb.st_mode))) ||
                ((S_ISREG(dsb.st_mode)) && (!S_ISREG(ssb.st_mode))) ||
                ((S_ISBLK(dsb.st_mode)) && (!S_ISBLK(ssb.st_mode))) ||
                ((S_ISCHR(dsb.st_mode)) && (!S_ISCHR(ssb.st_mode))) ||
                ((S_ISSOCK(dsb.st_mode)) && (!S_ISSOCK(ssb.st_mode))) ||
                ((S_ISFIFO(dsb.st_mode)) && (!S_ISFIFO(ssb.st_mode))) ||
                ((S_ISLNK(dsb.st_mode)) && (!S_ISLNK(ssb.st_mode))))
            {
                cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr,
                     "Promised file copy %s exists but type mismatch with source=%s\n", destfile, sourcefile);
                return;
            }
        }

        if (ok_to_copy && (attr.transaction.action == cfa_warn))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! Image file \"%s\" exists but is not up to date wrt %s\n",
                 destfile, sourcefile);
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! Only a warning has been promised\n");
            return;
        }

        if ((attr.copy.force_update) || ok_to_copy || (S_ISLNK(ssb.st_mode)))       /* Always check links */
        {
            if ((S_ISREG(srcmode)) || (attr.copy.link_type == FILE_LINK_TYPE_NONE))
            {
                if (DONTDO)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Should update file %s from source %s on %s", destfile, sourcefile, server);
                    return;
                }

                if (MatchRlistItem(AUTO_DEFINE_LIST, destfile))
                {
                    FileAutoDefine(destfile, pp->ns);
                }

                if (CopyRegularFile(sourcefile, destfile, ssb, dsb, attr, pp))
                {
                    if (cfstat(destfile, &dsb) == -1)
                    {
                        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "stat", pp, attr, "Can't stat destination %s\n", destfile);
                    }
                    else
                    {
                        char *source_host = server ? server : "localhost";

                        cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Updated %s from source %s on %s", destfile,
                             sourcefile, source_host);

                        VerifyCopiedFileAttributes(destfile, &dsb, &ssb, attr, pp);
                    }

                    if (RlistIsInListOfRegex(SINGLE_COPY_LIST, destfile))
                    {
                        RlistPrependScalarIdemp(&SINGLE_COPY_CACHE, destfile);
                    }
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "Was not able to copy %s to %s\n", sourcefile, destfile);
                }

                return;
            }

            if (S_ISLNK(ssb.st_mode))
            {
                LinkCopy(sourcefile, destfile, &ssb, attr, pp);
            }
        }
        else
        {
            VerifyCopiedFileAttributes(destfile, &dsb, &ssb, attr, pp);

            /* Now we have to check for single copy, even though nothing was copied
               otherwise we can get oscillations between multipe versions if type
               is based on a checksum */

            if (RlistIsInListOfRegex(SINGLE_COPY_LIST, destfile))
            {
                RlistPrependScalarIdemp(&SINGLE_COPY_CACHE, destfile);
            }

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, attr, " -> File %s is an up to date copy of source\n", destfile);
        }
    }
}

static void PurgeLocalFiles(Item *filelist, char *localdir, Attributes attr, Promise *pp)
{
    Dir *dirh;
    struct stat sb;
    const struct dirent *dirp;
    char filename[CF_BUFSIZE] = { 0 };

    CfDebug("PurgeLocalFiles(%s)\n", localdir);

    if (strlen(localdir) < 2)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Purge of %s denied -- too dangerous!", localdir);
        return;
    }

    /* If we purge with no authentication we wipe out EVERYTHING ! */

    if ((pp->conn) && (!pp->conn->authenticated))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Not purge local files %s - no authenticated contact with a source\n", localdir);
        return;
    }

    if (!attr.havedepthsearch)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! No depth search when copying %s so purging does not apply\n", localdir);
        return;
    }

/* chdir to minimize the risk of race exploits during copy (which is inherently dangerous) */

    if (chdir(localdir) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "chdir", "Can't chdir to local directory %s\n", localdir);
        return;
    }

    if ((dirh = OpenDirLocal(".")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "opendir", "Can't open local directory %s\n", localdir);
        return;
    }

    for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
    {
        if (!ConsiderFile(dirp->d_name, localdir, attr, pp))
        {
            continue;
        }

        if (!IsItemIn(filelist, dirp->d_name))
        {
            strncpy(filename, localdir, CF_BUFSIZE - 2);

            AddSlash(filename);

            Join(filename, dirp->d_name, CF_BUFSIZE - 1);

            if (DONTDO)
            {
                printf(" !! Need to purge %s from copy dest directory\n", filename);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " !! Purging %s in copy dest directory\n", filename);

                if (lstat(filename, &sb) == -1)
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "lstat", pp, attr, " !! Couldn't stat %s while purging\n", filename);
                }
                else if (S_ISDIR(sb.st_mode))
                {
                    Attributes purgeattr = { {0} };
                    memset(&purgeattr, 0, sizeof(purgeattr));

                    /* Deletion is based on a files promise */

                    purgeattr.havedepthsearch = true;
                    purgeattr.havedelete = true;
                    purgeattr.delete.dirlinks = cfa_linkdelete;
                    purgeattr.delete.rmdirs = true;
                    purgeattr.recursion.depth = CF_INFINITY;
                    purgeattr.recursion.travlinks = false;
                    purgeattr.recursion.xdev = false;

                    SetSearchDevice(&sb, pp);

                    if (!DepthSearch(filename, &sb, 0, purgeattr, pp))
                    {
                        cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "rmdir", pp, attr,
                             " !! Couldn't empty directory %s while purging\n", filename);
                    }

                    if (chdir("..") != 0)
                    {
                        CfOut(OUTPUT_LEVEL_ERROR, "chdir", "!! Can't step out of directory \"%s\" before deletion", filename);
                    }

                    if (rmdir(filename) == -1)
                    {
                        cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "rmdir", pp, attr,
                             " !! Couldn't remove directory %s while purging\n", filename);
                    }
                }
                else if (unlink(filename) == -1)
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, attr, " !! Couldn't delete %s while purging\n", filename);
                }
            }
        }
    }

    CloseDir(dirh);
}

static void SourceSearchAndCopy(char *from, char *to, int maxrecurse, Attributes attr, Promise *pp)
{
    struct stat sb, dsb;
    char newfrom[CF_BUFSIZE];
    char newto[CF_BUFSIZE];
    Item *namecache = NULL;
    const struct dirent *dirp;
    Dir *dirh;

    if (maxrecurse == 0)        /* reached depth limit */
    {
        CfDebug("MAXRECURSE ran out, quitting at level %s\n", from);
        return;
    }

    CfDebug("RecursiveCopy(%s,%s,lev=%d)\n", from, to, maxrecurse);

    if (strlen(from) == 0)      /* Check for root dir */
    {
        from = "/";
    }

    /* Check that dest dir exists before starting */

    strncpy(newto, to, CF_BUFSIZE - 10);
    AddSlash(newto);
    strcat(newto, "dummy");

    if (attr.transaction.action != cfa_warn)
    {
        struct stat tostat;

        if (!MakeParentDirectory(newto, attr.move_obstructions))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "Unable to make directory for %s in file-copy %s to %s\n", newto,
                 attr.copy.source, attr.copy.destination);
            return;
        }

        DeleteSlash(to);

        /* Set aside symlinks */

        if (lstat(to, &tostat) != 0)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "lstat", pp, attr, "Unable to stat newly created directory %s", to);
            return;
        }

        if (S_ISLNK(tostat.st_mode))
        {
            char backup[CF_BUFSIZE];
            mode_t mask;

            if (!attr.move_obstructions)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", "Path %s is a symlink. Unable to move it aside without move_obstructions is set",
                      to);
                return;
            }

            strcpy(backup, to);
            DeleteSlash(to);
            strcat(backup, ".cf-moved");

            if (cf_rename(to, backup) == -1)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", "Unable to backup old %s", to);
                unlink(to);
            }

            mask = umask(0);
            if (cf_mkdir(to, DEFAULTMODE) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "cf_mkdir", "Unable to make directory %s", to);
                umask(mask);
                return;
            }
            umask(mask);
        }
    }

    if ((dirh = OpenDirForPromise(from, attr, pp)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, "copy can't open directory [%s]\n", from);
        return;
    }

    for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
    {
        if (!ConsiderFile(dirp->d_name, from, attr, pp))
        {
            continue;
        }

        if (attr.copy.purge)    /* Do not purge this file */
        {
            AppendItem(&namecache, dirp->d_name, NULL);
        }

        strncpy(newfrom, from, CF_BUFSIZE - 2); /* Assemble pathname */
        strncpy(newto, to, CF_BUFSIZE - 2);

        if (!JoinPath(newfrom, dirp->d_name))
        {
            CloseDir(dirh);
            return;
        }

        if ((attr.recursion.travlinks) || (attr.copy.link_type == FILE_LINK_TYPE_NONE))
        {
            /* No point in checking if there are untrusted symlinks here,
               since this is from a trusted source, by defintion */

            if (cf_stat(newfrom, &sb, attr, pp) == -1)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "cf_stat", " !! (Can't stat %s)\n", newfrom);
                continue;
            }
        }
        else
        {
            if (cf_lstat(newfrom, &sb, attr, pp) == -1)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "cf_stat", " !! (Can't stat %s)\n", newfrom);
                continue;
            }
        }

        /* If we are tracking subdirs in copy, then join else don't add */

        if (attr.copy.collapse)
        {
            if ((!S_ISDIR(sb.st_mode)) && (!JoinPath(newto, dirp->d_name)))
            {
                CloseDir(dirh);
                return;
            }
        }
        else
        {
            if (!JoinPath(newto, dirp->d_name))
            {
                CloseDir(dirh);
                return;
            }
        }

        if ((attr.recursion.xdev) && (DeviceBoundary(&sb, pp)))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Skipping %s on different device\n", newfrom);
            continue;
        }

        if (S_ISDIR(sb.st_mode))
        {
            if (attr.recursion.travlinks)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Traversing directory links during copy is too dangerous, pruned");
                continue;
            }

            if (SkipDirLinks(newfrom, dirp->d_name, attr.recursion))
            {
                continue;
            }

            memset(&dsb, 0, sizeof(struct stat));

            /* Only copy dirs if we are tracking subdirs */

            if ((!attr.copy.collapse) && (cfstat(newto, &dsb) == -1))
            {
                if (cf_mkdir(newto, 0700) == -1)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "cf_mkdir", pp, attr, " !! Can't make directory %s\n", newto);
                    continue;
                }

                if (cfstat(newto, &dsb) == -1)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "stat", pp, attr,
                         " !! Can't stat local copy %s - failed to establish directory\n", newto);
                    continue;
                }
            }

            CfOut(OUTPUT_LEVEL_VERBOSE, "", " ->>  Entering %s\n", newto);

            if (!attr.copy.collapse)
            {
                VerifyCopiedFileAttributes(newto, &dsb, &sb, attr, pp);
            }

            SourceSearchAndCopy(newfrom, newto, maxrecurse - 1, attr, pp);
        }
        else
        {
            VerifyCopy(newfrom, newto, attr, pp);
        }
    }

    if (attr.copy.purge)
    {
        PurgeLocalFiles(namecache, to, attr, pp);
        DeleteItemList(namecache);
    }

    DeleteCompressedArray(pp->inode_cache);
    pp->inode_cache = NULL;
    CloseDir(dirh);
}

static void VerifyCopy(char *source, char *destination, Attributes attr, Promise *pp)
{
    Dir *dirh;
    char sourcefile[CF_BUFSIZE];
    char sourcedir[CF_BUFSIZE];
    char destdir[CF_BUFSIZE];
    char destfile[CF_BUFSIZE];
    struct stat ssb, dsb;
    const struct dirent *dirp;
    int found;

    CfDebug("VerifyCopy (source=%s destination=%s)\n", source, destination);

    if (attr.copy.link_type == FILE_LINK_TYPE_NONE)
    {
        CfDebug("Treating links as files for %s\n", source);
        found = cf_stat(source, &ssb, attr, pp);
    }
    else
    {
        found = cf_lstat(source, &ssb, attr, pp);
    }

    if (found == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "Can't stat %s in verify copy\n", source);
        DeleteClientCache(attr, pp);
        return;
    }

    if (ssb.st_nlink > 1)       /* Preserve hard link structure when copying */
    {
        RegisterAHardLink(ssb.st_ino, destination, attr, pp);
    }

    if (S_ISDIR(ssb.st_mode))
    {
        strcpy(sourcedir, source);
        AddSlash(sourcedir);
        strcpy(destdir, destination);
        AddSlash(destdir);

        if ((dirh = OpenDirForPromise(sourcedir, attr, pp)) == NULL)
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "opendir", pp, attr, "Can't open directory %s\n", sourcedir);
            DeleteClientCache(attr, pp);
            return;
        }

        /* Now check any overrides */

        if (cfstat(destdir, &dsb) == -1)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "stat", pp, attr, "Can't stat directory %s\n", destdir);
        }
        else
        {
            VerifyCopiedFileAttributes(destdir, &dsb, &ssb, attr, pp);
        }

        for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
        {
            if (!ConsiderFile(dirp->d_name, sourcedir, attr, pp))
            {
                continue;
            }

            strcpy(sourcefile, sourcedir);

            if (!JoinPath(sourcefile, dirp->d_name))
            {
                FatalError("VerifyCopy");
            }

            strcpy(destfile, destdir);

            if (!JoinPath(destfile, dirp->d_name))
            {
                FatalError("VerifyCopy");
            }

            if (attr.copy.link_type == FILE_LINK_TYPE_NONE)
            {
                if (cf_stat(sourcefile, &ssb, attr, pp) == -1)
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "stat", pp, attr, "Can't stat source file (notlinked) %s\n", sourcefile);
                    DeleteClientCache(attr, pp);
                    return;
                }
            }
            else
            {
                if (cf_lstat(sourcefile, &ssb, attr, pp) == -1)
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "lstat", pp, attr, "Can't stat source file %s\n", sourcefile);
                    DeleteClientCache(attr, pp);
                    return;
                }
            }

            CfCopyFile(sourcefile, destfile, ssb, attr, pp);
        }

        CloseDir(dirh);
        DeleteClientCache(attr, pp);
        return;
    }

    strcpy(sourcefile, source);
    strcpy(destfile, destination);

    CfCopyFile(sourcefile, destfile, ssb, attr, pp);
    DeleteClientCache(attr, pp);
}

static void LinkCopy(char *sourcefile, char *destfile, struct stat *sb, Attributes attr, Promise *pp)
/* Link the file to the source, instead of copying */
#ifdef __MINGW32__
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Windows does not support symbolic links");
    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "Windows can't link \"%s\" to \"%s\"", sourcefile, destfile);
}
#else                           /* !__MINGW32__ */
{
    char linkbuf[CF_BUFSIZE];
    const char *lastnode;
    int status = CF_UNKNOWN;
    struct stat dsb;

    linkbuf[0] = '\0';

    if ((S_ISLNK(sb->st_mode)) && (cf_readlink(sourcefile, linkbuf, CF_BUFSIZE, attr, pp) == -1))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "Can't readlink %s\n", sourcefile);
        return;
    }
    else if (S_ISLNK(sb->st_mode))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checking link from %s to %s\n", destfile, linkbuf);

        if ((attr.copy.link_type == FILE_LINK_TYPE_ABSOLUTE) && (!IsAbsoluteFileName(linkbuf)))        /* Not absolute path - must fix */
        {
            char vbuff[CF_BUFSIZE];

            strcpy(vbuff, sourcefile);
            ChopLastNode(vbuff);
            AddSlash(vbuff);
            strncat(vbuff, linkbuf, CF_BUFSIZE - 1);
            strncpy(linkbuf, vbuff, CF_BUFSIZE - 1);
        }
    }
    else
    {
        strcpy(linkbuf, sourcefile);
    }

    lastnode = ReadLastNode(sourcefile);

    if (MatchRlistItem(attr.copy.copy_links, lastnode))
    {
        struct stat ssb;

        ExpandLinks(linkbuf, sourcefile, 0);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "cfengine: link item in copy %s marked for copying from %s instead\n", sourcefile,
              linkbuf);
        cfstat(linkbuf, &ssb);
        CfCopyFile(linkbuf, destfile, ssb, attr, pp);
        return;
    }

    switch (attr.copy.link_type)
    {
    case FILE_LINK_TYPE_SYMLINK:

        if (*linkbuf == '.')
        {
            status = VerifyRelativeLink(destfile, linkbuf, attr, pp);
        }
        else
        {
            status = VerifyLink(destfile, linkbuf, attr, pp);
        }
        break;

    case FILE_LINK_TYPE_RELATIVE:
        status = VerifyRelativeLink(destfile, linkbuf, attr, pp);
        break;

    case FILE_LINK_TYPE_ABSOLUTE:
        status = VerifyAbsoluteLink(destfile, linkbuf, attr, pp);
        break;

    case FILE_LINK_TYPE_HARDLINK:
        status = VerifyHardLink(destfile, linkbuf, attr, pp);
        break;

    default:
        FatalError("LinkCopy software error");
        return;
    }

    if ((status == CF_CHG) || (status == CF_NOP))
    {
        if (lstat(destfile, &dsb) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "lstat", "Can't lstat %s\n", destfile);
        }
        else
        {
            VerifyCopiedFileAttributes(destfile, &dsb, sb, attr, pp);
        }

        if (status == CF_CHG)
        {
            cfPS(OUTPUT_LEVEL_INFORM, status, "", pp, attr, " -> Created link %s", destfile);
        }
        else if (status == CF_NOP)
        {
            cfPS(OUTPUT_LEVEL_INFORM, status, "", pp, attr, " -> Link %s as promised", destfile);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, status, "", pp, attr, " -> Unable to create link %s", destfile);
        }
    }
}
#endif /* !__MINGW32__ */

int CopyRegularFile(char *source, char *dest, struct stat sstat, struct stat dstat, Attributes attr, Promise *pp)
{
    char backup[CF_BUFSIZE];
    char new[CF_BUFSIZE], *linkable;
    AgentConnection *conn = pp->conn;
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

#ifdef WITH_SELINUX
    int selinux_enabled = 0;

/* need to keep track of security context of destination file (if any) */
    security_context_t scontext = NULL;
    struct stat cur_dest;
    int dest_exists;

    selinux_enabled = (is_selinux_enabled() > 0);
#endif

    CfDebug("CopyRegularFile(%s,%s)\n", source, dest);

    discardbackup = ((attr.copy.backup == BACKUP_OPTION_NO_BACKUP) || (attr.copy.backup == BACKUP_OPTION_REPOSITORY_STORE));

    if (DONTDO)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Promise requires copy from %s to %s\n", source, dest);
        return false;
    }

#ifdef WITH_SELINUX
    if (selinux_enabled)
    {
        dest_exists = cfstat(dest, &cur_dest);

        if (dest_exists == 0)
        {
            /* get current security context of destination file */
            getfilecon(dest, &scontext);
        }
        else
        {
            /* use default security context when creating destination file */
            matchpathcon(dest, 0, &scontext);
            setfscreatecon(scontext);
        }
    }
#endif

    /* Make an assoc array of inodes used to preserve hard links */

    linkable = CompressedArrayValue(pp->inode_cache, sstat.st_ino);

    if (sstat.st_nlink > 1)     /* Preserve hard links, if possible */
    {
        if ((CompressedArrayElementExists(pp->inode_cache, sstat.st_ino)) && (strcmp(dest, linkable) != 0))
        {
            unlink(dest);
            MakeHardLink(dest, linkable, attr, pp);
            return true;
        }
    }

    if ((attr.copy.servers != NULL) && (strcmp(attr.copy.servers->item, "localhost") != 0))
    {
        CfDebug("This is a remote copy from server: %s\n", (char *) attr.copy.servers->item);
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

        strncpy(new, tmpstr, CF_BUFSIZE);

        free(tmpstr);
    }
    else
    {
#endif

        strncpy(new, dest, CF_BUFSIZE);

        if (!JoinSuffix(new, CF_NEW))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to construct filename for copy");
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

        if (attr.copy.encrypt)
        {
            if (!EncryptCopyRegularFileNet(source, new, sstat.st_size, attr, pp))
            {
                return false;
            }
        }
        else
        {
            if (!CopyRegularFileNet(source, new, sstat.st_size, attr, pp))
            {
                return false;
            }
        }
    }
    else
    {
        if (!CopyRegularFileDiskReport(source, new, attr, pp))
        {
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

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Copy of regular file succeeded %s to %s\n", source, new);

    backup[0] = '\0';

    if (!discardbackup)
    {
        char stamp[CF_BUFSIZE];
        time_t stampnow;

        CfDebug("Backup file %s\n", source);

        strncpy(backup, dest, CF_BUFSIZE);

        if (attr.copy.backup == BACKUP_OPTION_TIMESTAMP)
        {
            stampnow = time((time_t *) NULL);
            snprintf(stamp, CF_BUFSIZE - 1, "_%lu_%s", CFSTARTTIME, CanonifyName(cf_ctime(&stampnow)));

            if (!JoinSuffix(backup, stamp))
            {
                return false;
            }
        }

        if (!JoinSuffix(backup, CF_SAVED))
        {
            return false;
        }

        /* Now in case of multiple copies of same object, try to avoid overwriting original backup */

        if (lstat(backup, &s) != -1)
        {
            if (S_ISDIR(s.st_mode))     /* if there is a dir in the way */
            {
                backupisdir = true;
                PurgeLocalFiles(NULL, backup, attr, pp);
                rmdir(backup);
            }

            unlink(backup);
        }

        if (cf_rename(dest, backup) == -1)
        {
            /* ignore */
        }

        backupok = (lstat(backup, &s) != -1);   /* Did the cf_rename() succeed? NFS-safe */
    }
    else
    {
        /* Mainly important if there is a dir in the way */

        if (cfstat(dest, &s) != -1)
        {
            if (S_ISDIR(s.st_mode))
            {
                PurgeLocalFiles(NULL, dest, attr, pp);
                rmdir(dest);
            }
        }
    }

    if (lstat(new, &dstat) == -1)
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "stat", pp, attr, "Can't stat new file %s - another agent has picked it up?\n", new);
        return false;
    }

    if ((S_ISREG(dstat.st_mode)) && (dstat.st_size != sstat.st_size))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr,
             " !! New file %s seems to have been corrupted in transit (dest %d and src %d), aborting!\n", new,
             (int) dstat.st_size, (int) sstat.st_size);

        if (backupok)
        {
            cf_rename(backup, dest);    /* ignore failure of this call, as there is nothing more we can do */
        }

        return false;
    }

    if (attr.copy.verify)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?? Final verification of transmission ...\n");

        if (CompareFileHashes(source, new, &sstat, &dstat, attr, pp))
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, attr,
                 " !! New file %s seems to have been corrupted in transit, aborting!\n", new);

            if (backupok)
            {
                cf_rename(backup, dest);
            }

            return false;
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> New file %s transmitted correctly - verified\n", new);
        }
    }

#ifdef __APPLE__
    if (rsrcfork)
    {                           /* Can't just "mv" the resource fork, unfortunately */
        rsrcrd = open(new, O_RDONLY | O_BINARY);
        rsrcwd = open(dest, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0600);

        if (rsrcrd == -1 || rsrcwd == -1)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "open", "Open of Darwin resource fork rsrcrd/rsrcwd failed\n");
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
                    CfOut(OUTPUT_LEVEL_INFORM, "read", "Read of Darwin resource fork rsrcrd failed\n");
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
                        CfOut(OUTPUT_LEVEL_INFORM, "write", "Write of Darwin resource fork rsrcwd failed\n");
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

        if (cf_rename(new, dest) == -1)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_rename", pp, attr,
                 " !! Could not install copy file as %s, directory in the way?\n", dest);

            if (backupok)
            {
                cf_rename(backup, dest);        /* ignore failure */
            }

            return false;
        }

#ifdef __APPLE__
    }
#endif

    if ((!discardbackup) && backupisdir)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Cannot move a directory to repository, leaving at %s", backup);
    }
    else if ((!discardbackup) && (ArchiveToRepository(backup, attr, pp)))
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

#ifdef WITH_SELINUX
    if (selinux_enabled)
    {
        if (dest_exists == 0)
        {
            /* set dest context to whatever it was before copy */
            setfilecon(dest, scontext);
        }
        else
        {
            /* set create context back to default */
            setfscreatecon(NULL);
        }
        freecon(scontext);
    }
#endif

    return true;
}

static int TransformFile(char *file, Attributes attr, Promise *pp)
{
    char comm[CF_EXPANDSIZE], line[CF_BUFSIZE];
    FILE *pop = NULL;
    int print = false;
    int transRetcode = 0;

    if (attr.transformer == NULL || file == NULL)
    {
        return false;
    }

    ExpandScalar(attr.transformer, comm);
    CfOut(OUTPUT_LEVEL_INFORM, "", "I: Transforming: %s ", comm);

    if (!IsExecutable(CommandArg0(comm)))
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, "I: Transformer %s %s failed", attr.transformer, file);
        return false;
    }

    if (strncmp(comm, "/bin/echo", strlen("/bin/echo")) == 0)
    {
        print = true;
    }

    if (!DONTDO)
    {
        CfLock thislock = AcquireLock(comm, VUQNAME, CFSTARTTIME, attr, pp, false);

        if (thislock.lock == NULL)
        {
            return false;
        }

        if ((pop = cf_popen(comm, "r")) == NULL)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, "I: Transformer %s %s failed", attr.transformer, file);
            YieldCurrentLock(thislock);
            return false;
        }

        while (!feof(pop))
        {
            if (CfReadLine(line, CF_BUFSIZE, pop) == -1)
            {
                FatalError("Error in CfReadLine");
            }

            if (print)
            {
                CfOut(OUTPUT_LEVEL_REPORTING, "", "%s", line);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", "%s", line);
            }
        }

        transRetcode = cf_pclose(pop);

        if (VerifyCommandRetcode(transRetcode, true, attr, pp))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "-> Transformer %s => %s seemed to work ok", file, comm);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "-> Transformer %s => %s returned error", file, comm);
        }

        YieldCurrentLock(thislock);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " -> Need to transform file \"%s\" with \"%s\"", file, comm);
    }

    return true;
}

static void VerifyName(char *path, struct stat *sb, Attributes attr, Promise *pp)
{
    mode_t newperm;
    struct stat dsb;

    if (lstat(path, &dsb) == -1)
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, attr, "File object named %s is not there (promise kept)", path);
        return;
    }
    else
    {
        if (attr.rename.disable)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! Warning - file object %s exists, contrary to promise\n", path);
        }
    }

    if (attr.rename.newname)
    {
        if (DONTDO)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " -> File %s should be renamed to %s to keep promise\n", path, attr.rename.newname);
            return;
        }
        else
        {
            if (!FileInRepository(attr.rename.newname))
            {
                if (cf_rename(path, attr.rename.newname) == -1)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_rename", pp, attr, " !! Error occurred while renaming %s\n", path);
                    return;
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Renaming file %s to %s\n", path, attr.rename.newname);
                }
            }
            else
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr,
                     " !! Rename to same destination twice? Would overwrite saved copy - aborting");
            }
        }

        return;
    }

    if (S_ISLNK(dsb.st_mode))
    {
        if (attr.rename.disable)
        {
            if (!DONTDO)
            {
                if (unlink(path) == -1)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "unlink", pp, attr, " !! Unable to unlink %s\n", path);
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Disabling symbolic link %s by deleting it\n", path);
                }
            }
            else
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " * Need to disable link %s to keep promise\n", path);
            }

            return;
        }
    }

/* Normal disable - has priority */

    if (attr.rename.disable)
    {
        char newname[CF_BUFSIZE];

        if (attr.transaction.action == cfa_warn)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! %s '%s' should be renamed",
                 S_ISDIR(sb->st_mode) ? "Directory" : "File", path);
            return;
        }

        if (attr.rename.newname && strlen(attr.rename.newname) > 0)
        {
            if (IsAbsPath(attr.rename.newname))
            {
                strncpy(path, attr.rename.newname, CF_BUFSIZE - 1);
            }
            else
            {
                strcpy(newname, path);
                ChopLastNode(newname);

                if (!JoinPath(newname, attr.rename.newname))
                {
                    return;
                }
            }
        }
        else
        {
            strcpy(newname, path);

            if (attr.rename.disable_suffix)
            {
                if (!JoinSuffix(newname, attr.rename.disable_suffix))
                {
                    return;
                }
            }
            else
            {
                if (!JoinSuffix(newname, ".cfdisabled"))
                {
                    return;
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
            CfOut(OUTPUT_LEVEL_INFORM, "", " -> File %s should be renamed to %s to keep promise\n", path, newname);
            return;
        }
        else
        {
            cf_chmod(path, newperm);

            if (!FileInRepository(newname))
            {
                if (cf_rename(path, newname) == -1)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_rename", pp, attr, "Error occurred while renaming %s\n", path);
                    return;
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Disabling/renaming file %s to %s with mode %jo\n", path,
                         newname, (uintmax_t)newperm);
                }

                if (ArchiveToRepository(newname, attr, pp))
                {
                    unlink(newname);
                }
            }
            else
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr,
                     " !! Disable required twice? Would overwrite saved copy - changing permissions only");
            }
        }

        return;
    }

    if (attr.rename.rotate == 0)
    {
        if (attr.transaction.action == cfa_warn)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! File '%s' should be truncated", path);
        }
        else if (!DONTDO)
        {
            TruncateFile(path);
            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Truncating (emptying) %s\n", path);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " * File %s needs emptying", path);
        }
        return;
    }

    if (attr.rename.rotate > 0)
    {
        if (attr.transaction.action == cfa_warn)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! File '%s' should be rotated", path);
        }
        else if (!DONTDO)
        {
            RotateFiles(path, attr.rename.rotate);
            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Rotating files %s in %d fifo\n", path, attr.rename.rotate);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " * File %s needs rotating", path);
        }

        return;
    }
}

static void VerifyDelete(char *path, struct stat *sb, Attributes attr, Promise *pp)
{
    const char *lastnode = ReadLastNode(path);
    char buf[CF_MAXVARSIZE];

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Verifying file deletions for %s\n", path);

    if (DONTDO)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Promise requires deletion of file object %s\n", path);
    }
    else
    {
        switch (attr.transaction.action)
        {
        case cfa_warn:

            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! %s '%s' should be deleted",
                 S_ISDIR(sb->st_mode) ? "Directory" : "File", path);
            break;

        case cfa_fix:

            if (!S_ISDIR(sb->st_mode))
            {
                if (unlink(lastnode) == -1)
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "unlink", pp, attr, "Couldn't unlink %s tidying\n", path);
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Deleted file %s\n", path);
                }
            }
            else                // directory
            {
                if (!attr.delete.rmdirs)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "unlink", "Keeping directory %s\n", path);
                    return;
                }

                if (attr.havedepthsearch && strcmp(path, pp->promiser) == 0)
                {
                    /* This is the parent and we cannot delete it from here - must delete separately */
                    return;
                }

                // use the full path if we are to delete the current dir
                if ((strcmp(lastnode, ".") == 0) && strlen(path) > 2)
                {
                    snprintf(buf, sizeof(buf), "%s", path);
                    buf[strlen(path) - 1] = '\0';
                    buf[strlen(path) - 2] = '\0';
                }
                else
                {
                    snprintf(buf, sizeof(buf), "%s", lastnode);
                }

                if (rmdir(buf) == -1)
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "rmdir", pp, attr,
                         " !! Delete directory %s failed (cannot delete node called \"%s\")\n", path, buf);
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Deleted directory %s\n", path);
                }
            }

            break;

        default:
            FatalError("Cfengine: internal error: illegal file action\n");
        }
    }
}

static void TouchFile(char *path, struct stat *sb, Attributes attr, Promise *pp)
{
    if (!DONTDO)
    {
        if (utime(path, NULL) != -1)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Touched (updated time stamps) %s\n", path);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "utime", pp, attr, "Touch %s failed to update timestamps\n", path);
        }
    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Need to touch (update timestamps) %s\n", path);
    }
}

void VerifyFileAttributes(char *file, struct stat *dstat, Attributes attr, Promise *pp)
{
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

        CfDebug("VerifyFileAttributes(%s -> %" PRIoMAX ")\n", file, (uintmax_t)newperm);

        /* directories must have x set if r set, regardless  */

        if (S_ISDIR(dstat->st_mode))
        {
            if (attr.perms.rxdirs)
            {
                CfDebug("Directory...fixing x bits\n");

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
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "NB: rxdirs is set to false - x for r bits not checked\n");
            }
        }
    }

    VerifySetUidGid(file, dstat, newperm, pp, attr);

# ifdef __APPLE__
    if (VerifyFinderType(file, dstat, attr, pp))
    {
        /* nop */
    }
# endif
#endif

    if (VerifyOwner(file, pp, attr, dstat))
    {
        /* nop */
    }

#ifdef __MINGW32__
    if (NovaWin_FileExists(file) && !NovaWin_IsDir(file))
#else
    if (attr.havechange && S_ISREG(dstat->st_mode))
#endif
    {
        VerifyFileIntegrity(file, attr, pp);
    }

    if (attr.havechange)
    {
        VerifyFileChanges(file, dstat, attr, pp);
    }

#ifndef __MINGW32__
    if (S_ISLNK(dstat->st_mode))        /* No point in checking permission on a link */
    {
        KillGhostLink(file, attr, pp);
        umask(maskvalue);
        return;
    }
#endif

    if (attr.acl.acl_entries)
    {
        VerifyACL(file, attr, pp);
    }

#ifndef __MINGW32__
    VerifySetUidGid(file, dstat, dstat->st_mode, pp, attr);

    if ((newperm & 07777) == (dstat->st_mode & 07777))  /* file okay */
    {
        CfDebug("File okay, newperm = %" PRIoMAX ", stat = %" PRIoMAX "\n", (uintmax_t)(newperm & 07777), (uintmax_t)(dstat->st_mode & 07777));
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, attr, " -> File permissions on %s as promised\n", file);
    }
    else
    {
        CfDebug("Trying to fix mode...newperm = %" PRIoMAX ", stat = %" PRIoMAX "\n", (uintmax_t)(newperm & 07777), (uintmax_t)(dstat->st_mode & 07777));

        switch (attr.transaction.action)
        {
        case cfa_warn:

            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! %s has permission %jo - [should be %jo]\n", file,
                 (uintmax_t)dstat->st_mode & 07777, (uintmax_t)newperm & 07777);
            break;

        case cfa_fix:

            if (!DONTDO)
            {
                if (cf_chmod(file, newperm & 07777) == -1)
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "cf_chmod", "cf_chmod failed on %s\n", file);
                    break;
                }
            }

            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Object %s had permission %jo, changed it to %jo\n", file,
                 (uintmax_t)dstat->st_mode & 07777, (uintmax_t)newperm & 07777);
            break;

        default:
            FatalError("cfengine: internal error VerifyFileAttributes(): illegal file action\n");
        }
    }

# if defined HAVE_CHFLAGS       /* BSD special flags */

    newflags = (dstat->st_flags & CHFLAGS_MASK);
    newflags |= attr.perms.plus_flags;
    newflags &= ~(attr.perms.minus_flags);

    if ((newflags & CHFLAGS_MASK) == (dstat->st_flags & CHFLAGS_MASK))  /* file okay */
    {
        CfDebug("BSD File okay, flags = %" PRIxMAX ", current = %" PRIxMAX "\n",
                (uintmax_t)(newflags & CHFLAGS_MASK),
                (uintmax_t)(dstat->st_flags & CHFLAGS_MASK));
    }
    else
    {
        CfDebug("BSD Fixing %s, newflags = %" PRIxMAX ", flags = %" PRIxMAX "\n",
                file, (uintmax_t)(newflags & CHFLAGS_MASK),
                (uintmax_t)(dstat->st_flags & CHFLAGS_MASK));

        switch (attr.transaction.action)
        {
        case cfa_warn:

            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr,
                 " !! %s has flags %jo - [should be %jo]\n",
                 file, (uintmax_t)(dstat->st_mode & CHFLAGS_MASK),
                 (uintmax_t)(newflags & CHFLAGS_MASK));
            break;

        case cfa_fix:

            if (!DONTDO)
            {
                if (chflags(file, newflags & CHFLAGS_MASK) == -1)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_DENIED, "chflags", pp, attr, " !! Failed setting BSD flags %jx on %s\n", (uintmax_t)newflags,
                         file);
                    break;
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> %s had flags %jo, changed it to %jo\n", file,
                         (uintmax_t)(dstat->st_flags & CHFLAGS_MASK),
                         (uintmax_t)(newflags & CHFLAGS_MASK));
                }
            }

            break;

        default:
            FatalError("cfengine: internal error VerifyFileAttributes() illegal file action\n");
        }
    }
# endif
#endif

    if (attr.touch)
    {
        if (utime(file, NULL) == -1)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_DENIED, "utime", pp, attr, " !! Touching file %s failed", file);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Touching file %s", file);
        }
    }

#ifndef __MINGW32__
    umask(maskvalue);
#endif
    CfDebug("VerifyFileAttributes(Done)\n");
}

int DepthSearch(char *name, struct stat *sb, int rlevel, Attributes attr, Promise *pp)
{
    Dir *dirh;
    int goback;
    const struct dirent *dirp;
    char path[CF_BUFSIZE];
    struct stat lsb;

    if (!attr.havedepthsearch)  /* if the search is trivial, make sure that we are in the parent dir of the leaf */
    {
        char basedir[CF_BUFSIZE];

        CfDebug(" -> Direct file reference %s, no search implied\n", name);
        snprintf(basedir, sizeof(basedir), "%s", name);
        ChopLastNode(basedir);
        if (chdir(basedir))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to chdir into '%s'\n", basedir);
            return false;
        }
        return VerifyFileLeaf(name, sb, attr, pp);
    }

    if (rlevel > CF_RECURSION_LIMIT)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "WARNING: Very deep nesting of directories (>%d deep): %s (Aborting files)", rlevel, name);
        return false;
    }

    if (rlevel > CF_RECURSION_LIMIT)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "WARNING: Very deep nesting of directories (>%d deep): %s (Aborting files)", rlevel, name);
        return false;
    }

    memset(path, 0, CF_BUFSIZE);

    CfDebug("To iterate is Human, to recurse is Divine...(%s)\n", name);

    if (!PushDirState(name, sb))
    {
        return false;
    }

    if ((dirh = OpenDirLocal(".")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "opendir", "Could not open existing directory %s\n", name);
        return false;
    }

    for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
    {
        if (!ConsiderFile(dirp->d_name, name, attr, pp))
        {
            continue;
        }

        strcpy(path, name);
        AddSlash(path);

        if (!JoinPath(path, dirp->d_name))
        {
            CloseDir(dirh);
            return true;
        }

        if (lstat(dirp->d_name, &lsb) == -1)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "lstat", "Recurse was looking at %s when an error occurred:\n", path);
            continue;
        }

        if (S_ISLNK(lsb.st_mode))       /* should we ignore links? */
        {
            if (!KillGhostLink(path, attr, pp))
            {
                VerifyFileLeaf(path, &lsb, attr, pp);
            }
            else
            {
                continue;
            }
        }

        /* See if we are supposed to treat links to dirs as dirs and descend */

        if ((attr.recursion.travlinks) && (S_ISLNK(lsb.st_mode)))
        {
            if ((lsb.st_uid != 0) && (lsb.st_uid != getuid()))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "",
                      "File %s is an untrusted link: cfengine will not follow it with a destructive operation", path);
                continue;
            }

            /* if so, hide the difference by replacing with actual object */

            if (cfstat(dirp->d_name, &lsb) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "stat", "Recurse was working on %s when this failed:\n", path);
                continue;
            }
        }

        if ((attr.recursion.xdev) && (DeviceBoundary(&lsb, pp)))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping %s on different device - use xdev option to change this\n", path);
            continue;
        }

        if (S_ISDIR(lsb.st_mode))
        {
            if (SkipDirLinks(path, dirp->d_name, attr.recursion))
            {
                continue;
            }

            if ((attr.recursion.depth > 1) && (rlevel <= attr.recursion.depth))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " ->>  Entering %s (%d)\n", path, rlevel);
                goback = DepthSearch(path, &lsb, rlevel + 1, attr, pp);
                PopDirState(goback, name, sb, attr.recursion);
                VerifyFileLeaf(path, &lsb, attr, pp);
            }
            else
            {
                VerifyFileLeaf(path, &lsb, attr, pp);
            }
        }
        else
        {
            VerifyFileLeaf(path, &lsb, attr, pp);
        }
    }

    CloseDir(dirh);
    return true;
}

static int PushDirState(char *name, struct stat *sb)
{
    if (chdir(name) == -1)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "chdir", "Could not change to directory %s, mode %jo in tidy", name, (uintmax_t)(sb->st_mode & 07777));
        return false;
    }
    else
    {
        CfDebug("Changed directory to %s\n", name);
    }

    CheckLinkSecurity(sb, name);
    return true;
}

static void PopDirState(int goback, char *name, struct stat *sb, Recursion r)
{
    if (goback && (r.travlinks))
    {
        if (chdir(name) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "chdir", "Error in backing out of recursive travlink descent securely to %s", name);
            FatalError("Terminating");
        }

        CheckLinkSecurity(sb, name);
    }
    else if (goback)
    {
        if (chdir("..") == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "chdir", "Error in backing out of recursive descent securely to %s", name);
            FatalError("Terminating");
        }
    }
}

static void CheckLinkSecurity(struct stat *sb, char *name)
{
    struct stat security;

    CfDebug("Checking the inode and device to make sure we are where we think we are...\n");

    if (cfstat(".", &security) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "stat", "Could not stat directory %s after entering!", name);
        return;
    }

    if ((sb->st_dev != security.st_dev) || (sb->st_ino != security.st_ino))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              "SERIOUS SECURITY ALERT: path race exploited in recursion to/from %s. Not safe for agent to continue - aborting",
              name);
        FatalError("Terminating");
    }
}

static void VerifyCopiedFileAttributes(char *file, struct stat *dstat, struct stat *sstat, Attributes attr,
                                       Promise *pp)
{
#ifndef __MINGW32__
    mode_t newplus, newminus;
    uid_t save_uid;
    gid_t save_gid;

// If we get here, there is both a src and dest file

    CfDebug("VerifyCopiedFile(%s,+%" PRIoMAX ",-%" PRIoMAX ")\n", file, (uintmax_t)attr.perms.plus, (uintmax_t)attr.perms.minus);

    save_uid = (attr.perms.owners)->uid;
    save_gid = (attr.perms.groups)->gid;

    if (attr.copy.preserve)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Attempting to preserve file permissions from the source: %jo",
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
    VerifyFileAttributes(file, dstat, attr, pp);

#ifndef __MINGW32__
    (attr.perms.owners)->uid = save_uid;
    (attr.perms.groups)->gid = save_gid;
#endif
}

static void *CopyFileSources(char *destination, Attributes attr, Promise *pp)
{
    char *source = attr.copy.source;
    char *server = pp->this_server;
    char vbuff[CF_BUFSIZE];
    struct stat ssb, dsb;
    struct timespec start;
    char eventname[CF_BUFSIZE];

    CfDebug("CopyFileSources(%s,%s)", source, destination);

    if ((pp->conn != NULL) && (!pp->conn->authenticated))
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, attr, "No authenticated source %s in files.copyfrom promise\n", source);
        return NULL;
    }

    if (cf_stat(attr.copy.source, &ssb, attr, pp) == -1)
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, "Can't stat %s in files.copyfrom promise\n", source);
        return NULL;
    }

    start = BeginMeasure();

    strncpy(vbuff, destination, CF_BUFSIZE - 4);

    if (S_ISDIR(ssb.st_mode))   /* could be depth_search */
    {
        AddSlash(vbuff);
        strcat(vbuff, ".");
    }

    if (!MakeParentDirectory(vbuff, attr.move_obstructions))
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, "Can't make directories for %s in files.copyfrom promise\n", vbuff);
        return NULL;
    }

    if (S_ISDIR(ssb.st_mode))   /* could be depth_search */
    {
        if (attr.copy.purge)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! (Destination purging enabled)\n");
        }

        CfOut(OUTPUT_LEVEL_VERBOSE, "", " ->>  Entering %s\n", source);
        SetSearchDevice(&ssb, pp);
        SourceSearchAndCopy(source, destination, attr.recursion.depth, attr, pp);

        if (cfstat(destination, &dsb) != -1)
        {
            if (attr.copy.check_root)
            {
                VerifyCopiedFileAttributes(destination, &dsb, &ssb, attr, pp);
            }
        }
    }
    else
    {
        VerifyCopy(source, destination, attr, pp);
    }

    snprintf(eventname, CF_BUFSIZE - 1, "Copy(%s:%s > %s)", server, source, destination);
    EndMeasure(eventname, start);

    return NULL;
}

int ScheduleCopyOperation(char *destination, Attributes attr, Promise *pp)
{
    AgentConnection *conn = NULL;

    if (!attr.copy.source)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Copy file %s check\n", destination);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Copy file %s from %s check\n", destination, attr.copy.source);
    }

    if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item, "localhost") == 0)
    {
        pp->this_server = xstrdup("localhost");
    }
    else
    {
        conn = NewServerConnection(attr, pp);

        if (conn == NULL)
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, " -> No suitable server responded to hail");
            PromiseRef(OUTPUT_LEVEL_INFORM, pp);
            return false;
        }
    }

    pp->conn = conn;            /* for ease of access */
    pp->cache = NULL;

    CopyFileSources(destination, attr, pp);

    if (attr.transaction.background)
    {
        DisconnectServer(conn);
    }
    else
    {
        ServerNotBusy(conn);
    }

    return true;
}

int ScheduleLinkOperation(char *destination, char *source, Attributes attr, Promise *pp)
{
    const char *lastnode;

    lastnode = ReadLastNode(destination);

    if (MatchRlistItem(attr.link.copy_patterns, lastnode))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Link %s matches copy_patterns\n", destination);
        VerifyCopy(attr.link.source, destination, attr, pp);
        return true;
    }

    switch (attr.link.link_type)
    {
    case FILE_LINK_TYPE_SYMLINK:
        VerifyLink(destination, source, attr, pp);
        break;
    case FILE_LINK_TYPE_HARDLINK:
        VerifyHardLink(destination, source, attr, pp);
        break;
    case FILE_LINK_TYPE_RELATIVE:
        VerifyRelativeLink(destination, source, attr, pp);
        break;
    case FILE_LINK_TYPE_ABSOLUTE:
        VerifyAbsoluteLink(destination, source, attr, pp);
        break;
    default:
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unknown link type - should not happen.\n");
        break;
    }

    return true;
}

int ScheduleLinkChildrenOperation(char *destination, char *source, int recurse, Attributes attr, Promise *pp)
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
            CfOut(OUTPUT_LEVEL_ERROR, "", "Cannot promise to link multiple files to children of %s as it is not a directory!",
                  destination);
            return false;
        }
    }

    snprintf(promiserpath, CF_BUFSIZE, "%s/.", destination);

    if ((ret == -1 || !S_ISDIR(lsb.st_mode)) && !CfCreateFile(promiserpath, pp, attr))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Cannot promise to link multiple files to children of %s as it is not a directory!",
              destination);
        return false;
    }

    if ((dirh = OpenDirLocal(source)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "opendir", pp, attr, "Can't open source of children to link %s\n", attr.link.source);
        return false;
    }

    for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
    {
        if (!ConsiderFile(dirp->d_name, source, attr, pp))
        {
            continue;
        }

        /* Assemble pathnames */

        strncpy(promiserpath, destination, CF_BUFSIZE - 1);
        AddSlash(promiserpath);

        if (!JoinPath(promiserpath, dirp->d_name))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Can't construct filename which verifying child links\n");
            CloseDir(dirh);
            return false;
        }

        strncpy(sourcepath, source, CF_BUFSIZE - 1);
        AddSlash(sourcepath);

        if (!JoinPath(sourcepath, dirp->d_name))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, "Can't construct filename while verifying child links\n");
            CloseDir(dirh);
            return false;
        }

        if ((lstat(promiserpath, &lsb) != -1) && !S_ISLNK(lsb.st_mode) && !S_ISDIR(lsb.st_mode))
        {
            if (attr.link.when_linking_children == cfa_override)
            {
                attr.move_obstructions = true;
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Have promised not to disturb %s\'s existing content", promiserpath);
                continue;
            }
        }

        if ((attr.recursion.depth > recurse) && (lstat(sourcepath, &lsb) != -1) && S_ISDIR(lsb.st_mode))
        {
            ScheduleLinkChildrenOperation(promiserpath, sourcepath, recurse + 1, attr, pp);
        }
        else
        {
            ScheduleLinkOperation(promiserpath, sourcepath, attr, pp);
        }
    }

    CloseDir(dirh);
    return true;
}

static void VerifyFileIntegrity(char *file, Attributes attr, Promise *pp)
{
    unsigned char digest1[EVP_MAX_MD_SIZE + 1];
    unsigned char digest2[EVP_MAX_MD_SIZE + 1];
    int changed = false, one, two;

    if ((attr.change.report_changes != FILE_CHANGE_REPORT_CONTENT_CHANGE) && (attr.change.report_changes != FILE_CHANGE_REPORT_ALL))
    {
        return;
    }

    memset(digest1, 0, EVP_MAX_MD_SIZE + 1);
    memset(digest2, 0, EVP_MAX_MD_SIZE + 1);

    if (attr.change.hash == HASH_METHOD_BEST)
    {
        if (!DONTDO)
        {
            HashFile(file, digest1, HASH_METHOD_MD5);
            HashFile(file, digest2, HASH_METHOD_SHA1);

            one = FileHashChanged(file, digest1, OUTPUT_LEVEL_ERROR, HASH_METHOD_MD5, attr, pp);
            two = FileHashChanged(file, digest2, OUTPUT_LEVEL_ERROR, HASH_METHOD_SHA1, attr, pp);

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

            if (FileHashChanged(file, digest1, OUTPUT_LEVEL_ERROR, attr.change.hash, attr, pp))
            {
                changed = true;
            }
        }
    }

    if (changed)
    {
        NewPersistentContext(pp->ns, "checksum_alerts", CF_PERSISTENCE, CONTEXT_STATE_POLICY_PRESERVE);
        LogHashChange(file, FILE_STATE_CONTENT_CHANGED, "Content changed", pp);
    }

    if (attr.change.report_diffs)
    {
        LogFileChange(file, changed, attr, pp);
    }
}

static int CompareForFileCopy(char *sourcefile, char *destfile, struct stat *ssb, struct stat *dsb, Attributes attr,
                              Promise *pp)
{
    int ok_to_copy;

    switch (attr.copy.compare)
    {
    case FILE_COMPARATOR_CHECKSUM:
    case FILE_COMPARATOR_HASH:

        if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
        {
            ok_to_copy = CompareFileHashes(sourcefile, destfile, ssb, dsb, attr, pp);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Checksum comparison replaced by ctime: files not regular\n");
            ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);
        }

        if (ok_to_copy)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Image file %s has a wrong digest/checksum (should be copy of %s)\n", destfile,
                  sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_BINARY:

        if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
        {
            ok_to_copy = CompareBinaryFiles(sourcefile, destfile, ssb, dsb, attr, pp);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Byte comparison replaced by ctime: files not regular\n");
            ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);
        }

        if (ok_to_copy)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Image file %s has a wrong binary checksum (should be copy of %s)\n", destfile,
                  sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_MTIME:

        ok_to_copy = (dsb->st_mtime < ssb->st_mtime);

        if (ok_to_copy)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Image file %s out of date (should be copy of %s)\n", destfile, sourcefile);
            return ok_to_copy;
        }
        break;

    case FILE_COMPARATOR_ATIME:

        ok_to_copy = (dsb->st_ctime < ssb->st_ctime) ||
            (dsb->st_mtime < ssb->st_mtime) || (CompareBinaryFiles(sourcefile, destfile, ssb, dsb, attr, pp));

        if (ok_to_copy)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Image file %s seems out of date (should be copy of %s)\n", destfile, sourcefile);
            return ok_to_copy;
        }
        break;

    default:
        ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);

        if (ok_to_copy)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Image file %s out of date (should be copy of %s)\n", destfile, sourcefile);
            return ok_to_copy;
        }
        break;
    }

    return false;
}

static void FileAutoDefine(char *destfile, const char *ns)
{
    char context[CF_MAXVARSIZE];

    snprintf(context, CF_MAXVARSIZE, "auto_%s", CanonifyName(destfile));
    NewClass(context,  ns);
    CfOut(OUTPUT_LEVEL_INFORM, "", "Auto defining class %s\n", context);
}

#ifndef __MINGW32__
static void VerifySetUidGid(char *file, struct stat *dstat, mode_t newperm, Promise *pp, Attributes attr)
{
    int amroot = true;

    if (!IsPrivileged())
    {
        amroot = false;
    }

    if ((dstat->st_uid == 0) && (dstat->st_mode & S_ISUID))
    {
        if (newperm & S_ISUID)
        {
            if (!IsItemIn(VSETUIDLIST, file))
            {
                if (amroot)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, "NEW SETUID root PROGRAM %s\n", file);
                }

                PrependItem(&VSETUIDLIST, file, NULL);
            }
        }
        else
        {
            switch (attr.transaction.action)
            {
            case cfa_fix:

                cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Removing setuid (root) flag from %s...\n\n", file);
                break;

            case cfa_warn:

                if (amroot)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! WARNING setuid (root) flag on %s...\n\n", file);
                }
                break;
            }
        }
    }

    if (dstat->st_uid == 0 && (dstat->st_mode & S_ISGID))
    {
        if (newperm & S_ISGID)
        {
            if (!IsItemIn(VSETUIDLIST, file))
            {
                if (S_ISDIR(dstat->st_mode))
                {
                    /* setgid directory */
                }
                else
                {
                    if (amroot)
                    {
                        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! NEW SETGID root PROGRAM %s\n", file);
                    }

                    PrependItem(&VSETUIDLIST, file, NULL);
                }
            }
        }
        else
        {
            switch (attr.transaction.action)
            {
            case cfa_fix:

                cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Removing setgid (root) flag from %s...\n\n", file);
                break;

            case cfa_warn:

                cfPS(OUTPUT_LEVEL_INFORM, CF_WARN, "", pp, attr, " !! WARNING setgid (root) flag on %s...\n\n", file);
                break;

            default:
                break;
            }
        }
    }
}
#endif

#ifdef __APPLE__

static int VerifyFinderType(char *file, struct stat *statbuf, Attributes a, Promise *pp)
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

    CfDebug("VerifyFinderType of %s for %s\n", file, a.perms.findertype);

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
                CfOut(OUTPUT_LEVEL_INFORM, "", "Promised to set Finder Type code of %s to %s\n", file, a.perms.findertype);
                return 0;
            }

            /* setattrlist does not take back in the long ssize */
            retval = setattrlist(file, &attrs, &fndrInfo.created, 4 * sizeof(struct timespec) + sizeof(FInfo), 0);

            CfDebug("CheckFinderType setattrlist returned %d\n", retval);

            if (retval >= 0)
            {
                cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, "Setting Finder Type code of %s to %s\n", file, a.perms.findertype);
            }
            else
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Setting Finder Type code of %s to %s failed!!\n", file,
                     a.perms.findertype);
            }

            return retval;

        case cfa_warn:
            CfOut(OUTPUT_LEVEL_ERROR, "", "Darwin FinderType does not match -- not fixing.\n");
            return 0;

        default:
            return 0;
        }
    }
    else
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, "Finder Type code of %s to %s is as promised\n", file, a.perms.findertype);
        return 0;
    }
}

#endif

static void TruncateFile(char *name)
{
    struct stat statbuf;
    int fd;

    if (cfstat(name, &statbuf) == -1)
    {
        CfDebug("cfengine: didn't find %s to truncate\n", name);
    }
    else
    {
        if ((fd = creat(name, 000)) == -1)      /* dummy mode ignored */
        {
            CfOut(OUTPUT_LEVEL_ERROR, "creat", "Failed to create or truncate file %s\n", name);
        }
        else
        {
            close(fd);
        }
    }
}

static void RegisterAHardLink(int i, char *value, Attributes attr, Promise *pp)
{
    if (!FixCompressedArrayValue(i, value, &(pp->inode_cache)))
    {
        /* Not root hard link, remove to preserve consistency */
        if (DONTDO)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Need to remove old hard link %s to preserve structure..\n", value);
        }
        else
        {
            if (attr.transaction.action == cfa_warn)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Need to remove old hard link %s to preserve structure..\n", value);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Removing old hard link %s to preserve structure..\n", value);
                unlink(value);
            }
        }
    }
}

static int cf_stat(char *file, struct stat *buf, Attributes attr, Promise *pp)
{
    int res;

    if ((attr.copy.servers == NULL) || (strcmp(attr.copy.servers->item, "localhost") == 0))
    {
        res = cfstat(file, buf);
        CheckForFileHoles(buf, pp);
        return res;
    }
    else
    {
        return cf_remote_stat(file, buf, "file", attr, pp);
    }
}

#ifndef __MINGW32__

static int cf_readlink(char *sourcefile, char *linkbuf, int buffsize, Attributes attr, Promise *pp)
 /* wrapper for network access */
{
    Stat *sp;

    memset(linkbuf, 0, buffsize);

    if ((attr.copy.servers == NULL) || (strcmp(attr.copy.servers->item, "localhost") == 0))
    {
        return readlink(sourcefile, linkbuf, buffsize - 1);
    }

    for (sp = pp->cache; sp != NULL; sp = sp->next)
    {
        if ((strcmp(attr.copy.servers->item, sp->cf_server) == 0) && (strcmp(sourcefile, sp->cf_filename) == 0))
        {
            if (sp->cf_readlink != NULL)
            {
                if (strlen(sp->cf_readlink) + 1 > buffsize)
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, "readlink value is too large in cfreadlink\n");
                    CfOut(OUTPUT_LEVEL_ERROR, "", "Contained [%s]]n", sp->cf_readlink);
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
    }

    return -1;
}

#endif /* !__MINGW32__ */

static bool CopyRegularFileDiskReport(char *source, char *destination, Attributes attr, Promise *pp)
// TODO: return error codes in CopyRegularFileDisk and print them to cfPS here
{
    bool make_holes = false;

    if(pp && (pp->makeholes))
    {
        make_holes = true;
    }

    bool result = CopyRegularFileDisk(source, destination, make_holes);

    if(!result)
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, "Failed copying file %s to %s", source, destination);
    }

    return result;
}

static int SkipDirLinks(char *path, const char *lastnode, Recursion r)
{
    CfDebug("SkipDirLinks(%s,%s)\n", path, lastnode);

    if (r.exclude_dirs)
    {
        if ((MatchRlistItem(r.exclude_dirs, path)) || (MatchRlistItem(r.exclude_dirs, lastnode)))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping matched excluded directory %s\n", path);
            return true;
        }
    }

    if (r.include_dirs)
    {
        if (!((MatchRlistItem(r.include_dirs, path)) || (MatchRlistItem(r.include_dirs, lastnode))))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping matched non-included directory %s\n", path);
            return true;
        }
    }

    return false;
}

#ifndef __MINGW32__

static int VerifyOwner(char *file, Promise *pp, Attributes attr, struct stat *sb)
{
    struct passwd *pw;
    struct group *gp;
    UidList *ulp;
    GidList *glp;
    short uidmatch = false, gidmatch = false;
    uid_t uid = CF_SAME_OWNER;
    gid_t gid = CF_SAME_GROUP;

    CfDebug("VerifyOwner: %" PRIdMAX "\n", (uintmax_t) sb->st_uid);

    for (ulp = attr.perms.owners; ulp != NULL; ulp = ulp->next)
    {
        if (ulp->uid == CF_SAME_OWNER || sb->st_uid == ulp->uid)        /* "same" matches anything */
        {
            uid = ulp->uid;
            uidmatch = true;
            break;
        }
    }

    if (attr.perms.groups->next == NULL && attr.perms.groups->gid == CF_UNKNOWN_GROUP)  // Only one non.existent item
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, " !! Unable to make file belong to an unknown group");
    }

    if (attr.perms.owners->next == NULL && attr.perms.owners->uid == CF_UNKNOWN_OWNER)  // Only one non.existent item
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr, " !! Unable to make file belong to an unknown user");
    }

    for (glp = attr.perms.groups; glp != NULL; glp = glp->next)
    {
        if (glp->gid == CF_SAME_GROUP || sb->st_gid == glp->gid)        /* "same" matches anything */
        {
            gid = glp->gid;
            gidmatch = true;
            break;
        }
    }

    if (uidmatch && gidmatch)
    {
        return false;
    }
    else
    {
        if (!uidmatch)
        {
            for (ulp = attr.perms.owners; ulp != NULL; ulp = ulp->next)
            {
                if (attr.perms.owners->uid != CF_UNKNOWN_OWNER)
                {
                    uid = attr.perms.owners->uid;       /* default is first (not unknown) item in list */
                    break;
                }
            }
        }

        if (!gidmatch)
        {
            for (glp = attr.perms.groups; glp != NULL; glp = glp->next)
            {
                if (attr.perms.groups->gid != CF_UNKNOWN_GROUP)
                {
                    gid = attr.perms.groups->gid;       /* default is first (not unknown) item in list */
                    break;
                }
            }
        }

        switch (attr.transaction.action)
        {
        case cfa_fix:

            if (uid == CF_SAME_OWNER && gid == CF_SAME_GROUP)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Touching %s\n", file);
            }
            else
            {
                if (uid != CF_SAME_OWNER)
                {
                    CfDebug("(Change owner to uid %" PRIuMAX " if possible)\n", (uintmax_t)uid);
                }

                if (gid != CF_SAME_GROUP)
                {
                    CfDebug("Change group to gid %" PRIuMAX " if possible)\n", (uintmax_t)gid);
                }
            }

            if (!DONTDO && S_ISLNK(sb->st_mode))
            {
# ifdef HAVE_LCHOWN
                CfDebug("Using LCHOWN function\n");
                if (lchown(file, uid, gid) == -1)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "lchown", " !! Cannot set ownership on link %s!\n", file);
                }
                else
                {
                    return true;
                }
# endif
            }
            else if (!DONTDO)
            {
                if (!uidmatch)
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Owner of %s was %ju, setting to %ju", file, (uintmax_t)sb->st_uid,
                         (uintmax_t)uid);
                }

                if (!gidmatch)
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Group of %s was %ju, setting to %ju", file, (uintmax_t)sb->st_gid,
                         (uintmax_t)gid);
                }

                if (!S_ISLNK(sb->st_mode))
                {
                    if (chown(file, uid, gid) == -1)
                    {
                        cfPS(OUTPUT_LEVEL_INFORM, CF_DENIED, "chown", pp, attr, " !! Cannot set ownership on file %s!\n", file);
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "File %s is not owned by anybody in the passwd database\n", file);
                CfOut(OUTPUT_LEVEL_ERROR, "", "(uid = %ju,gid = %ju)\n", (uintmax_t)sb->st_uid, (uintmax_t)sb->st_gid);
                break;
            }

            if ((gp = getgrgid(sb->st_gid)) == NULL)
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! File %s is not owned by any group in group database\n",
                     file);
                break;
            }

            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! File %s is owned by [%s], group [%s]\n", file, pw->pw_name,
                 gp->gr_name);
            break;
        }
    }

    return false;
}

#endif /* !__MINGW32__ */

static void VerifyFileChanges(char *file, struct stat *sb, Attributes attr, Promise *pp)
{
    struct stat cmpsb;
    CF_DB *dbp;
    char message[CF_BUFSIZE];
    int ok = true;

    if ((attr.change.report_changes != FILE_CHANGE_REPORT_STATS_CHANGE) && (attr.change.report_changes != FILE_CHANGE_REPORT_ALL))
    {
        return;
    }

    if (!OpenDB(&dbp, dbid_filestats))
    {
        return;
    }

    if (!ReadDB(dbp, file, &cmpsb, sizeof(struct stat)))
    {
        if (!DONTDO)
        {
            WriteDB(dbp, file, sb, sizeof(struct stat));
            CloseDB(dbp);
            return;
        }
    }

    if (cmpsb.st_mode != sb->st_mode)
    {
        ok = false;
    }

    if (cmpsb.st_uid != sb->st_uid)
    {
        ok = false;
    }

    if (cmpsb.st_gid != sb->st_gid)
    {
        ok = false;
    }

    if (cmpsb.st_dev != sb->st_dev)
    {
        ok = false;
    }

    if (cmpsb.st_ino != sb->st_ino)
    {
        ok = false;
    }

    if (cmpsb.st_mtime != sb->st_mtime)
    {
        ok = false;
    }

    if (ok)
    {
        CloseDB(dbp);
        return;
    }

    if (cmpsb.st_mode != sb->st_mode)
    {
        snprintf(message, CF_BUFSIZE - 1, "ALERT: Permissions for %s changed %jo -> %jo", file,
                 (uintmax_t)cmpsb.st_mode, (uintmax_t)sb->st_mode);
        CfOut(OUTPUT_LEVEL_ERROR, "", "%s", message);

        char msg_temp[CF_MAXVARSIZE] = { 0 };
        snprintf(msg_temp, sizeof(msg_temp), "Permission: %jo -> %jo",
                 (uintmax_t)cmpsb.st_mode, (uintmax_t)sb->st_mode);

        LogHashChange(file, FILE_STATE_STATS_CHANGED, msg_temp, pp);
    }

    if (cmpsb.st_uid != sb->st_uid)
    {
        snprintf(message, CF_BUFSIZE - 1, "ALERT: owner for %s changed %jd -> %jd", file, (uintmax_t) cmpsb.st_uid,
                 (uintmax_t) sb->st_uid);
        CfOut(OUTPUT_LEVEL_ERROR, "", "%s", message);

        char msg_temp[CF_MAXVARSIZE] = { 0 };
        snprintf(msg_temp, sizeof(msg_temp), "Owner: %jd -> %jd",
                 (uintmax_t)cmpsb.st_uid, (uintmax_t)sb->st_uid);

        LogHashChange(file, FILE_STATE_STATS_CHANGED, msg_temp, pp);
    }

    if (cmpsb.st_gid != sb->st_gid)
    {
        snprintf(message, CF_BUFSIZE - 1, "ALERT: group for %s changed %jd -> %jd", file, (uintmax_t) cmpsb.st_gid,
                 (uintmax_t) sb->st_gid);
        CfOut(OUTPUT_LEVEL_ERROR, "", "%s", message);

        char msg_temp[CF_MAXVARSIZE] = { 0 };
        snprintf(msg_temp, sizeof(msg_temp), "Group: %jd -> %jd",
                 (uintmax_t)cmpsb.st_gid, (uintmax_t)sb->st_gid);

        LogHashChange(file, FILE_STATE_STATS_CHANGED, msg_temp, pp);
    }

    if (cmpsb.st_dev != sb->st_dev)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "ALERT: device for %s changed %jd -> %jd", file, (intmax_t) cmpsb.st_dev,
              (intmax_t) sb->st_dev);
    }

    if (cmpsb.st_ino != sb->st_ino)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "ALERT: inode for %s changed %ju -> %ju", file, (uintmax_t) cmpsb.st_ino,
              (uintmax_t) sb->st_ino);
    }

    if (cmpsb.st_mtime != sb->st_mtime)
    {
        char from[CF_MAXVARSIZE];
        char to[CF_MAXVARSIZE];

        strcpy(from, cf_ctime(&(cmpsb.st_mtime)));
        strcpy(to, cf_ctime(&(sb->st_mtime)));
        Chop(from, CF_MAXVARSIZE);
        Chop(to, CF_MAXVARSIZE);
        CfOut(OUTPUT_LEVEL_ERROR, "", "ALERT: Last modified time for %s changed %s -> %s", file, from, to);
    }

    if (pp->ref)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Preceding promise: %s", pp->ref);
    }

    if (attr.change.update && !DONTDO)
    {
        DeleteDB(dbp, file);
        WriteDB(dbp, file, sb, sizeof(struct stat));
    }

    CloseDB(dbp);
}

int CfCreateFile(char *file, Promise *pp, Attributes attr)
{
    int fd;

    /* If name ends in /. then this is a directory */

// attr.move_obstructions for MakeParentDirectory

    if (!IsAbsoluteFileName(file))
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "creat", pp, attr,
             " !! Cannot create a relative filename %s - has no invariant meaning\n", file);
        return false;
    }

    if (strcmp(".", ReadLastNode(file)) == 0)
    {
        CfDebug("File object \"%s \"seems to be a directory\n", file);

        if (!DONTDO && attr.transaction.action != cfa_warn)
        {
            if (!MakeParentDirectory(file, attr.move_obstructions))
            {
                cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "creat", pp, attr, " !! Error creating directories for %s\n", file);
                return false;
            }

            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Created directory %s\n", file);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Warning promised, need to create directory %s", file);
            return false;
        }
    }
    else
    {
        if (!DONTDO && attr.transaction.action != cfa_warn)
        {
            mode_t saveumask = umask(0);
            mode_t filemode = 0600;     /* Decide the mode for filecreation */

            if (ConstraintGetRvalValue("mode", pp, RVAL_TYPE_SCALAR) == NULL)
            {
                /* Relying on umask is risky */
                filemode = 0600;
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> No mode was set, choose plain file default %ju\n", (uintmax_t)filemode);
            }
            else
            {
                filemode = attr.perms.plus & ~(attr.perms.minus);
            }

            MakeParentDirectory(file, attr.move_obstructions);

            if ((fd = creat(file, filemode)) == -1)
            {
                cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "creat", pp, attr, " !! Error creating file %s, mode = %ju\n", file, (uintmax_t)filemode);
                umask(saveumask);
                return false;
            }
            else
            {
                cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Created file %s, mode = %ju\n", file, (uintmax_t)filemode);
                close(fd);
                umask(saveumask);
            }
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Warning promised, need to create file %s\n", file);
            return false;
        }
    }

    return true;
}

static int DeviceBoundary(struct stat *sb, Promise *pp)
{
    if (sb->st_dev == pp->rootdevice)
    {
        return false;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Device change from %jd to %jd\n", (intmax_t) pp->rootdevice, (intmax_t) sb->st_dev);
        return true;
    }
}

void SetSearchDevice(struct stat *sb, Promise *pp)
{
    CfDebug("Registering root device as %" PRIdMAX "\n", (intmax_t) sb->st_dev);
    pp->rootdevice = sb->st_dev;
}
