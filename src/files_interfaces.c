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

/*****************************************************************************/
/*                                                                           */
/* File: files_interfaces.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include "dir.h"
#include "files_names.h"

static void PurgeLocalFiles(Item *filelist, char *directory, Attributes attr, Promise *pp);
static void CfCopyFile(char *sourcefile, char *destfile, struct stat sourcestatbuf, Attributes attr, Promise *pp);
static int CompareForFileCopy(char *sourcefile, char *destfile, struct stat *ssb, struct stat *dsb, Attributes attr,
                              Promise *pp);
static void RegisterAHardLink(int i, char *value, Attributes attr, Promise *pp);
static void FileAutoDefine(char *destfile);
static void LoadSetuid(Attributes a, Promise *pp);
static void SaveSetuid(Attributes a, Promise *pp);

/*****************************************************************************/

/* File copying is a special case, particularly complex - cannot be integrated */

void SourceSearchAndCopy(char *from, char *to, int maxrecurse, Attributes attr, Promise *pp)
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
            cfPS(cf_error, CF_FAIL, "", pp, attr, "Unable to make directory for %s in file-copy %s to %s\n", newto,
                 attr.copy.source, attr.copy.destination);
            return;
        }

        DeleteSlash(to);

        /* Set aside symlinks */

        if (lstat(to, &tostat) != 0)
        {
            cfPS(cf_error, CF_WARN, "lstat", pp, attr, "Unable to stat newly created directory %s", to);
            return;
        }

        if (S_ISLNK(tostat.st_mode))
        {
            char backup[CF_BUFSIZE];
            mode_t mask;

            if (!attr.move_obstructions)
            {
                CfOut(cf_inform, "", "Path %s is a symlink. Unable to move it aside without move_obstructions is set",
                      to);
                return;
            }

            strcpy(backup, to);
            DeleteSlash(to);
            strcat(backup, ".cf-moved");

            if (cf_rename(to, backup) == -1)
            {
                CfOut(cf_inform, "", "Unable to backup old %s", to);
                unlink(to);
            }

            mask = umask(0);
            if (cf_mkdir(to, DEFAULTMODE) == -1)
            {
                CfOut(cf_error, "cf_mkdir", "Unable to make directory %s", to);
                umask(mask);
                return;
            }
            umask(mask);
        }
    }

    if ((dirh = OpenDirForPromise(from, attr, pp)) == NULL)
    {
        cfPS(cf_inform, CF_INTERPT, "", pp, attr, "copy can't open directory [%s]\n", from);
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

        if (attr.recursion.travlinks || attr.copy.link_type == cfa_notlinked)
        {
            /* No point in checking if there are untrusted symlinks here,
               since this is from a trusted source, by defintion */

            if (cf_stat(newfrom, &sb, attr, pp) == -1)
            {
                CfOut(cf_verbose, "cf_stat", " !! (Can't stat %s)\n", newfrom);
                continue;
            }
        }
        else
        {
            if (cf_lstat(newfrom, &sb, attr, pp) == -1)
            {
                CfOut(cf_verbose, "cf_stat", " !! (Can't stat %s)\n", newfrom);
                continue;
            }
        }

        /* If we are tracking subdirs in copy, then join else don't add */

        if (attr.copy.collapse)
        {
            if (!S_ISDIR(sb.st_mode) && !JoinPath(newto, dirp->d_name))
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

        if (attr.recursion.xdev && DeviceBoundary(&sb, pp))
        {
            CfOut(cf_verbose, "", " !! Skipping %s on different device\n", newfrom);
            continue;
        }

        if (S_ISDIR(sb.st_mode))
        {
            if (attr.recursion.travlinks)
            {
                CfOut(cf_verbose, "", "Traversing directory links during copy is too dangerous, pruned");
                continue;
            }

            if (SkipDirLinks(newfrom, dirp->d_name, attr.recursion))
            {
                continue;
            }

            memset(&dsb, 0, sizeof(struct stat));

            /* Only copy dirs if we are tracking subdirs */

            if (!attr.copy.collapse && (cfstat(newto, &dsb) == -1))
            {
                if (cf_mkdir(newto, 0700) == -1)
                {
                    cfPS(cf_error, CF_INTERPT, "cf_mkdir", pp, attr, " !! Can't make directory %s\n", newto);
                    continue;
                }

                if (cfstat(newto, &dsb) == -1)
                {
                    cfPS(cf_error, CF_INTERPT, "stat", pp, attr,
                         " !! Can't stat local copy %s - failed to establish directory\n", newto);
                    continue;
                }
            }

            CfOut(cf_verbose, "", " ->>  Entering %s\n", newto);

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

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

void VerifyFilePromise(char *path, Promise *pp)
{
    struct stat osb, oslb, dsb;
    Attributes a = { {0} };
    CfLock thislock;
    int exists, rlevel = 0;

    a = GetFilesAttributes(pp);

    if (!FileSanityChecks(path, a, pp))
    {
        return;
    }

    thislock = AcquireLock(path, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

    CF_OCCUR++;

    LoadSetuid(a, pp);

    if (lstat(path, &oslb) == -1)       /* Careful if the object is a link */
    {
        if (a.create || a.touch)
        {
            if (!CfCreateFile(path, pp, a))
            {
                SaveSetuid(a, pp);
                YieldCurrentLock(thislock);
                return;
            }
            else
            {
                exists = (lstat(path, &oslb) != -1);
            }
        }

        exists = false;
    }
    else
    {
        if (a.create || a.touch)
        {
            cfPS(cf_verbose, CF_NOP, "", pp, a, " -> File \"%s\" exists as promised", path);
        }
        exists = true;
    }

    if (a.havedelete && !exists)
    {
        cfPS(cf_verbose, CF_NOP, "", pp, a, " -> File \"%s\" does not exist as promised", path);
    }

    if (!a.havedepthsearch)     /* if the search is trivial, make sure that we are in the parent dir of the leaf */
    {
        char basedir[CF_BUFSIZE];

        CfDebug(" -> Direct file reference %s, no search implied\n", path);
        snprintf(basedir, sizeof(basedir), "%s", path);

        if (strcmp(ReadLastNode(basedir), ".") == 0)
        {
            // Handle /.  notation for deletion of directories
            ChopLastNode(basedir);
            ChopLastNode(path);
        }

        ChopLastNode(basedir);
        chdir(basedir);
    }

    if (exists && !VerifyFileLeaf(path, &oslb, a, pp))
    {
        if (!S_ISDIR(oslb.st_mode))
        {
            SaveSetuid(a, pp);
            YieldCurrentLock(thislock);
            return;
        }
    }

    if (cfstat(path, &osb) == -1)
    {
        if (a.create || a.touch)
        {
            if (!CfCreateFile(path, pp, a))
            {
                SaveSetuid(a, pp);
                YieldCurrentLock(thislock);
                return;
            }
            else
            {
                exists = true;
            }
        }
        else
        {
            exists = false;
        }
    }
    else
    {
        if (!S_ISDIR(osb.st_mode))
        {
            if (a.havedepthsearch)
            {
                CfOut(cf_inform, "",
                      "Warning: depth_search (recursion) is promised for a base object %s that is not a directory",
                      path);
                SaveSetuid(a, pp);
                YieldCurrentLock(thislock);
                return;
            }
        }

        exists = true;
    }

    if (a.link.link_children)
    {
        if (cfstat(a.link.source, &dsb) != -1)
        {
            if (!S_ISDIR(dsb.st_mode))
            {
                CfOut(cf_error, "", "Cannot promise to link the children of %s as it is not a directory!",
                      a.link.source);
                SaveSetuid(a, pp);
                YieldCurrentLock(thislock);
                return;
            }
        }
    }

/* Phase 1 - */

    if (exists && (a.havedelete || a.haverename || a.haveperms || a.havechange || a.transformer))
    {
        lstat(path, &oslb);     /* if doesn't exist have to stat again anyway */

        if (a.havedepthsearch)
        {
            SetSearchDevice(&oslb, pp);
        }

        DepthSearch(path, &oslb, rlevel, a, pp);

        /* normally searches do not include the base directory */

        if (a.recursion.include_basedir)
        {
            int save_search = a.havedepthsearch;

            /* Handle this node specially */

            a.havedepthsearch = false;
            DepthSearch(path, &oslb, rlevel, a, pp);
            a.havedepthsearch = save_search;
        }
        else
        {
            /* unless child nodes were repaired, set a promise kept class */
            if (!IsDefinedClass("repaired"))
            {
                cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Basedir \"%s\" not promising anything", path);
            }
        }

        if (a.change.report_changes == cfa_contentchange || a.change.report_changes == cfa_allchanges)
        {
            if (a.havedepthsearch)
            {
                PurgeHashes(NULL, a, pp);
            }
            else
            {
                PurgeHashes(path, a, pp);
            }
        }
    }

/* Phase 2a - copying is potentially threadable if no followup actions */

    if (a.havecopy)
    {
        ScheduleCopyOperation(path, a, pp);
    }

/* Phase 2b link after copy in case need file first */

    if (a.havelink && a.link.link_children)
    {
        ScheduleLinkChildrenOperation(path, a.link.source, 1, a, pp);
    }
    else if (a.havelink)
    {
        ScheduleLinkOperation(path, a.link.source, a, pp);
    }

/* Phase 3 - content editing */

    if (a.haveedit)
    {
        ScheduleEditOperation(path, a, pp);
    }

// Once more in case a file has been created as a result of editing or copying

    if (cfstat(path, &osb) != -1 && S_ISREG(osb.st_mode))
    {
        VerifyFileLeaf(path, &osb, a, pp);
    }

    SaveSetuid(a, pp);
    YieldCurrentLock(thislock);
}

/*********************************************************************/

void VerifyCopy(char *source, char *destination, Attributes attr, Promise *pp)
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

    if (attr.copy.link_type == cfa_notlinked)
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
        cfPS(cf_error, CF_FAIL, "", pp, attr, "Can't stat %s in verify copy\n", source);
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
            cfPS(cf_verbose, CF_FAIL, "opendir", pp, attr, "Can't open directory %s\n", sourcedir);
            DeleteClientCache(attr, pp);
            return;
        }

        /* Now check any overrides */

        if (cfstat(destdir, &dsb) == -1)
        {
            cfPS(cf_error, CF_FAIL, "stat", pp, attr, "Can't stat directory %s\n", destdir);
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

            if (attr.copy.link_type == cfa_notlinked)
            {
                if (cf_stat(sourcefile, &ssb, attr, pp) == -1)
                {
                    cfPS(cf_inform, CF_FAIL, "stat", pp, attr, "Can't stat source file (notlinked) %s\n", sourcefile);
                    DeleteClientCache(attr, pp);
                    return;
                }
            }
            else
            {
                if (cf_lstat(sourcefile, &ssb, attr, pp) == -1)
                {
                    cfPS(cf_inform, CF_FAIL, "lstat", pp, attr, "Can't stat source file %s\n", sourcefile);
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

/*********************************************************************/

static void PurgeLocalFiles(Item *filelist, char *localdir, Attributes attr, Promise *pp)
{
    Dir *dirh;
    struct stat sb;
    const struct dirent *dirp;
    char filename[CF_BUFSIZE] = { 0 };

    CfDebug("PurgeLocalFiles(%s)\n", localdir);

    if (strlen(localdir) < 2)
    {
        CfOut(cf_error, "", "Purge of %s denied -- too dangerous!", localdir);
        return;
    }

    /* If we purge with no authentication we wipe out EVERYTHING ! */

    if (pp->conn && !pp->conn->authenticated)
    {
        CfOut(cf_verbose, "", " !! Not purge local files %s - no authenticated contact with a source\n", localdir);
        return;
    }

    if (!attr.havedepthsearch)
    {
        CfOut(cf_verbose, "", " !! No depth search when copying %s so purging does not apply\n", localdir);
        return;
    }

/* chdir to minimize the risk of race exploits during copy (which is inherently dangerous) */

    if (chdir(localdir) == -1)
    {
        CfOut(cf_verbose, "chdir", "Can't chdir to local directory %s\n", localdir);
        return;
    }

    if ((dirh = OpenDirLocal(".")) == NULL)
    {
        CfOut(cf_verbose, "opendir", "Can't open local directory %s\n", localdir);
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
                CfOut(cf_inform, "", " !! Purging %s in copy dest directory\n", filename);

                if (lstat(filename, &sb) == -1)
                {
                    cfPS(cf_verbose, CF_INTERPT, "lstat", pp, attr, " !! Couldn't stat %s while purging\n", filename);
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
                        cfPS(cf_verbose, CF_INTERPT, "rmdir", pp, attr,
                             " !! Couldn't empty directory %s while purging\n", filename);
                    }

                    if (chdir("..") != 0)
                    {
                        CfOut(cf_error, "chdir", "!! Can't step out of directory \"%s\" before deletion", filename);
                    }

                    if (rmdir(filename) == -1)
                    {
                        cfPS(cf_verbose, CF_INTERPT, "rmdir", pp, attr,
                             " !! Couldn't remove directory %s while purging\n", filename);
                    }
                }
                else if (unlink(filename) == -1)
                {
                    cfPS(cf_verbose, CF_FAIL, "", pp, attr, " !! Couldn't delete %s while purging\n", filename);
                }
            }
        }
    }

    CloseDir(dirh);
}

/*********************************************************************/

static void CfCopyFile(char *sourcefile, char *destfile, struct stat ssb, Attributes attr, Promise *pp)
{
    char *server;
    const char *lastnode;
    struct stat dsb;
    int found;
    mode_t srcmode = ssb.st_mode;

    CfDebug("CopyFile(%s,%s)\n", sourcefile, destfile);

#ifdef MINGW
    if (attr.copy.copy_links != NULL)
    {
        CfOut(cf_verbose, "",
              "copy_from.copylink_patterns is ignored on Windows (source files cannot be symbolic links)");
    }
#endif /* MINGW */

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
        CfOut(cf_inform, "", " !! File copy promise loop: file/dir %s is its own source", sourcefile);
        return;
    }

    if (!SelectLeaf(sourcefile, &ssb, attr, pp))
    {
        CfDebug("Skipping non-selected file %s\n", sourcefile);
        return;
    }

    if (IsInListOfRegex(SINGLE_COPY_CACHE, destfile))
    {
        CfOut(cf_inform, "", " -> Skipping single-copied file %s\n", destfile);
        return;
    }

    if (attr.copy.link_type != cfa_notlinked)
    {
        lastnode = ReadLastNode(sourcefile);

        if (MatchRlistItem(attr.copy.link_instead, lastnode))
        {
            if (MatchRlistItem(attr.copy.copy_links, lastnode))
            {
                CfOut(cf_inform, "",
                      "File %s matches both copylink_patterns and linkcopy_patterns - promise loop (skipping)!",
                      sourcefile);
                return;
            }
            else
            {
                CfOut(cf_verbose, "", "Copy item %s marked for linking\n", sourcefile);
#ifdef MINGW
                CfOut(cf_verbose, "", "Links are not yet supported on Windows - copying instead\n", sourcefile);
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
        if ((S_ISLNK(dsb.st_mode) && (attr.copy.link_type == cfa_notlinked))
            || (S_ISLNK(dsb.st_mode) && !S_ISLNK(ssb.st_mode)))
        {
            if (!S_ISLNK(ssb.st_mode) && (attr.copy.type_check && (attr.copy.link_type != cfa_notlinked)))
            {
                cfPS(cf_error, CF_FAIL, "", pp, attr,
                     "file image exists but destination type is silly (file/dir/link doesn't match)\n");
                PromiseRef(cf_error, pp);
                return;
            }

            if (DONTDO)
            {
                CfOut(cf_verbose, "", "Need to remove old symbolic link %s to make way for copy\n", destfile);
            }
            else
            {
                if (unlink(destfile) == -1)
                {
                    cfPS(cf_error, CF_FAIL, "unlink", pp, attr, "Couldn't remove link %s", destfile);
                    return;
                }

                CfOut(cf_verbose, "", "Removing old symbolic link %s to make way for copy\n", destfile);
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
        if (ssb.st_size < attr.copy.min_size || ssb.st_size > attr.copy.max_size)
        {
            cfPS(cf_verbose, CF_NOP, "", pp, attr, " -> Source file %s size is not in the permitted safety range\n",
                 sourcefile);
            return;
        }
    }

    if (found == -1)
    {
        if (attr.transaction.action == cfa_warn)
        {
            cfPS(cf_error, CF_WARN, "", pp, attr, " !! Image file \"%s\" is non-existent and should be a copy of %s\n",
                 destfile, sourcefile);
            return;
        }

        if (S_ISREG(srcmode) || (S_ISLNK(srcmode) && attr.copy.link_type == cfa_notlinked))
        {
            if (DONTDO)
            {
                CfOut(cf_verbose, "", " -> %s wasn't at destination (needs copying)", destfile);
                return;
            }
            else
            {
                CfOut(cf_verbose, "", " -> %s wasn't at destination (copying)", destfile);

                if (server)
                {
                    CfOut(cf_inform, "", " -> Copying from %s:%s\n", server, sourcefile);
                }
                else
                {
                    CfOut(cf_inform, "", " -> Copying from localhost:%s\n", sourcefile);
                }
            }

            if (S_ISLNK(srcmode) && attr.copy.link_type != cfa_notlinked)
            {
                CfOut(cf_verbose, "", " -> %s is a symbolic link\n", sourcefile);
                LinkCopy(sourcefile, destfile, &ssb, attr, pp);
            }
            else if (CopyRegularFile(sourcefile, destfile, ssb, dsb, attr, pp))
            {
                if (cfstat(destfile, &dsb) == -1)
                {
                    CfOut(cf_error, "stat", "Can't stat destination file %s\n", destfile);
                }
                else
                {
                    VerifyCopiedFileAttributes(destfile, &dsb, &ssb, attr, pp);
                }

                if (server)
                {
                    cfPS(cf_verbose, CF_CHG, "", pp, attr, " -> Updated file from %s:%s\n", server, sourcefile);
                }
                else
                {
                    cfPS(cf_verbose, CF_CHG, "", pp, attr, " -> Updated file from localhost:%s\n", sourcefile);
                }

                if (SINGLE_COPY_LIST)
                {
                    IdempPrependRScalar(&SINGLE_COPY_CACHE, destfile, CF_SCALAR);
                }

                if (MatchRlistItem(AUTO_DEFINE_LIST, destfile))
                {
                    FileAutoDefine(destfile);
                }
            }
            else
            {
                if (server)
                {
                    cfPS(cf_inform, CF_FAIL, "", pp, attr, " !! Copy from %s:%s failed\n", server, sourcefile);
                }
                else
                {
                    cfPS(cf_inform, CF_FAIL, "", pp, attr, " !! Copy from localhost:%s failed\n", sourcefile);
                }
            }

            return;
        }

        if (S_ISFIFO(srcmode))
        {
#ifdef HAVE_MKFIFO
            if (DONTDO)
            {
                CfOut(cf_inform, "", "Need to make FIFO %s\n", destfile);
            }
            else if (mkfifo(destfile, srcmode))
            {
                cfPS(cf_error, CF_FAIL, "mkfifo", pp, attr, " !! Cannot create fifo `%s'", destfile);
                return;
            }

            cfPS(cf_inform, CF_CHG, "", pp, attr, " -> Created fifo %s", destfile);
#endif
        }
        else
        {
#ifndef MINGW                   // only regular files on windows
            if (S_ISBLK(srcmode) || S_ISCHR(srcmode) || S_ISSOCK(srcmode))
            {
                if (DONTDO)
                {
                    CfOut(cf_inform, "", "Make BLK/CHR/SOCK %s\n", destfile);
                }
                else if (mknod(destfile, srcmode, ssb.st_rdev))
                {
                    cfPS(cf_error, CF_FAIL, "mknod", pp, attr, " !! Cannot create special file `%s'", destfile);
                    return;
                }

                cfPS(cf_inform, CF_CHG, "mknod", pp, attr, " -> Created special file/device `%s'", destfile);
            }
#endif /* NOT MINGW */
        }

        if (S_ISLNK(srcmode) && attr.copy.link_type != cfa_notlinked)
        {
            LinkCopy(sourcefile, destfile, &ssb, attr, pp);
        }
    }
    else
    {
        int ok_to_copy = false;

        CfOut(cf_verbose, "", " -> Destination file \"%s\" already exists\n", destfile);

        if (attr.copy.compare == cfa_exists)
        {
            CfOut(cf_verbose, "", " -> Existence only is promised, no copying required\n");
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

        if (attr.copy.type_check && attr.copy.link_type != cfa_notlinked)
        {
            if ((S_ISDIR(dsb.st_mode) && !S_ISDIR(ssb.st_mode)) ||
                (S_ISREG(dsb.st_mode) && !S_ISREG(ssb.st_mode)) ||
                (S_ISBLK(dsb.st_mode) && !S_ISBLK(ssb.st_mode)) ||
                (S_ISCHR(dsb.st_mode) && !S_ISCHR(ssb.st_mode)) ||
                (S_ISSOCK(dsb.st_mode) && !S_ISSOCK(ssb.st_mode)) ||
                (S_ISFIFO(dsb.st_mode) && !S_ISFIFO(ssb.st_mode)) || (S_ISLNK(dsb.st_mode) && !S_ISLNK(ssb.st_mode)))

            {
                cfPS(cf_inform, CF_FAIL, "", pp, attr,
                     "Promised file copy %s exists but type mismatch with source=%s\n", destfile, sourcefile);
                return;
            }
        }

        if (ok_to_copy && (attr.transaction.action == cfa_warn))
        {
            cfPS(cf_error, CF_WARN, "", pp, attr, " !! Image file \"%s\" exists but is not up to date wrt %s\n",
                 destfile, sourcefile);
            cfPS(cf_error, CF_WARN, "", pp, attr, " !! Only a warning has been promised\n");
            return;
        }

        if (attr.copy.force_update || ok_to_copy || S_ISLNK(ssb.st_mode))       /* Always check links */
        {
            if (S_ISREG(srcmode) || attr.copy.link_type == cfa_notlinked)
            {
                if (DONTDO)
                {
                    CfOut(cf_error, "", "Should update file %s from source %s on %s", destfile, sourcefile, server);
                    return;
                }
                else
                {
                    if (server)
                    {
                        cfPS(cf_inform, CF_CHG, "", pp, attr, " -> Updated %s from source %s on %s", destfile,
                             sourcefile, server);
                    }
                    else
                    {
                        cfPS(cf_inform, CF_CHG, "", pp, attr, " -> Updated %s from source %s on localhost", destfile,
                             sourcefile);
                    }
                }

                if (MatchRlistItem(AUTO_DEFINE_LIST, destfile))
                {
                    FileAutoDefine(destfile);
                }

                if (CopyRegularFile(sourcefile, destfile, ssb, dsb, attr, pp))
                {
                    if (cfstat(destfile, &dsb) == -1)
                    {
                        cfPS(cf_error, CF_INTERPT, "stat", pp, attr, "Can't stat destination %s\n", destfile);
                    }
                    else
                    {
                        VerifyCopiedFileAttributes(destfile, &dsb, &ssb, attr, pp);
                    }

                    if (IsInListOfRegex(SINGLE_COPY_LIST, destfile))
                    {
                        IdempPrependRScalar(&SINGLE_COPY_CACHE, destfile, CF_SCALAR);
                    }
                }
                else
                {
                    cfPS(cf_error, CF_FAIL, "", pp, attr, "Was not able to copy %s to %s\n", sourcefile, destfile);
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

            if (IsInListOfRegex(SINGLE_COPY_LIST, destfile))
            {
                IdempPrependRScalar(&SINGLE_COPY_CACHE, destfile, CF_SCALAR);
            }

            cfPS(cf_verbose, CF_NOP, "", pp, attr, " -> File %s is an up to date copy of source\n", destfile);
        }
    }
}

/*********************************************************************/

int cfstat(const char *path, struct stat *buf)
{
#ifdef MINGW
    return NovaWin_stat(path, buf);
#else
    return stat(path, buf);
#endif
}

/*********************************************************************/

int cf_stat(char *file, struct stat *buf, Attributes attr, Promise *pp)
{
    int res;

    if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item, "localhost") == 0)
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

/*********************************************************************/

int cf_lstat(char *file, struct stat *buf, Attributes attr, Promise *pp)
{
    int res;

    if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item, "localhost") == 0)
    {
        res = lstat(file, buf);
        CheckForFileHoles(buf, pp);
        return res;
    }
    else
    {
        return cf_remote_stat(file, buf, "link", attr, pp);
    }
}

/*********************************************************************/

#ifndef MINGW

int cf_readlink(char *sourcefile, char *linkbuf, int buffsize, Attributes attr, Promise *pp)
 /* wrapper for network access */
{
    Stat *sp;

    memset(linkbuf, 0, buffsize);

    if (attr.copy.servers == NULL || strcmp(attr.copy.servers->item, "localhost") == 0)
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
                    cfPS(cf_error, CF_FAIL, "", pp, attr, "readlink value is too large in cfreadlink\n");
                    CfOut(cf_error, "", "Contained [%s]]n", sp->cf_readlink);
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

#endif /* NOT MINGW */

/*********************************************************************/

int CfReadLine(char *buff, int size, FILE *fp)
{
    char ch;

    buff[0] = '\0';
    buff[size - 1] = '\0';      /* mark end of buffer */

    if (fgets(buff, size, fp) == NULL)
    {
        *buff = '\0';           /* EOF */
        return false;
    }
    else
    {
        char *tmp;

        if ((tmp = strrchr(buff, '\n')) != NULL)
        {
            /* remove newline */
            *tmp = '\0';
        }
        else
        {
            /* The line was too long and truncated so, discard probable remainder */
            while (true)
            {
                if (feof(fp))
                {
                    break;
                }

                ch = fgetc(fp);

                if (ch == '\n')
                {
                    break;
                }
            }
        }
    }

    return true;
}

/*******************************************************************/

int FileSanityChecks(char *path, Attributes a, Promise *pp)
{
    if (a.havelink && a.havecopy)
    {
        CfOut(cf_error, "",
              " !! Promise constraint conflicts - %s file cannot both be a copy of and a link to the source", path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.havelink && !a.link.source)
    {
        CfOut(cf_error, "", " !! Promise to establish a link at %s has no source", path);
        PromiseRef(cf_error, pp);
        return false;
    }

/* We can't do this verification during parsing as we did not yet read the body,
 * so we can't distinguish between link and copy source. In post-verification
 * all bodies are already expanded, so we don't have the information either */

    if (a.havecopy && a.copy.source && !FullTextMatch(CF_ABSPATHRANGE, a.copy.source))
    {
        /* FIXME: somehow redo a PromiseRef to be able to embed it into a string */
        CfOut(cf_error, "", " !! Non-absolute path in source attribute (have no invariant meaning): %s", a.copy.source);
        PromiseRef(cf_error, pp);
        FatalError("Bailing out");
    }

    if (a.haveeditline && a.haveeditxml)
    {
        CfOut(cf_error, "", " !! Promise constraint conflicts - %s editing file as both line and xml makes no sense",
              path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.havedepthsearch && a.haveedit)
    {
        CfOut(cf_error, "", " !! Recursive depth_searches are not compatible with general file editing");
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.havedelete && (a.create || a.havecopy || a.haveedit || a.haverename))
    {
        CfOut(cf_error, "", " !! Promise constraint conflicts - %s cannot be deleted and exist at the same time", path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.haverename && (a.create || a.havecopy || a.haveedit))
    {
        CfOut(cf_error, "",
              " !! Promise constraint conflicts - %s cannot be renamed/moved and exist there at the same time", path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.havedelete && a.havedepthsearch && !a.haveselect)
    {
        CfOut(cf_error, "",
              " !! Dangerous or ambiguous promise - %s specifies recursive deletion but has no file selection criteria",
              path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.haveselect && !a.select.result)
    {
        CfOut(cf_error, "", " !! File select constraint body promised no result (check body definition)");
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.havedelete && a.haverename)
    {
        CfOut(cf_error, "", " !! File %s cannot promise both deletion and renaming", path);
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.havecopy && a.havedepthsearch && a.havedelete)
    {
        CfOut(cf_inform, "",
              " !! Warning: depth_search of %s applies to both delete and copy, but these refer to different searches (source/destination)",
              pp->promiser);
        PromiseRef(cf_inform, pp);
    }

    if (a.transaction.background && a.transaction.audit)
    {
        CfOut(cf_error, "", " !! Auditing cannot be performed on backgrounded promises (this might change).");
        PromiseRef(cf_error, pp);
        return false;
    }

    if ((a.havecopy || a.havelink) && a.transformer)
    {
        CfOut(cf_error, "", " !! File object(s) %s cannot both be a copy of source and transformed simultaneously",
              pp->promiser);
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.haveselect && a.select.result == NULL)
    {
        CfOut(cf_error, "", " !! Missing file_result attribute in file_select body");
        PromiseRef(cf_error, pp);
        return false;
    }

    if (a.havedepthsearch && a.change.report_diffs)
    {
        CfOut(cf_error, "", " !! Difference reporting is not allowed during a depth_search");
        PromiseRef(cf_error, pp);
        return false;
    }

    return true;
}

/*********************************************************************/

static void LoadSetuid(Attributes a, Promise *pp)
{
    Attributes b = { {0} };
    char filename[CF_BUFSIZE];

    b = a;
    b.edits.backup = cfa_nobackup;
    b.edits.maxfilesize = 1000000;

    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", CFWORKDIR, VSYSNAME.nodename);
    MapName(filename);

    if (!LoadFileAsItemList(&VSETUIDLIST, filename, b, pp))
    {
        CfOut(cf_verbose, "", "Did not find any previous setuid log %s, creating a new one", filename);
    }
}

/*********************************************************************/

static void SaveSetuid(Attributes a, Promise *pp)
{
    Attributes b = { {0} };
    char filename[CF_BUFSIZE];

    b = a;
    b.edits.backup = cfa_nobackup;
    b.edits.maxfilesize = 1000000;

    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", CFWORKDIR, VSYSNAME.nodename);
    MapName(filename);

    PurgeItemList(&VSETUIDLIST, "SETUID/SETGID");

    if (!CompareToFile(VSETUIDLIST, filename, a, pp))
    {
        SaveItemListAsFile(VSETUIDLIST, filename, b, pp);
    }

    DeleteItemList(VSETUIDLIST);
    VSETUIDLIST = NULL;
}

/*********************************************************************/
/* Level 4                                                           */
/*********************************************************************/

static int CompareForFileCopy(char *sourcefile, char *destfile, struct stat *ssb, struct stat *dsb, Attributes attr,
                              Promise *pp)
{
    int ok_to_copy;

    switch (attr.copy.compare)
    {
    case cfa_checksum:
    case cfa_hash:

        if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
        {
            ok_to_copy = CompareFileHashes(sourcefile, destfile, ssb, dsb, attr, pp);
        }
        else
        {
            CfOut(cf_verbose, "", "Checksum comparison replaced by ctime: files not regular\n");
            ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);
        }

        if (ok_to_copy)
        {
            CfOut(cf_verbose, "", " !! Image file %s has a wrong digest/checksum (should be copy of %s)\n", destfile,
                  sourcefile);
            return ok_to_copy;
        }
        break;

    case cfa_binary:

        if (S_ISREG(dsb->st_mode) && S_ISREG(ssb->st_mode))
        {
            ok_to_copy = CompareBinaryFiles(sourcefile, destfile, ssb, dsb, attr, pp);
        }
        else
        {
            CfOut(cf_verbose, "", "Byte comparison replaced by ctime: files not regular\n");
            ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);
        }

        if (ok_to_copy)
        {
            CfOut(cf_verbose, "", " !! Image file %s has a wrong binary checksum (should be copy of %s)\n", destfile,
                  sourcefile);
            return ok_to_copy;
        }
        break;

    case cfa_mtime:

        ok_to_copy = (dsb->st_mtime < ssb->st_mtime);

        if (ok_to_copy)
        {
            CfOut(cf_verbose, "", " !! Image file %s out of date (should be copy of %s)\n", destfile, sourcefile);
            return ok_to_copy;
        }
        break;

    case cfa_atime:

        ok_to_copy = (dsb->st_ctime < ssb->st_ctime) ||
            (dsb->st_mtime < ssb->st_mtime) || CompareBinaryFiles(sourcefile, destfile, ssb, dsb, attr, pp);

        if (ok_to_copy)
        {
            CfOut(cf_verbose, "", " !! Image file %s seems out of date (should be copy of %s)\n", destfile, sourcefile);
            return ok_to_copy;
        }
        break;

    default:
        ok_to_copy = (dsb->st_ctime < ssb->st_ctime) || (dsb->st_mtime < ssb->st_mtime);

        if (ok_to_copy)
        {
            CfOut(cf_verbose, "", " !! Image file %s out of date (should be copy of %s)\n", destfile, sourcefile);
            return ok_to_copy;
        }
        break;
    }

    return false;
}

/*************************************************************************************/

void LinkCopy(char *sourcefile, char *destfile, struct stat *sb, Attributes attr, Promise *pp)
/* Link the file to the source, instead of copying */
#ifdef MINGW
{
    CfOut(cf_verbose, "", "Windows does not support symbolic links");
    cfPS(cf_error, CF_FAIL, "", pp, attr, "Windows can't link \"%s\" to \"%s\"", sourcefile, destfile);
}
#else                           /* NOT MINGW */
{
    char linkbuf[CF_BUFSIZE];
    const char *lastnode;
    int status = CF_UNKNOWN;
    struct stat dsb;

    linkbuf[0] = '\0';

    if (S_ISLNK(sb->st_mode) && cf_readlink(sourcefile, linkbuf, CF_BUFSIZE, attr, pp) == -1)
    {
        cfPS(cf_error, CF_FAIL, "", pp, attr, "Can't readlink %s\n", sourcefile);
        return;
    }
    else if (S_ISLNK(sb->st_mode))
    {
        CfOut(cf_verbose, "", "Checking link from %s to %s\n", destfile, linkbuf);

        if (attr.copy.link_type == cfa_absolute && !IsAbsoluteFileName(linkbuf))        /* Not absolute path - must fix */
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
        CfOut(cf_verbose, "", "cfengine: link item in copy %s marked for copying from %s instead\n", sourcefile,
              linkbuf);
        cfstat(linkbuf, &ssb);
        CfCopyFile(linkbuf, destfile, ssb, attr, pp);
        return;
    }

    switch (attr.copy.link_type)
    {
    case cfa_symlink:

        if (*linkbuf == '.')
        {
            status = VerifyRelativeLink(destfile, linkbuf, attr, pp);
        }
        else
        {
            status = VerifyLink(destfile, linkbuf, attr, pp);
        }
        break;

    case cfa_relative:
        status = VerifyRelativeLink(destfile, linkbuf, attr, pp);
        break;

    case cfa_absolute:
        status = VerifyAbsoluteLink(destfile, linkbuf, attr, pp);
        break;

    case cfa_hardlink:
        status = VerifyHardLink(destfile, linkbuf, attr, pp);
        break;

    default:
        FatalError("LinkCopy software error");
        return;
    }

    if (status == CF_CHG || status == CF_NOP)
    {
        if (lstat(destfile, &dsb) == -1)
        {
            CfOut(cf_error, "lstat", "Can't lstat %s\n", destfile);
        }
        else
        {
            VerifyCopiedFileAttributes(destfile, &dsb, sb, attr, pp);
        }

        if (status == CF_CHG)
        {
            cfPS(cf_inform, status, "", pp, attr, " -> Created link %s", destfile);
        }
        else if (status == CF_NOP)
        {
            cfPS(cf_inform, status, "", pp, attr, " -> Link %s as promised", destfile);
        }
        else
        {
            cfPS(cf_inform, status, "", pp, attr, " -> Unable to create link %s", destfile);
        }
    }
}
#endif /* NOT MINGW */

/*************************************************************************************/

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

#ifdef DARWIN
/* For later copy from new to dest */
    char *rsrcbuf;
    int rsrcbytesr;             /* read */
    int rsrcbytesw;             /* written */
    int rsrcbytesl;             /* to read */
    int rsrcrd;
    int rsrcwd;

/* Keep track of if a resrouce fork */
    char *tmpstr;
    char *forkpointer;
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

    discardbackup = (attr.copy.backup == cfa_nobackup || attr.copy.backup == cfa_repos_store);

    if (DONTDO)
    {
        CfOut(cf_error, "", "Promise requires copy from %s to %s\n", source, dest);
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
        if (CompressedArrayElementExists(pp->inode_cache, sstat.st_ino) && (strcmp(dest, linkable) != 0))
        {
            unlink(dest);
            MakeHardLink(dest, linkable, attr, pp);
            return true;
        }
    }

    if (attr.copy.servers != NULL && strcmp(attr.copy.servers->item, "localhost") != 0)
    {
        CfDebug("This is a remote copy from server: %s\n", (char *) attr.copy.servers->item);
        remote = true;
    }

#ifdef DARWIN
    if (strstr(dest, _PATH_RSRCFORKSPEC))
    {
        char *tempstr = xstrndup(dest, CF_BUFSIZE);

        rsrcfork = 1;
        /* Drop _PATH_RSRCFORKSPEC */
        forkpointer = strstr(tmpstr, _PATH_RSRCFORKSPEC);
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
            CfOut(cf_error, "", "Unable to construct filename for copy");
            return false;
        }

#ifdef DARWIN
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
        if (!CopyRegularFileDisk(source, new, attr, pp))
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

    CfOut(cf_verbose, "", " -> Copy of regular file succeeded %s to %s\n", source, new);

    backup[0] = '\0';

    if (!discardbackup)
    {
        char stamp[CF_BUFSIZE];
        time_t stampnow;

        CfDebug("Backup file %s\n", source);

        strncpy(backup, dest, CF_BUFSIZE);

        if (attr.copy.backup == cfa_timestamp)
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
        cfPS(cf_inform, CF_FAIL, "stat", pp, attr, "Can't stat new file %s - another agent has picked it up?\n", new);
        return false;
    }

    if (S_ISREG(dstat.st_mode) && dstat.st_size != sstat.st_size)
    {
        cfPS(cf_error, CF_FAIL, "", pp, attr,
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
        CfOut(cf_verbose, "", " ?? Final verification of transmission ...\n");

        if (CompareFileHashes(source, new, &sstat, &dstat, attr, pp))
        {
            cfPS(cf_verbose, CF_FAIL, "", pp, attr,
                 " !! New file %s seems to have been corrupted in transit, aborting!\n", new);

            if (backupok)
            {
                cf_rename(backup, dest);
            }

            return false;
        }
        else
        {
            CfOut(cf_verbose, "", " -> New file %s transmitted correctly - verified\n", new);
        }
    }

#ifdef DARWIN
    if (rsrcfork)
    {                           /* Can't just "mv" the resource fork, unfortunately */
        rsrcrd = open(new, O_RDONLY | O_BINARY);
        rsrcwd = open(dest, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0600);

        if (rsrcrd == -1 || rsrcwd == -1)
        {
            CfOut(cf_inform, "open", "Open of Darwin resource fork rsrcrd/rsrcwd failed\n");
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
                    CfOut(cf_inform, "read", "Read of Darwin resource fork rsrcrd failed\n");
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
                        CfOut(cf_inform, "write", "Write of Darwin resource fork rsrcwd failed\n");
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
            cfPS(cf_error, CF_FAIL, "cf_rename", pp, attr,
                 " !! Could not install copy file as %s, directory in the way?\n", dest);

            if (backupok)
            {
                cf_rename(backup, dest);        /* ignore failure */
            }

            return false;
        }

#ifdef DARWIN
    }
#endif

    if (!discardbackup && backupisdir)
    {
        CfOut(cf_inform, "", "Cannot move a directory to repository, leaving at %s", backup);
    }
    else if (!discardbackup && ArchiveToRepository(backup, attr, pp))
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

/*********************************************************************/

static void FileAutoDefine(char *destfile)
{
    char class[CF_MAXVARSIZE];

    snprintf(class, CF_MAXVARSIZE, "auto_%s", CanonifyName(destfile));
    NewClass(class);
    CfOut(cf_inform, "", "Auto defining class %s\n", class);
}

/*********************************************************************/
/* Level 3                                                           */
/*********************************************************************/

static void RegisterAHardLink(int i, char *value, Attributes attr, Promise *pp)
{
    if (!FixCompressedArrayValue(i, value, &(pp->inode_cache)))
    {
        /* Not root hard link, remove to preserve consistency */
        if (DONTDO)
        {
            CfOut(cf_verbose, "", " !! Need to remove old hard link %s to preserve structure..\n", value);
        }
        else
        {
            if (attr.transaction.action == cfa_warn)
            {
                CfOut(cf_verbose, "", " !! Need to remove old hard link %s to preserve structure..\n", value);
            }
            else
            {
                CfOut(cf_verbose, "", " -> Removing old hard link %s to preserve structure..\n", value);
                unlink(value);
            }
        }
    }
}
