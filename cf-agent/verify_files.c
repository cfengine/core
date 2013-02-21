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

#include "promises.h"
#include "vars.h"
#include "dir.h"
#include "scope.h"
#include "env_context.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_lib.h"
#include "files_operators.h"
#include "files_hashes.h"
#include "files_edit.h"
#include "files_properties.h"
#include "item_lib.h"
#include "matching.h"
#include "attributes.h"
#include "cfstream.h"
#include "transaction.h"
#include "string_lib.h"
#include "verify_files_utils.h"
#include "logging.h"
#include "generic_agent.h" // HashVariables
#include "misc_lib.h"
#include "fncall.h"


static void LoadSetuid(Attributes a, Promise *pp);
static void SaveSetuid(Attributes a, Promise *pp);
static void FindFilePromiserObjects(Promise *pp, const ReportContext *report_context);

/*****************************************************************************/

void LocateFilePromiserGroup(char *wildpath, Promise *pp, void (*fnptr) (char *path, Promise *ptr, const ReportContext *report_context),
                             const ReportContext *report_context) /* FIXME */
{
    Item *path, *ip, *remainder = NULL;
    char pbuffer[CF_BUFSIZE];
    struct stat statbuf;
    int count = 0, lastnode = false, expandregex = false;
    uid_t agentuid = getuid();
    int create = PromiseGetConstraintAsBoolean("create", pp);
    char *pathtype = ConstraintGetRvalValue("pathtype", pp, RVAL_TYPE_SCALAR);

    CfDebug("LocateFilePromiserGroup(%s)\n", wildpath);

/* Do a search for promiser objects matching wildpath */

    if ((!IsPathRegex(wildpath)) || (pathtype && (strcmp(pathtype, "literal") == 0)))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using literal pathtype for %s\n", wildpath);
        (*fnptr) (wildpath, pp, report_context);
        return;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using regex pathtype for %s (see pathtype)\n", wildpath);
    }

    pbuffer[0] = '\0';
    path = SplitString(wildpath, '/');  // require forward slash in regex on all platforms

    for (ip = path; ip != NULL; ip = ip->next)
    {
        if ((ip->name == NULL) || (strlen(ip->name) == 0))
        {
            continue;
        }

        if (ip->next == NULL)
        {
            lastnode = true;
        }

        /* No need to chdir as in recursive descent, since we know about the path here */

        if (IsRegex(ip->name))
        {
            remainder = ip->next;
            expandregex = true;
            break;
        }
        else
        {
            expandregex = false;
        }

        if (!JoinPath(pbuffer, ip->name))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Buffer has limited size in LocateFilePromiserGroup\n");
            return;
        }

        if (cfstat(pbuffer, &statbuf) != -1)
        {
            if ((S_ISDIR(statbuf.st_mode)) && ((statbuf.st_uid) != agentuid) && ((statbuf.st_uid) != 0))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "",
                      "Directory %s in search path %s is controlled by another user (uid %ju) - trusting its content is potentially risky (possible race)\n",
                      pbuffer, wildpath, (uintmax_t)statbuf.st_uid);
                PromiseRef(OUTPUT_LEVEL_INFORM, pp);
            }
        }
    }

    if (expandregex)            /* Expand one regex link and hand down */
    {
        char nextbuffer[CF_BUFSIZE], nextbufferOrig[CF_BUFSIZE], regex[CF_BUFSIZE];
        const struct dirent *dirp;
        Dir *dirh;
        Attributes dummyattr = { {0} };

        memset(&dummyattr, 0, sizeof(dummyattr));
        memset(regex, 0, CF_BUFSIZE);

        strncpy(regex, ip->name, CF_BUFSIZE - 1);

        if ((dirh = OpenDirLocal(pbuffer)) == NULL)
        {
            // Could be a dummy directory to be created so this is not an error.
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using best-effort expanded (but non-existent) file base path %s\n", wildpath);
            (*fnptr) (wildpath, pp, report_context);
            DeleteItemList(path);
            return;
        }
        else
        {
            count = 0;

            for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
            {
                if (!ConsiderFile(dirp->d_name, pbuffer, dummyattr, pp))
                {
                    continue;
                }

                if ((!lastnode) && (!S_ISDIR(statbuf.st_mode)))
                {
                    CfDebug("Skipping non-directory %s\n", dirp->d_name);
                    continue;
                }

                if (FullTextMatch(regex, dirp->d_name))
                {
                    CfDebug("Link %s matched regex %s\n", dirp->d_name, regex);
                }
                else
                {
                    continue;
                }

                count++;

                strncpy(nextbuffer, pbuffer, CF_BUFSIZE - 1);
                AddSlash(nextbuffer);
                strcat(nextbuffer, dirp->d_name);

                for (ip = remainder; ip != NULL; ip = ip->next)
                {
                    AddSlash(nextbuffer);
                    strcat(nextbuffer, ip->name);
                }

                /* The next level might still contain regexs, so go again as long as expansion is not nullpotent */

                if ((!lastnode) && (strcmp(nextbuffer, wildpath) != 0))
                {
                    LocateFilePromiserGroup(nextbuffer, pp, fnptr, report_context);
                }
                else
                {
                    Promise *pcopy;

                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using expanded file base path %s\n", nextbuffer);

                    /* Now need to recompute any back references to get the complete path */

                    snprintf(nextbufferOrig, sizeof(nextbufferOrig), "%s", nextbuffer);
                    MapNameForward(nextbuffer);

                    if (!FullTextMatch(pp->promiser, nextbuffer))
                    {
                        CfDebug("Error recomputing references for \"%s\" in: %s", pp->promiser, nextbuffer);
                    }

                    /* If there were back references there could still be match.x vars to expand */

                    pcopy = ExpandDeRefPromise(CONTEXTID, pp);
                    (*fnptr) (nextbufferOrig, pcopy, report_context);
                    PromiseDestroy(pcopy);
                }
            }

            CloseDir(dirh);
        }
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Using file base path %s\n", pbuffer);
        (*fnptr) (pbuffer, pp, report_context);
    }

    if (count == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "No promiser file objects matched as regular expression %s\n", wildpath);

        if (create)
        {
            VerifyFilePromise(pp->promiser, pp, report_context);
        }
    }

    DeleteItemList(path);
}

void VerifyFilePromise(char *path, Promise *pp, const ReportContext *report_context)
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

    DeleteScalar("this", "promiser");
    NewScalar("this", "promiser", path, DATA_TYPE_STRING); 
    
    thislock = AcquireLock(path, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

    CF_OCCUR++;

    LoadSetuid(a, pp);

    if (lstat(path, &oslb) == -1)       /* Careful if the object is a link */
    {
        if ((a.create) || (a.touch))
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
        if ((a.create) || (a.touch))
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> File \"%s\" exists as promised", path);
        }
        exists = true;
    }

    if ((a.havedelete) && (!exists))
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> File \"%s\" does not exist as promised", path);
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
        if (chdir(basedir))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to chdir into '%s'\n", basedir);
        }
    }

    if (exists && (!VerifyFileLeaf(path, &oslb, a, pp)))
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
        if ((a.create) || (a.touch))
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
                CfOut(OUTPUT_LEVEL_INFORM, "",
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "Cannot promise to link the children of %s as it is not a directory!",
                      a.link.source);
                SaveSetuid(a, pp);
                YieldCurrentLock(thislock);
                return;
            }
        }
    }

/* Phase 1 - */

    if (exists && ((a.havedelete) || (a.haverename) || (a.haveperms) || (a.havechange) || (a.transformer)))
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
            if (!IsDefinedClass("repaired" , pp->ns))
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Basedir \"%s\" not promising anything", path);
            }
        }

        if (((a.change.report_changes) == FILE_CHANGE_REPORT_CONTENT_CHANGE) || ((a.change.report_changes) == FILE_CHANGE_REPORT_ALL))
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

    if ((a.havelink) && (a.link.link_children))
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
        ScheduleEditOperation(path, a, pp, report_context);
    }

// Once more in case a file has been created as a result of editing or copying

    if ((cfstat(path, &osb) != -1) && (S_ISREG(osb.st_mode)))
    {
        VerifyFileLeaf(path, &osb, a, pp);
    }

    SaveSetuid(a, pp);
    YieldCurrentLock(thislock);
}

/*****************************************************************************/

int ScheduleEditOperation(char *filename, Attributes a, Promise *pp, const ReportContext *report_context)
{
    Bundle *bp;
    void *vp;
    FnCall *fp;
    char edit_bundle_name[CF_BUFSIZE], lockname[CF_BUFSIZE], qualified_edit[CF_BUFSIZE], *method_deref;
    Rlist *params = { 0 };
    int retval = false;
    CfLock thislock;

    snprintf(lockname, CF_BUFSIZE - 1, "fileedit-%s", filename);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return false;
    }

    pp->edcontext = NewEditContext(filename, a, pp);

    if (pp->edcontext == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "File %s was marked for editing but could not be opened\n", filename);
        FinishEditContext(pp->edcontext, a, pp);
        YieldCurrentLock(thislock);
        return false;
    }

    Policy *policy = PolicyFromPromise(pp);

    if (a.haveeditline)
    {
        if ((vp = ConstraintGetRvalValue("edit_line", pp, RVAL_TYPE_FNCALL)))
        {
            fp = (FnCall *) vp;
            strcpy(edit_bundle_name, fp->name);
            params = fp->args;
        }
        else if ((vp = ConstraintGetRvalValue("edit_line", pp, RVAL_TYPE_SCALAR)))
        {
            strcpy(edit_bundle_name, (char *) vp);
            params = NULL;
        }             
        else
        {
            FinishEditContext(pp->edcontext, a, pp);
            YieldCurrentLock(thislock);
            return false;
        }

        if (strncmp(edit_bundle_name,"default:",strlen("default:")) == 0) // CF_NS == ':'
        {
            method_deref = strchr(edit_bundle_name, CF_NS) + 1;
        }
        else if ((strchr(edit_bundle_name, CF_NS) == NULL) && (strcmp(pp->ns, "default") != 0))
        {
            snprintf(qualified_edit, CF_BUFSIZE, "%s%c%s", pp->ns, CF_NS, edit_bundle_name);
            method_deref = qualified_edit;
        }
        else            
        {
            method_deref = edit_bundle_name;
        }        

        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Handling file edits in edit_line bundle %s\n", method_deref);

        // add current filename to context - already there?
        if ((bp = PolicyGetBundle(policy, NULL, "edit_line", method_deref)))
        {
            BannerSubBundle(bp, params);

            DeleteScope(bp->name);
            NewScope(bp->name);
            HashVariables(policy, bp->name, report_context);

            AugmentScope(bp->name, bp->ns, bp->args, params);
            PushPrivateClassContext(a.edits.inherit);
            retval = ScheduleEditLineOperations(filename, bp, a, pp, report_context);
            PopPrivateClassContext();
            DeleteScope(bp->name);
        }
        else
           {
           printf("DIDN*T FIND %s ... %s \n", method_deref, edit_bundle_name);
           }
    }


    if (a.haveeditxml)
    {
        if ((vp = ConstraintGetRvalValue("edit_xml", pp, RVAL_TYPE_FNCALL)))
        {
            fp = (FnCall *) vp;
            strcpy(edit_bundle_name, fp->name);
            params = fp->args;
        }
        else if ((vp = ConstraintGetRvalValue("edit_xml", pp, RVAL_TYPE_SCALAR)))
        {
            strcpy(edit_bundle_name, (char *) vp);
            params = NULL;
        }
        else
        {
            FinishEditContext(pp->edcontext, a, pp);
            YieldCurrentLock(thislock);
            return false;
        }

        if (strncmp(edit_bundle_name,"default:",strlen("default:")) == 0) // CF_NS == ':'
           {
           method_deref = strchr(edit_bundle_name, CF_NS) + 1;
           }
        else
           {
           method_deref = edit_bundle_name;
           }
        
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Handling file edits in edit_xml bundle %s\n", method_deref);

        if ((bp = PolicyGetBundle(policy, NULL, "edit_xml", method_deref)))
        {
            BannerSubBundle(bp, params);

            DeleteScope(bp->name);
            NewScope(bp->name);
            HashVariables(policy, bp->name, report_context);

            AugmentScope(bp->name, bp->ns, bp->args, params);
            PushPrivateClassContext(a.edits.inherit);
            retval = ScheduleEditXmlOperations(filename, bp, a, pp, report_context);
            PopPrivateClassContext();
            DeleteScope(bp->name);
        }
    }

    
    if (a.template)
    {
        if ((bp = MakeTemporaryBundleFromTemplate(a,pp)))
        {
            BannerSubBundle(bp,params);
            a.haveeditline = true;

            DeleteScope(bp->name);
            NewScope(bp->name);
            HashVariables(policy, bp->name, report_context);

            PushPrivateClassContext(a.edits.inherit);
            retval = ScheduleEditLineOperations(filename, bp, a, pp, report_context);
            PopPrivateClassContext();
            DeleteScope(bp->name);
        }
        // FIXME: why it crashes? DeleteBundles(bp);
    }

    FinishEditContext(pp->edcontext, a, pp);
    YieldCurrentLock(thislock);
    return retval;
}

/*****************************************************************************/

void *FindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context)
{
    PromiseBanner(pp);
    FindFilePromiserObjects(pp, report_context);

    if (AM_BACKGROUND_PROCESS && (!pp->done))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Exiting backgrounded promise");
        PromiseRef(OUTPUT_LEVEL_VERBOSE, pp);
        exit(0);
    }

    return (void *) NULL;
}

/*****************************************************************************/

static void FindFilePromiserObjects(Promise *pp, const ReportContext *report_context)
{
    char *val = ConstraintGetRvalValue("pathtype", pp, RVAL_TYPE_SCALAR);
    int literal = (PromiseGetConstraintAsBoolean("copy_from", pp)) || ((val != NULL) && (strcmp(val, "literal") == 0));

/* Check if we are searching over a regular expression */

    if (literal)
    {
        // Prime the promiser temporarily, may override later
        NewScalar("this", "promiser", pp->promiser, DATA_TYPE_STRING);
        VerifyFilePromise(pp->promiser, pp, report_context);
    }
    else                        // Default is to expand regex paths
    {
        LocateFilePromiserGroup(pp->promiser, pp, VerifyFilePromise, report_context);
    }
}

static void LoadSetuid(Attributes a, Promise *pp)
{
    Attributes b = { {0} };
    char filename[CF_BUFSIZE];

    b = a;
    b.edits.backup = BACKUP_OPTION_NO_BACKUP;
    b.edits.maxfilesize = 1000000;

    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", CFWORKDIR, VSYSNAME.nodename);
    MapName(filename);

    if (!LoadFileAsItemList(&VSETUIDLIST, filename, b, pp))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Did not find any previous setuid log %s, creating a new one", filename);
    }
}

/*********************************************************************/

static void SaveSetuid(Attributes a, Promise *pp)
{
    Attributes b = { {0} };
    char filename[CF_BUFSIZE];

    b = a;
    b.edits.backup = BACKUP_OPTION_NO_BACKUP;
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

