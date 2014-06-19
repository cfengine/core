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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <verify_files.h>

#include <actuator.h>
#include <promises.h>
#include <vars.h>
#include <dir.h>
#include <scope.h>
#include <eval_context.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_lib.h>
#include <files_operators.h>
#include <files_hashes.h>
#include <files_edit.h>
#include <files_editxml.h>
#include <files_editline.h>
#include <files_properties.h>
#include <files_select.h>
#include <item_lib.h>
#include <match_scope.h>
#include <attributes.h>
#include <locks.h>
#include <string_lib.h>
#include <verify_files_utils.h>
#include <verify_files_hashes.h>
#include <misc_lib.h>
#include <fncall.h>
#include <promiser_regex_resolver.h>
#include <ornaments.h>
#include <audit.h>
#include <expand.h>
#include <mustache.h>
#include <known_dirs.h>
#include <evalfunction.h>

static void LoadSetuid(void);
static void SaveSetuid(void);
static PromiseResult FindFilePromiserObjects(EvalContext *ctx, const Promise *pp);
static PromiseResult VerifyFilePromise(EvalContext *ctx, char *path, const Promise *pp);

/*****************************************************************************/

static int FileSanityChecks(char *path, Attributes a, const Promise *pp)
{
    if ((a.havelink) && (a.havecopy))
    {
        Log(LOG_LEVEL_ERR,
            "Promise constraint conflicts - '%s' file cannot both be a copy of and a link to the source", path);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    /* We can't do this verification during parsing as we did not yet read the
     * body, so we can't distinguish between link and copy source. In
     * post-verification all bodies are already expanded, so we don't have the
     * information either */
    if ((a.havelink) && (!a.link.source))
    {
        Log(LOG_LEVEL_ERR, "Promise to establish a link at '%s' has no source", path);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.haveeditline) && (a.haveeditxml))
    {
        Log(LOG_LEVEL_ERR, "Promise constraint conflicts - '%s' editing file as both line and xml makes no sense",
              path);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.havedepthsearch) && (a.haveedit))
    {
        Log(LOG_LEVEL_ERR, "Recursive depth_searches are not compatible with general file editing");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.havedelete) && ((a.create) || (a.havecopy) || (a.haveedit) || (a.haverename)))
    {
        Log(LOG_LEVEL_ERR, "Promise constraint conflicts - '%s' cannot be deleted and exist at the same time", path);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.haverename) && ((a.create) || (a.havecopy) || (a.haveedit)))
    {
        Log(LOG_LEVEL_ERR,
            "Promise constraint conflicts - '%s' cannot be renamed/moved and exist there at the same time", path);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.havedelete) && (a.havedepthsearch) && (!a.haveselect))
    {
        Log(LOG_LEVEL_ERR,
            "Dangerous or ambiguous promise - '%s' specifies recursive deletion but has no file selection criteria",
              path);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.haveselect) && (!a.select.result))
    {
        Log(LOG_LEVEL_ERR, "File select constraint body promised no result (check body definition)");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.havedelete) && (a.haverename))
    {
        Log(LOG_LEVEL_ERR, "File '%s' cannot promise both deletion and renaming", path);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.havecopy) && (a.havedepthsearch) && (a.havedelete))
    {
        Log(LOG_LEVEL_WARNING,
            "depth_search of '%s' applies to both delete and copy, but these refer to different searches (source/destination)",
              pp->promiser);
        PromiseRef(LOG_LEVEL_INFO, pp);
    }

    if ((a.transaction.background) && (a.transaction.audit))
    {
        Log(LOG_LEVEL_ERR, "Auditing cannot be performed on backgrounded promises (this might change).");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if (((a.havecopy) || (a.havelink)) && (a.transformer))
    {
        Log(LOG_LEVEL_ERR, "File object(s) '%s' cannot both be a copy of source and transformed simultaneously",
              pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.haveselect) && (a.select.result == NULL))
    {
        Log(LOG_LEVEL_ERR, "Missing file_result attribute in file_select body");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    if ((a.havedepthsearch) && (a.change.report_diffs))
    {
        Log(LOG_LEVEL_ERR, "Difference reporting is not allowed during a depth_search");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    return true;
}

static bool AttrHasNoAction(Attributes attr)
{
    /* Hopefully this includes all "actions" for a files promise. See struct
     * Attributes for reference. */
    if (!(attr.transformer || attr.haverename || attr.havedelete ||
          attr.havecopy || attr.create || attr.touch || attr.havelink ||
          attr.haveperms || attr.havechange || attr.acl.acl_entries ||
          attr.haveedit || attr.haveeditline || attr.haveeditxml))
    {
        return true;
    }
    else
    {
        return false;
    }
}

static PromiseResult VerifyFilePromise(EvalContext *ctx, char *path, const Promise *pp)
{
    struct stat osb, oslb, dsb;
    CfLock thislock;
    int exists;

    Attributes a = GetFilesAttributes(ctx, pp);

    if (!FileSanityChecks(path, a, pp))
    {
        ClearFilesAttributes(&a);
        return PROMISE_RESULT_NOOP;
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser", path, CF_DATA_TYPE_STRING, "source=promise");

    thislock = AcquireLock(ctx, path, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        ClearFilesAttributes(&a);
        return PROMISE_RESULT_SKIPPED;
    }

    LoadSetuid();

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (lstat(path, &oslb) == -1)       /* Careful if the object is a link */
    {
        if ((a.create) || (a.touch))
        {
            if (!CfCreateFile(ctx, path, pp, a, &result))
            {
                goto exit;
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
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "File '%s' exists as promised", path);
        }
        exists = true;
    }

    if ((a.havedelete) && (!exists))
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "File '%s' does not exist as promised", path);
        goto exit;
    }

    if (!a.havedepthsearch)     /* if the search is trivial, make sure that we are in the parent dir of the leaf */
    {
        char basedir[CF_BUFSIZE];

        Log(LOG_LEVEL_DEBUG, "Direct file reference '%s', no search implied", path);
        snprintf(basedir, sizeof(basedir), "%s", path);

        if (strcmp(ReadLastNode(basedir), ".") == 0)
        {
            // Handle /.  notation for deletion of directories
            ChopLastNode(basedir);
            ChopLastNode(path);
        }

        ChopLastNode(basedir);
        if (safe_chdir(basedir))
        {
            Log(LOG_LEVEL_ERR, "Failed to chdir into '%s'. (chdir: '%s')", basedir, GetErrorStr());
        }
    }

    /* If file or directory exists but it is not selected by body file_select
     * (if we have one) then just exit. But continue if it's a directory and
     * depth_search is on, so that we can file_select into it. */
    if (exists
        && (a.haveselect && !SelectLeaf(ctx, path, &oslb, a.select))
        && !(a.havedepthsearch && S_ISDIR(oslb.st_mode)))
    {
        goto exit;
    }

    if (stat(path, &osb) == -1)
    {
        if ((a.create) || (a.touch))
        {
            if (!CfCreateFile(ctx, path, pp, a, &result))
            {
                goto exit;
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
                Log(LOG_LEVEL_WARNING,
                    "depth_search (recursion) is promised for a base object '%s' that is not a directory",
                      path);
                goto exit;
            }
        }

        exists = true;
    }

    if (a.link.link_children)
    {
        if (stat(a.link.source, &dsb) != -1)
        {
            if (!S_ISDIR(dsb.st_mode))
            {
                Log(LOG_LEVEL_ERR, "Cannot promise to link the children of '%s' as it is not a directory!",
                      a.link.source);
                goto exit;
            }
        }
    }

/* Phase 1 - */

    if (exists && ((a.havedelete) || (a.haverename) || (a.haveperms) || (a.havechange) || (a.transformer)))
    {
        lstat(path, &oslb);     /* if doesn't exist have to stat again anyway */

        DepthSearch(ctx, path, &oslb, 0, a, pp, oslb.st_dev, &result);

        /* normally searches do not include the base directory */

        if (a.recursion.include_basedir)
        {
            int save_search = a.havedepthsearch;

            /* Handle this node specially */

            a.havedepthsearch = false;
            DepthSearch(ctx, path, &oslb, 0, a, pp, oslb.st_dev, &result);
            a.havedepthsearch = save_search;
        }
        else
        {
            /* unless child nodes were repaired, set a promise kept class */
            if (!IsDefinedClass(ctx, "repaired"))
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Basedir '%s' not promising anything", path);
            }
        }
    }

/* Phase 2a - copying is potentially threadable if no followup actions */

    if (a.havecopy)
    {
        result = PromiseResultUpdate(result, ScheduleCopyOperation(ctx, path, a, pp));
    }

/* Phase 2b link after copy in case need file first */

    if ((a.havelink) && (a.link.link_children))
    {
        result = PromiseResultUpdate(result, ScheduleLinkChildrenOperation(ctx, path, a.link.source, 1, a, pp));
    }
    else if (a.havelink)
    {
        result = PromiseResultUpdate(result, ScheduleLinkOperation(ctx, path, a.link.source, a, pp));
    }

/* Phase 3 - content editing */

    if (a.haveedit)
    {
        if (exists)
        {
            result = PromiseResultUpdate(result, ScheduleEditOperation(ctx, path, a, pp));
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Promised to edit '%s', but file does not exist", path);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
    }

// Once more in case a file has been created as a result of editing or copying

    exists = (stat(path, &osb) != -1);

    if (exists && (S_ISREG(osb.st_mode))
        && (!a.haveselect || SelectLeaf(ctx, path, &osb, a.select)))
    {
        VerifyFileLeaf(ctx, path, &osb, a, pp, &result);
    }

    if (!exists && a.havechange)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Promised to monitor '%s' for changes, but file does not exist", path);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

exit:

    if (AttrHasNoAction(a))
    {
        Log(LOG_LEVEL_INFO,
            "No action was requested for file '%s'. Maybe a typo in the policy?", path);
    }

    SaveSetuid();
    YieldCurrentLock(thislock);
    ClearFilesAttributes(&a);

    return result;
}

/*****************************************************************************/

static PromiseResult RenderTemplateCFEngine(EvalContext *ctx, const Promise *pp,
                                            const Rlist *bundle_args, Attributes a,
                                            EditContext *edcontext)
{
    PromiseResult result = PROMISE_RESULT_NOOP;

    Policy *tmp_policy = PolicyNew();
    Bundle *bp = NULL;
    if ((bp = MakeTemporaryBundleFromTemplate(ctx, tmp_policy, a, pp, &result)))
    {
        BannerSubBundle(bp, bundle_args);
        a.haveeditline = true;

        EvalContextStackPushBundleFrame(ctx, bp, bundle_args, a.edits.inherit);
        BundleResolve(ctx, bp);

        ScheduleEditLineOperations(ctx, bp, a, pp, edcontext);

        EvalContextStackPopFrame(ctx);

        if (edcontext->num_edits == 0)
        {
            edcontext->num_edits++;
        }
    }

    PolicyDestroy(tmp_policy);

    return result;
}

static bool SaveBufferCallback(const char *dest_filename, void *param, NewLineMode new_line_mode)
{
    FILE *fp = safe_fopen(dest_filename, (new_line_mode == NewLineMode_Native) ? "wt" : "w");
    if (!fp)
    {
        Log(LOG_LEVEL_ERR, "Unable to open destination file '%s' for writing. (fopen: %s)",
            dest_filename, GetErrorStr());
        return false;
    }

    Buffer *output_buffer = param;

    size_t bytes_written = fwrite(BufferData(output_buffer), sizeof(char), BufferSize(output_buffer), fp);
    if (bytes_written != BufferSize(output_buffer))
    {
        Log(LOG_LEVEL_ERR, "Error writing to output file '%s' when writing. %zd bytes written but expected %d. (fclose: %s)",
            dest_filename, bytes_written, BufferSize(output_buffer), GetErrorStr());
        fclose(fp);
        return false;
    }

    if (fclose(fp) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to close file '%s' after writing. (fclose: %s)",
            dest_filename, GetErrorStr());
        return false;
    }

    return true;
}

static PromiseResult RenderTemplateMustache(EvalContext *ctx, const Promise *pp, Attributes a,
                                            EditContext *edcontext)
{
    PromiseResult result = PROMISE_RESULT_NOOP;

    if (!FileCanOpen(a.edit_template, "r"))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Template file '%s' could not be opened for reading", a.edit_template);
        return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    unsigned char existing_output_digest[EVP_MAX_MD_SIZE + 1] = { 0 };
    if (access(pp->promiser, R_OK) == 0)
    {
        HashFile(pp->promiser, existing_output_digest, CF_DEFAULT_DIGEST);
    }

    int template_fd = safe_open(a.edit_template, O_RDONLY | O_TEXT);
    Writer *template_writer = NULL;
    if (template_fd >= 0)
    {
        template_writer = FileReadFromFd(template_fd, SIZE_MAX, NULL);
        close(template_fd);
    }
    if (!template_writer)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Could not read template file '%s'", a.edit_template);
        return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    JsonElement *default_template_data = NULL;
    if (!a.template_data)
    {
        a.template_data = default_template_data = DefaultTemplateData(ctx);
    }

    Buffer *output_buffer = BufferNew();
    if (MustacheRender(output_buffer, StringWriterData(template_writer), a.template_data))
    {
        unsigned char rendered_output_digest[EVP_MAX_MD_SIZE + 1] = { 0 };
        HashString(BufferData(output_buffer), BufferSize(output_buffer), rendered_output_digest, CF_DEFAULT_DIGEST);
        if (!HashesMatch(existing_output_digest, rendered_output_digest, CF_DEFAULT_DIGEST))
        {
            if (SaveAsFile(SaveBufferCallback, output_buffer, edcontext->filename, a, edcontext->new_line_mode))
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Updated rendering of '%s' from template mustache template '%s'",
                     pp->promiser, a.edit_template);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Updated rendering of '%s' from template mustache template '%s'",
                     pp->promiser, a.edit_template);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            }
        }

        JsonDestroy(default_template_data);
        WriterClose(template_writer);
        BufferDestroy(output_buffer);

        return result;
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Error rendering mustache template '%s'", a.edit_template);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        JsonDestroy(default_template_data);
        WriterClose(template_writer);
        BufferDestroy(output_buffer);
        return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }
}

PromiseResult ScheduleEditOperation(EvalContext *ctx, char *filename, Attributes a, const Promise *pp)
{
    void *vp;
    FnCall *fp;
    Rlist *args = NULL;
    char edit_bundle_name[CF_BUFSIZE], lockname[CF_BUFSIZE];
    CfLock thislock;

    snprintf(lockname, CF_BUFSIZE - 1, "fileedit-%s", filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);

    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    EditContext *edcontext = NewEditContext(filename, a);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (edcontext == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "File '%s' was marked for editing but could not be opened", filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        goto exit;
    }

    const Policy *policy = PolicyFromPromise(pp);

    if (a.haveeditline)
    {
        if ((vp = PromiseGetConstraintAsRval(pp, "edit_line", RVAL_TYPE_FNCALL)))
        {
            fp = (FnCall *) vp;
            strcpy(edit_bundle_name, fp->name);
            args = fp->args;
        }
        else if ((vp = PromiseGetConstraintAsRval(pp, "edit_line", RVAL_TYPE_SCALAR)))
        {
            strcpy(edit_bundle_name, (char *) vp);
            args = NULL;
        }             
        else
        {
            goto exit;
        }

        Log(LOG_LEVEL_VERBOSE, "Handling file edits in edit_line bundle '%s'", edit_bundle_name);

        const Bundle *bp = EvalContextResolveBundleExpression(ctx, policy, edit_bundle_name, "edit_line");
        if (bp)
        {
            BannerSubBundle(bp, args);

            EvalContextStackPushBundleFrame(ctx, bp, args, a.edits.inherit);

            BundleResolve(ctx, bp);

            ScheduleEditLineOperations(ctx, bp, a, pp, edcontext);

            EvalContextStackPopFrame(ctx);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Did not find bundle '%s' for edit operation", edit_bundle_name);
        }
    }


    if (a.haveeditxml)
    {
        if ((vp = PromiseGetConstraintAsRval(pp, "edit_xml", RVAL_TYPE_FNCALL)))
        {
            fp = (FnCall *) vp;
            strcpy(edit_bundle_name, fp->name);
            args = fp->args;
        }
        else if ((vp = PromiseGetConstraintAsRval(pp, "edit_xml", RVAL_TYPE_SCALAR)))
        {
            strcpy(edit_bundle_name, (char *) vp);
            args = NULL;
        }
        else
        {
            goto exit;
        }
        
        Log(LOG_LEVEL_VERBOSE, "Handling file edits in edit_xml bundle '%s'", edit_bundle_name);

        const Bundle *bp = EvalContextResolveBundleExpression(ctx, policy, edit_bundle_name, "edit_xml");
        if (bp)
        {
            BannerSubBundle(bp, args);

            EvalContextStackPushBundleFrame(ctx, bp, args, a.edits.inherit);
            BundleResolve(ctx, bp);

            ScheduleEditXmlOperations(ctx, bp, a, pp, edcontext);

            EvalContextStackPopFrame(ctx);
        }
    }

    
    if (a.edit_template)
    {
        if (!a.template_method || strcmp("cfengine", a.template_method) == 0)
        {
            PromiseResult render_result = RenderTemplateCFEngine(ctx, pp, args, a, edcontext);
            result = PromiseResultUpdate(result, render_result);
        }
        else if (strcmp("mustache", a.template_method) == 0)
        {
            PromiseResult render_result = RenderTemplateMustache(ctx, pp, a, edcontext);
            result = PromiseResultUpdate(result, render_result);
        }
    }

exit:
    FinishEditContext(ctx, edcontext, a, pp, &result);
    YieldCurrentLock(thislock);
    return result;
}

/*****************************************************************************/

PromiseResult FindAndVerifyFilesPromises(EvalContext *ctx, const Promise *pp)
{
    PromiseBanner(pp);
    return FindFilePromiserObjects(ctx, pp);
}

/*****************************************************************************/

static PromiseResult FindFilePromiserObjects(EvalContext *ctx, const Promise *pp)
{
    char *val = PromiseGetConstraintAsRval(pp, "pathtype", RVAL_TYPE_SCALAR);
    int literal = (PromiseGetConstraintAsBoolean(ctx, "copy_from", pp)) || ((val != NULL) && (strcmp(val, "literal") == 0));

/* Check if we are searching over a regular expression */

    PromiseResult result = PROMISE_RESULT_SKIPPED;
    if (literal)
    {
        // Prime the promiser temporarily, may override later
        result = PromiseResultUpdate(result, VerifyFilePromise(ctx, pp->promiser, pp));
    }
    else                        // Default is to expand regex paths
    {
        result = PromiseResultUpdate(result, LocateFilePromiserGroup(ctx, pp->promiser, pp, VerifyFilePromise));
    }

    return result;
}

static void LoadSetuid(void)
{
    char filename[CF_BUFSIZE];
    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", GetLogDir(), VSYSNAME.nodename);
    MapName(filename);

    VSETUIDLIST = RawLoadItemList(filename);
}

/*********************************************************************/

static void SaveSetuid(void)
{
    char filename[CF_BUFSIZE];
    snprintf(filename, CF_BUFSIZE, "%s/cfagent.%s.log", GetLogDir(), VSYSNAME.nodename);
    MapName(filename);

    PurgeItemList(&VSETUIDLIST, "SETUID/SETGID");

    Item *current = RawLoadItemList(filename);
    if (!ListsCompare(VSETUIDLIST, current))
    {
        RawSaveItemList(VSETUIDLIST, filename, NewLineMode_Unix);
    }

    DeleteItemList(VSETUIDLIST);
    VSETUIDLIST = NULL;
}
