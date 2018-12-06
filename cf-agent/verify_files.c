/*
   Copyright 2018 Northern.tech AS

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

    if ((a.haveedit) && (a.file_type) && (!strncmp(a.file_type, "fifo", 5)))
    {
        Log(LOG_LEVEL_ERR, "Editing is not allowed on fifos");
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

/*
 * Expands source in-place.
 */
static char *ExpandThisPromiserScalar(EvalContext *ctx, const char *ns, const char *scope, const char *source)
{
    if (!source)
    {
        return NULL;
    }
    Buffer *expanded = BufferNew();
    ExpandScalar(ctx, ns, scope, source, expanded);
    char *result = strdup(BufferData(expanded));
    BufferDestroy(expanded);
    return result;
}

/*
 * Overwrite non-specific attributes with expanded this.promiser.
 */
Attributes GetExpandedAttributes(EvalContext *ctx, const Promise *pp, const Attributes *attr)
{
    const char *namespace = PromiseGetBundle(pp)->ns;
    const char *scope = PromiseGetBundle(pp)->name;

    Attributes a = *attr; // shallow copy

    a.classes.change = ExpandList(ctx, namespace, scope, attr->classes.change, true);
    a.classes.failure = ExpandList(ctx, namespace, scope, attr->classes.failure, true);
    a.classes.denied = ExpandList(ctx, namespace, scope, attr->classes.denied, true);
    a.classes.timeout = ExpandList(ctx, namespace, scope, attr->classes.timeout, true);
    a.classes.kept = ExpandList(ctx, namespace, scope, attr->classes.kept, true);

    a.classes.del_change = ExpandList(ctx, namespace, scope, attr->classes.del_change, true);
    a.classes.del_kept = ExpandList(ctx, namespace, scope, attr->classes.del_kept, true);
    a.classes.del_notkept = ExpandList(ctx, namespace, scope, attr->classes.del_notkept, true);

    a.transaction.log_string = ExpandThisPromiserScalar(ctx, namespace, scope, attr->transaction.log_string);
    a.transaction.log_kept = ExpandThisPromiserScalar(ctx, namespace, scope, attr->transaction.log_kept);
    a.transaction.log_repaired = ExpandThisPromiserScalar(ctx, namespace, scope, attr->transaction.log_repaired);
    a.transaction.log_failed = ExpandThisPromiserScalar(ctx, namespace, scope, attr->transaction.log_failed);
    a.transaction.measure_id = ExpandThisPromiserScalar(ctx, namespace, scope, attr->transaction.measure_id);

    // a.transformer = ExpandThisPromiserScalar(ctx, namespace, scope, attr->transformer);
    a.edit_template = ExpandThisPromiserScalar(ctx, namespace, scope, attr->edit_template);
    a.edit_template_string = ExpandThisPromiserScalar(ctx, namespace, scope, attr->edit_template_string);

    return a;
}

void ClearExpandedAttributes(Attributes *a)
{
    DESTROY_AND_NULL(RlistDestroy, a->classes.change);
    DESTROY_AND_NULL(RlistDestroy, a->classes.failure);
    DESTROY_AND_NULL(RlistDestroy, a->classes.denied);
    DESTROY_AND_NULL(RlistDestroy, a->classes.timeout);
    DESTROY_AND_NULL(RlistDestroy, a->classes.kept);
    DESTROY_AND_NULL(RlistDestroy, a->classes.del_change);
    DESTROY_AND_NULL(RlistDestroy, a->classes.del_kept);
    DESTROY_AND_NULL(RlistDestroy, a->classes.del_notkept);

    FREE_AND_NULL(a->transaction.log_string);
    FREE_AND_NULL(a->transaction.log_kept);
    FREE_AND_NULL(a->transaction.log_repaired);
    FREE_AND_NULL(a->transaction.log_failed);
    FREE_AND_NULL(a->transaction.measure_id);

    FREE_AND_NULL(a->edit_template);
    FREE_AND_NULL(a->edit_template_string);

    ClearFilesAttributes(a);
}

static PromiseResult VerifyFilePromise(EvalContext *ctx, char *path, const Promise *pp)
{
    struct stat osb, oslb, dsb;
    CfLock thislock;
    int exists;
    bool link = false;

    Attributes attr = GetFilesAttributes(ctx, pp);

    if (!FileSanityChecks(path, attr, pp))
    {
        ClearFilesAttributes(&attr);
        return PROMISE_RESULT_NOOP;
    }

    thislock = AcquireLock(ctx, path, VUQNAME, CFSTARTTIME, attr.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        ClearFilesAttributes(&attr);
        return PROMISE_RESULT_SKIPPED;
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser", path, CF_DATA_TYPE_STRING, "source=promise");
    Attributes a = GetExpandedAttributes(ctx, pp, &attr);

    PromiseResult result = PROMISE_RESULT_NOOP;

    /* if template_data was specified, it must have been resolved to a data
     * container by now */
    /* check this early to prevent creation of the file below in case of failure */
    const Constraint *template_data_constraint = PromiseGetConstraint(pp, "template_data");
    if (template_data_constraint != NULL &&
        template_data_constraint->rval.type != RVAL_TYPE_CONTAINER)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "No template data for the promise '%s'", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        goto exit;
    }

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
        link = true;
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
            char msg[CF_BUFSIZE];
            snprintf(msg, sizeof(msg), "Failed to chdir into '%s'. (chdir: '%s')",
                     basedir, GetErrorStr());
            if (errno == ENOLINK)
            {
                Log(LOG_LEVEL_ERR, "%s. There may be a symlink in the path that has a different "
                    "owner from the owner of its target (security risk).", msg);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "%s", msg);
            }
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
                Log(LOG_LEVEL_DEBUG,
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

    if ((exists
         && (a.haverename || a.haveperms || a.havechange || a.transformer ||
             a.acl.acl_entries != NULL)
        ) ||
        ((exists || link) && a.havedelete))
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
            if (result == PROMISE_RESULT_NOOP)
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

    YieldCurrentLock(thislock);

    ClearExpandedAttributes(&a);
    EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser");

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
        Log(LOG_LEVEL_ERR,
            "Error writing to output file '%s' when writing. %zu bytes written but expected %u. (fclose: %s)",
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
                                            EditContext *edcontext, const char *template)
{
    PromiseResult result = PROMISE_RESULT_NOOP;

    JsonElement *destroy_this = NULL;

    if (a.template_data == NULL)
    {
        a.template_data = DefaultTemplateData(ctx, NULL);
        destroy_this = a.template_data;
    }

    unsigned char existing_output_digest[EVP_MAX_MD_SIZE + 1] = { 0 };
    if (access(pp->promiser, R_OK) == 0)
    {
        HashFile(pp->promiser, existing_output_digest, CF_DEFAULT_DIGEST,
                 edcontext->new_line_mode == NewLineMode_Native);
    }

    Buffer *output_buffer = BufferNew();

    char *message;
    if ( strcmp("inline_mustache", a.template_method) == 0)
    {
        message = xstrdup("inline");
    }
    else
    {
        message = xstrdup(a.edit_template);
    }

    if (MustacheRender(output_buffer, template, a.template_data))
    {
        unsigned char rendered_output_digest[EVP_MAX_MD_SIZE + 1] = { 0 };
        HashString(BufferData(output_buffer), BufferSize(output_buffer), rendered_output_digest, CF_DEFAULT_DIGEST);
        if (!HashesMatch(existing_output_digest, rendered_output_digest, CF_DEFAULT_DIGEST))
        {
            if (a.transaction.action == cfa_warn || DONTDO)
            {
                cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
                     "Need to update rendering of '%s' from mustache template '%s' but policy is dry-run",
                     pp->promiser, message);
                result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            }
            else
            {
                if (SaveAsFile(SaveBufferCallback, output_buffer, edcontext->filename, a, edcontext->new_line_mode))
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Updated rendering of '%s' from mustache template '%s'",
                         pp->promiser, message);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Failed to update rendering of '%s' from mustache template '%s'",
                         pp->promiser, message);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                }
            }
        }

        BufferDestroy(output_buffer);
        free(message);
        JsonDestroy(destroy_this);
        return result;
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Error rendering mustache template '%s'", a.edit_template);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        BufferDestroy(output_buffer);
        JsonDestroy(destroy_this);
        return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }
}

static PromiseResult RenderTemplateMustacheFromFile(EvalContext *ctx, const Promise *pp, Attributes a,
                                            EditContext *edcontext)
{
    PromiseResult result = PROMISE_RESULT_NOOP;

    if (!FileCanOpen(a.edit_template, "r"))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Template file '%s' could not be opened for reading", a.edit_template);
        return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }


    int template_fd = safe_open(a.edit_template, O_RDONLY | O_TEXT);
    Writer *template_writer = NULL;
    if (template_fd >= 0)
    {
        template_writer = FileReadFromFd(template_fd, SIZE_MAX, NULL);
        close(template_fd);
    }
    if (template_writer == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Could not read template file '%s'", a.edit_template);
        return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    result =  RenderTemplateMustache(ctx, pp, a, edcontext, StringWriterData(template_writer));

    WriterClose(template_writer);

    return result;
}

static PromiseResult RenderTemplateMustacheFromString(EvalContext *ctx, const Promise *pp, Attributes a,
                                            EditContext *edcontext)
{

    if ( a.edit_template_string == NULL  )
    {
        PromiseResult result = PROMISE_RESULT_NOOP;

        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "'edit_template_string' not set for promiser: '%s'", pp->promiser);
        return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    return  RenderTemplateMustache(ctx, pp, a, edcontext, a.edit_template_string);
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
            EvalContextStackPushBundleFrame(ctx, bp, args, a.edits.inherit);
            BundleResolve(ctx, bp);

            ScheduleEditXmlOperations(ctx, bp, a, pp, edcontext);

            EvalContextStackPopFrame(ctx);
        }
    }

    if (strcmp("cfengine", a.template_method) == 0)
    {
        if (a.edit_template)
        {
            Log(LOG_LEVEL_VERBOSE, "Rendering '%s' using template '%s' with method '%s'",
                filename, a.edit_template, a.template_method);

            PromiseResult render_result = RenderTemplateCFEngine(ctx, pp, args, a, edcontext);
            result = PromiseResultUpdate(result, render_result);
        }
    }
    else if (strcmp("mustache", a.template_method) == 0)
    {
        if (a.edit_template)
        {
            Log(LOG_LEVEL_VERBOSE, "Rendering '%s' using template '%s' with method '%s'",
                filename, a.edit_template, a.template_method);

            PromiseResult render_result = RenderTemplateMustacheFromFile(ctx, pp, a, edcontext);
            result = PromiseResultUpdate(result, render_result);
        }
    }
    else if (strcmp("inline_mustache", a.template_method) == 0)
    {
        if (a.edit_template_string)
        {
            Log(LOG_LEVEL_VERBOSE, "Rendering '%s' with method '%s'",
                filename, a.template_method);

            PromiseResult render_result = RenderTemplateMustacheFromString(ctx, pp, a, edcontext);
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
    PromiseBanner(ctx, pp);
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
