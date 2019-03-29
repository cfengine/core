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

#include <files_edit.h>

#include <actuator.h>
#include <eval_context.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_operators.h>
#include <files_lib.h>
#include <files_editxml.h>
#include <item_lib.h>
#include <policy.h>

/*****************************************************************************/

EditContext *NewEditContext(char *filename, Attributes a)
{
    EditContext *ec;

    if (!IsAbsoluteFileName(filename))
    {
        Log(LOG_LEVEL_ERR, "Relative file name '%s' was marked for editing but has no invariant meaning", filename);
        return NULL;
    }

    ec = xcalloc(1, sizeof(EditContext));

    ec->filename = filename;
    ec->new_line_mode = FileNewLineMode(filename);

    if (a.haveeditline)
    {
        if (!LoadFileAsItemList(&(ec->file_start), filename, a.edits))
        {
            free(ec);
            return NULL;
        }
    }

    if (a.haveeditxml)
    {
#ifdef HAVE_LIBXML2
        if (!LoadFileAsXmlDoc(&(ec->xmldoc), filename, a.edits))
        {
            free(ec);
            return NULL;
        }
#else
        Log(LOG_LEVEL_ERR, "Cannot edit XML files without LIBXML2");
        free(ec);
        return NULL;
#endif
    }

    if (a.edits.empty_before_use)
    {
        Log(LOG_LEVEL_VERBOSE, "Build file model from a blank slate (emptying)");
        DeleteItemList(ec->file_start);
        ec->file_start = NULL;
    }

    return ec;
}

/*****************************************************************************/

void FinishEditContext(EvalContext *ctx, EditContext *ec, Attributes a, const Promise *pp,
                       PromiseResult *result)
{
    if (*result == PROMISE_RESULT_NOOP || *result == PROMISE_RESULT_CHANGE)
    {
        // A sub promise with CHANGE status does not necessarily mean that the
        // file has really changed.
        *result = PROMISE_RESULT_NOOP;
    }
    else
    {
        // Failure or skipped. Don't update the file.
        goto end;
    }

    if (DONTDO || (a.transaction.action == cfa_warn))
    {
        if (ec &&
            !CompareToFile(ctx, ec->file_start, ec->filename, a, pp, result) &&
            ec->num_edits > 0)
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
                 "Should edit file '%s' but only a warning promised",
                 ec->filename);
            *result = PROMISE_RESULT_WARN;
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                 "No edit changes to file '%s' need saving", ec->filename);
            *result = PROMISE_RESULT_NOOP;
        }
    }
    else if (ec && (ec->num_rewrites > 0))
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a,
             "Edit file '%s'", ec->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    }
    else if (ec && (ec->num_edits > 0))
    {
        if (a.haveeditline || a.edit_template || a.edit_template_string)
        {
            if (CompareToFile(ctx, ec->file_start, ec->filename, a, pp, result))
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                     "No edit changes to file '%s' need saving", ec->filename);
            }
            else if (SaveItemListAsFile(ec->file_start, ec->filename, a, ec->new_line_mode))
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a,
                     "Edit file '%s'", ec->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                     "Unable to save file '%s' after editing", ec->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            }
        }

        if (a.haveeditxml)
        {
#ifdef HAVE_LIBXML2
            if (XmlCompareToFile(ec->xmldoc, ec->filename, a.edits))
            {
                if (ec)
                {
                    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                         "No edit changes to xml file '%s' need saving", ec->filename);
                }
            }
            else if (SaveXmlDocAsFile(ec->xmldoc, ec->filename, a, ec->new_line_mode))
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a,
                     "Edited xml file '%s'", ec->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                     "Failed to edit XML file '%s'", ec->filename);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            }
            xmlFreeDoc(ec->xmldoc);
#else
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "Cannot edit XML files without LIBXML2");
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
#endif
        }
    }
    else if (ec)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
             "No edit changes to file '%s' need saving", ec->filename);
    }

end:
    if (ec != NULL)
    {
        DeleteItemList(ec->file_start);
        free(ec);
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

#ifdef HAVE_LIBXML2
/***************************************************************************/

int LoadFileAsXmlDoc(xmlDocPtr *doc, const char *file, EditDefaults edits)
{
    struct stat statbuf;

    if (stat(file, &statbuf) == -1)
    {
        Log(LOG_LEVEL_ERR, "The proposed file '%s' could not be loaded. (stat: %s)", file, GetErrorStr());
        return false;
    }

    if (edits.maxfilesize != 0 && statbuf.st_size > edits.maxfilesize)
    {
        Log(LOG_LEVEL_INFO, "File '%s' is bigger than the edit limit. max_file_size = '%jd' > '%d' bytes", file,
              (intmax_t) statbuf.st_size, edits.maxfilesize);
        return false;
    }

    if (!S_ISREG(statbuf.st_mode))
    {
        Log(LOG_LEVEL_INFO, "'%s' is not a plain file", file);
        return false;
    }

    if (statbuf.st_size == 0)
    {
        if ((*doc = xmlNewDoc(BAD_CAST "1.0")) == NULL)
        {
            Log(LOG_LEVEL_INFO, "Document '%s' not parsed successfully. (xmlNewDoc: %s)", file, GetErrorStr());
            return false;
        }
    }
    else if ((*doc = xmlParseFile(file)) == NULL)
    {
        Log(LOG_LEVEL_INFO, "Document '%s' not parsed successfully. (xmlParseFile: %s)", file, GetErrorStr());
        return false;
    }

    return true;
}

/*********************************************************************/

static bool SaveXmlCallback(const char *dest_filename, void *param,
                            ARG_UNUSED NewLineMode new_line_mode)
{
    xmlDocPtr doc = param;

    //saving xml to file
    if (xmlSaveFile(dest_filename, doc) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to write xml document to file '%s' after editing. (xmlSaveFile: %s)", dest_filename, GetErrorStr());
        return false;
    }

    return true;
}

/*********************************************************************/

bool SaveXmlDocAsFile(xmlDocPtr doc, const char *file, Attributes a, NewLineMode new_line_mode)
{
    return SaveAsFile(&SaveXmlCallback, doc, file, a, new_line_mode);
}
#endif /* HAVE_LIBXML2 */
