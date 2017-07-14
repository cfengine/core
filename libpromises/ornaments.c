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

#include <ornaments.h>

#include <string_lib.h>
#include <rlist.h>
#include <logging.h>
#include <fncall.h>
#include <promises.h>                                          /* PromiseID */


/**
 * @brief Like StringAppend(), but replace characters '*' and '#' with their visible counterparts.
 * @param buffer Buffer to be used.
 * @param src    Constant string to append
 * @param n      Total size of dst buffer. The string will be truncated if this is exceeded.
 */
static bool StringAppendPromise(char *dst, const char *src, size_t n)
{
    int i, j;
    n--;
    for (i = 0; i < n && dst[i]; i++)
    {
    }
    for (j = 0; i < n && src[j]; i++, j++)
    {
        const char ch = src[j];
        switch (ch)
        {
        case CF_MANGLED_NS:
            dst[i] = ':';
            break;

        case CF_MANGLED_SCOPE:
            dst[i] = '.';
            break;

        default:
            dst[i] = ch;
            break;
        }
    }
    dst[i] = '\0';
    return (i < n || !src[j]);
}

/**
 * @brief Like @c BufferAppendPromiseStr, but if @c str contains newlines
 *   and is longer than 2*N+3, then only copy an abbreviated version
 *   consisting of the first and last N characters, separated by @c `...`
 * @param buffer Buffer to be used.
 * @param str    Constant string to append
 * @param n Total size of dst buffer. The string will be truncated if this is exceeded.
 * @param max_fragment Max. length of initial/final segment of @c str to keep
 * @note 2*max_fragment+3 is the maximum length of the appended string (excl. terminating NULL)
 *
 */
static bool StringAppendAbbreviatedPromise(char *dst, const char *src, size_t n,
                                           const size_t max_fragment)
{
    /* check if `src` contains a new line (may happen for "insert_lines") */
    const char *const nl = strchr(src, '\n');
    if (nl == NULL)
    {
        return StringAppendPromise(dst, src, n);
    }
    else
    {
        /* `src` contains a newline: abbreviate it by taking the first and last few characters */
        static const char sep[] = "...";
        char abbr[sizeof(sep) + 2 * max_fragment];
        const int head = (nl > src + max_fragment) ? max_fragment : (nl - src);
        const char * last_line = strrchr(src, '\n') + 1;
        assert(last_line); /* not max_fragmentULL, we know we have at least one '\n' */
        const int tail = strlen(last_line);
        if (tail > max_fragment)
        {
            last_line += tail - max_fragment;
        }
        memcpy(abbr, src, head);
        strcpy(abbr + head, sep);
        strcat(abbr, last_line);
        return StringAppendPromise(dst, abbr, n);
    }
}


/*********************************************************************/


void SpecialTypeBanner(TypeSequence type, int pass)
{
    if (type == TYPE_SEQUENCE_CONTEXTS)
    {
        Log(LOG_LEVEL_VERBOSE, "C: .........................................................");
        Log(LOG_LEVEL_VERBOSE, "C: BEGIN classes / conditions (pass %d)", pass);
    }
    if (type == TYPE_SEQUENCE_VARS)
    {
        Log(LOG_LEVEL_VERBOSE, "V: .........................................................");
        Log(LOG_LEVEL_VERBOSE, "V: BEGIN variables (pass %d)", pass);
    }
}

void PromiseBanner(EvalContext *ctx, const Promise *pp)
{
    char handle[CF_MAXVARSIZE];
    const char *sp;

    if ((sp = PromiseGetHandle(pp)) || (sp = PromiseID(pp)))
    {
        strlcpy(handle, sp, CF_MAXVARSIZE);
    }
    else
    {
        strcpy(handle, "");
    }

    Log(LOG_LEVEL_VERBOSE, "P: .........................................................");

    if (strlen(handle) > 0)
    {
        Log(LOG_LEVEL_VERBOSE, "P: BEGIN promise '%s' of type \"%s\" (pass %d)", handle, pp->parent_promise_type->name, EvalContextGetPass(ctx));
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "P: BEGIN un-named promise of type \"%s\" (pass %d)", pp->parent_promise_type->name, EvalContextGetPass(ctx));
    }

    const size_t n = 2*CF_MAXFRAGMENT + 3;
    char pretty_promise_name[n+1];
    pretty_promise_name[0] = '\0';
    StringAppendAbbreviatedPromise(pretty_promise_name, pp->promiser, n, CF_MAXFRAGMENT);
    Log(LOG_LEVEL_VERBOSE, "P:    Promiser/affected object: '%s'", pretty_promise_name);

    Rlist *params = NULL;
    char *varclass;
    FnCall *fp;

    if ((params = EvalContextGetBundleArgs(ctx)))
    {
        Writer *w = StringWriter();
        RlistWrite(w, params);
        Log(LOG_LEVEL_VERBOSE, "P:    From parameterized bundle: %s(%s)", PromiseGetBundle(pp)->name, StringWriterData(w));
        WriterClose(w);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "P:    Part of bundle: %s", PromiseGetBundle(pp)->name);
    }

    Log(LOG_LEVEL_VERBOSE, "P:    Base context class: %s", pp->classes);

    if ((varclass = PromiseGetConstraintAsRval(pp, "if", RVAL_TYPE_SCALAR)) || (varclass = PromiseGetConstraintAsRval(pp, "ifvarclass", RVAL_TYPE_SCALAR)))
    {
        Log(LOG_LEVEL_VERBOSE, "P:    \"if\" class condition: %s", varclass);
    }
    else if ((fp = (FnCall *)PromiseGetConstraintAsRval(pp, "if", RVAL_TYPE_FNCALL)) || (fp = (FnCall *)PromiseGetConstraintAsRval(pp, "ifvarclass", RVAL_TYPE_FNCALL)))
    {
        Writer *w = StringWriter();
        FnCallWrite(w, fp);
        Log(LOG_LEVEL_VERBOSE, "P:    \"if\" class condition: %s", StringWriterData(w));
    }
    else if ((varclass = PromiseGetConstraintAsRval(pp, "unless", RVAL_TYPE_SCALAR)))
    {
        Log(LOG_LEVEL_VERBOSE, "P:    \"unless\" class condition: %s", varclass);
    }
    else if ((fp = (FnCall *)PromiseGetConstraintAsRval(pp, "unless", RVAL_TYPE_FNCALL)))
    {
        Writer *w = StringWriter();
        FnCallWrite(w, fp);
        Log(LOG_LEVEL_VERBOSE, "P:    \"unless\" class condition: %s", StringWriterData(w));
    }

    Log(LOG_LEVEL_VERBOSE, "P:    Stack path: %s", EvalContextStackToString(ctx));

    if (pp->comment)
    {
        Log(LOG_LEVEL_VERBOSE, "P:\n");
        Log(LOG_LEVEL_VERBOSE, "P:    Comment:  %s", pp->comment);
    }
}

void Legend()
{
    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");
    Log(LOG_LEVEL_VERBOSE, "PREFIX LEGEND:");
    Log(LOG_LEVEL_VERBOSE, " V: variable or parameter new definition in scope");
    Log(LOG_LEVEL_VERBOSE, " C: class/context new definition ");
    Log(LOG_LEVEL_VERBOSE, " B: bundle start/end execution marker");
    Log(LOG_LEVEL_VERBOSE, " P: promise execution output ");
    Log(LOG_LEVEL_VERBOSE, " A: accounting output ");
    Log(LOG_LEVEL_VERBOSE, " T: time measurement for stated object (promise or bundle)");
    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");
}

void Banner(const char *s)
{
    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");
    Log(LOG_LEVEL_VERBOSE, " %s ", s);
    Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");

}

void BundleBanner(const Bundle *bp, const Rlist *params)
{
    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");

    if (params)
    {
        Writer *w = StringWriter();
        RlistWrite(w, params);
        Log(LOG_LEVEL_VERBOSE, "B: BEGIN bundle %s(%s)", bp->name, StringWriterData(w));
        WriterClose(w);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "B: BEGIN bundle %s", bp->name);
    }

    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
}

void EndBundleBanner(const Bundle *bp)
{
    if (bp == NULL)
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
    Log(LOG_LEVEL_VERBOSE, "B: END bundle %s", bp->name);
    Log(LOG_LEVEL_VERBOSE, "B: *****************************************************************");
}
