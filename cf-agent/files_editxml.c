/*
   Copyright 2017 Northern.tech AS

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

#include <cf3.defs.h>

#include <pcre_include.h>
#include <actuator.h>
#include <eval_context.h>
#include <promises.h>
#include <files_names.h>
#include <files_edit.h>
#include <vars.h>
#include <item_lib.h>
#include <sort.h>
#include <conversion.h>
#include <expand.h>
#include <scope.h>
#include <files_interfaces.h>
#include <attributes.h>
#include <locks.h>
#include <policy.h>
#include <ornaments.h>
#include <verify_classes.h>

enum editxmltypesequence
{
    elx_vars,
    elx_classes,
    elx_delete,
    elx_insert,
    elx_none
};

static const char *const EDITXMLTYPESEQUENCE[] =
{
    "vars",
    "classes",
    "build_xpath",
    "delete_tree",
    "insert_tree",
    "delete_attribute",
    "set_attribute",
    "delete_text",
    "set_text",
    "insert_text",
    "reports",
    NULL
};

static PromiseResult KeepEditXmlPromise(EvalContext *ctx, const Promise *pp, void *param);
#ifdef HAVE_LIBXML2
static bool VerifyXPathBuild(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static PromiseResult VerifyTreeDeletions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyTreeInsertions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyAttributeDeletions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyAttributeSet(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyTextDeletions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyTextSet(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext);
static PromiseResult VerifyTextInsertions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext);
static bool XmlSelectNode(EvalContext *ctx, char *xpath, xmlDocPtr doc, xmlNodePtr *docnode, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool BuildXPathInFile(EvalContext *ctx, char xpath[CF_BUFSIZE], xmlDocPtr doc, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool BuildXPathInNode(EvalContext *ctx, char xpath[CF_BUFSIZE], xmlDocPtr doc, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool DeleteTreeInNode(EvalContext *ctx, char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool InsertTreeInFile(EvalContext *ctx, char *root, xmlDocPtr doc, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool InsertTreeInNode(EvalContext *ctx, char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool DeleteAttributeInNode(EvalContext *ctx, char *attrname, xmlNodePtr docnode, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool SetAttributeInNode(EvalContext *ctx, char *attrname, char *attrvalue, xmlNodePtr docnode, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool DeleteTextInNode(EvalContext *ctx, char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool SetTextInNode(EvalContext *ctx, char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool InsertTextInNode(EvalContext *ctx, char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result);
static bool SanityCheckXPathBuild(EvalContext *ctx, Attributes a, const Promise *pp, PromiseResult *result);
static bool SanityCheckTreeDeletions(Attributes a);
static bool SanityCheckTreeInsertions(Attributes a, EditContext *edcontext);
static bool SanityCheckAttributeDeletions(Attributes a);
static bool SanityCheckAttributeSet(Attributes a);
static bool SanityCheckTextDeletions(Attributes a);
static bool SanityCheckTextSet(Attributes a);
static bool SanityCheckTextInsertions(Attributes a);

static bool XmlDocsEqualMem(xmlDocPtr doc1, xmlDocPtr doc2);
static bool XmlNodesCompare(xmlNodePtr node1, xmlNodePtr node2, Attributes a, const Promise *pp);
static bool XmlNodesCompareAttributes(xmlNodePtr node1, xmlNodePtr node2);
static bool XmlNodesCompareNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, const Promise *pp);
static bool XmlNodesCompareTags(const xmlNodePtr node1, const xmlNodePtr node2);
static bool XmlNodesCompareText(xmlNodePtr node1, xmlNodePtr node2);
static bool XmlNodesSubset(const xmlNodePtr node1, const xmlNodePtr node2, Attributes a, const Promise *pp);
static bool XmlNodesSubsetOfAttributes(const xmlNodePtr node1, const xmlNodePtr node2);
static bool XmlNodesSubsetOfNodes(const xmlNodePtr node1, const xmlNodePtr node2, Attributes a, const Promise *pp);
static bool XmlNodesSubstringOfText(const xmlNodePtr node1, const xmlNodePtr node2);
static xmlAttrPtr XmlVerifyAttributeInNode(const xmlChar *attrname, xmlChar *attrvalue, xmlNodePtr node);
static bool XmlVerifyTextInNodeExact(const xmlChar *text, const xmlNodePtr node);
static bool XmlVerifyTextInNodeSubstring(const xmlChar *text, xmlNodePtr node);
static bool XmlVerifyNodeInNodeExact(const xmlNodePtr node1, const xmlNodePtr node2, Attributes a, const Promise *pp);
static xmlNodePtr XmlVerifyNodeInNodeSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, const Promise *pp);

//xpath build functionality
static xmlNodePtr PredicateExtractNode(char predicate[CF_BUFSIZE]);
static bool PredicateRemoveHead(char xpath[CF_BUFSIZE]);

static xmlNodePtr XPathHeadExtractNode(EvalContext *ctx, char xpath[CF_BUFSIZE], Attributes a, const Promise *pp, PromiseResult *result);
static xmlNodePtr XPathTailExtractNode(EvalContext *ctx, char xpath[CF_BUFSIZE], Attributes a, const Promise *pp, PromiseResult *result);
static xmlNodePtr XPathSegmentExtractNode(char segment[CF_BUFSIZE]);
static char* XPathGetTail(char xpath[CF_BUFSIZE]);
static bool XPathRemoveHead(char xpath[CF_BUFSIZE]);
static bool XPathRemoveTail(char xpath[CF_BUFSIZE]);

//verification using PCRE - ContainsRegex
static bool PredicateHasTail(char *predicate);
static bool PredicateHeadContainsAttribute(char *predicate);
static bool PredicateHeadContainsNode(char *predicate);
static bool XPathHasTail(char *head);
static bool XPathHeadContainsNode(char *head);
static bool XPathHeadContainsPredicate(char *head);
static bool XPathVerifyBuildSyntax(EvalContext *ctx, const char* xpath, Attributes a, const Promise *pp, PromiseResult *result);
static bool XPathVerifyConvergence(const char* xpath);

//helper functions
static xmlChar *CharToXmlChar(char c[CF_BUFSIZE]);
static bool ContainsRegex(const char* rawstring, const char* regex);
static int XmlAttributeCount(const xmlNodePtr node);

#endif

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ScheduleEditXmlOperations(EvalContext *ctx, const Bundle *bp, Attributes a, const Promise *parentp, EditContext *edcontext)
{
    enum editxmltypesequence type;
    char lockname[CF_BUFSIZE];
    CfLock thislock;
    int pass;

    snprintf(lockname, CF_BUFSIZE - 1, "masterfilelock-%s", edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, parentp, true);

    if (thislock.lock == NULL)
    {
        return false;
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_EDIT, "filename", edcontext->filename, CF_DATA_TYPE_STRING, "source=promise");

    for (pass = 1; pass < CF_DONEPASSES; pass++)
    {
        for (type = 0; EDITXMLTYPESEQUENCE[type] != NULL; type++)
        {
            const PromiseType *sp = BundleGetPromiseType(bp, EDITXMLTYPESEQUENCE[type]);
            if (!sp)
            {
                continue;
            }

            EvalContextStackPushPromiseTypeFrame(ctx, sp);
            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                ExpandPromise(ctx, pp, KeepEditXmlPromise, edcontext);

                if (Abort(ctx))
                {
                    YieldCurrentLock(thislock);
                    EvalContextStackPopFrame(ctx);
                    return false;
                }
            }
            EvalContextStackPopFrame(ctx);
        }
    }

    YieldCurrentLock(thislock);
    return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static PromiseResult KeepEditXmlPromise(EvalContext *ctx, const Promise *pp,
                                        ARG_UNUSED void *param)
{
    PromiseBanner(ctx, pp);

    if (strcmp("classes", pp->parent_promise_type->name) == 0)
    {
        return VerifyClassPromise(ctx, pp, NULL);
    }
    else if (strcmp("build_xpath", pp->parent_promise_type->name) == 0)
    {
        Attributes a = GetInsertionAttributes(ctx, pp);
#ifdef HAVE_LIBXML2
        EditContext *edcontext = param;
        PromiseResult result = PROMISE_RESULT_NOOP;
        VerifyXPathBuild(ctx, a, pp, edcontext, &result);
        return result;
#else
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Cannot edit XML files without LIBXML2.");
        return PROMISE_RESULT_FAIL;
#endif
    }
    else if (strcmp("delete_tree", pp->parent_promise_type->name) == 0)
    {
        Attributes a = GetDeletionAttributes(ctx, pp);
#ifdef HAVE_LIBXML2
        EditContext *edcontext = param;
        PromiseResult result = VerifyTreeDeletions(ctx, a, pp, edcontext);
        return result;
#else
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Cannot edit XML files without LIBXML2");
        return PROMISE_RESULT_FAIL;
#endif
    }
    else if (strcmp("insert_tree", pp->parent_promise_type->name) == 0)
    {
        Attributes a = GetInsertionAttributes(ctx, pp);
#ifdef HAVE_LIBXML2
        EditContext *edcontext = param;
        PromiseResult result = VerifyTreeInsertions(ctx, a, pp, edcontext);
        return result;
#else
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Cannot edit XML files without LIBXML2");
        return PROMISE_RESULT_FAIL;
#endif
    }
    else if (strcmp("delete_attribute", pp->parent_promise_type->name) == 0)
    {
        Attributes a = GetDeletionAttributes(ctx, pp);
#ifdef HAVE_LIBXML2
        EditContext *edcontext = param;
        PromiseResult result = VerifyAttributeDeletions(ctx, a, pp, edcontext);
        return result;
#else
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Cannot edit XML files without LIBXML2");
        return PROMISE_RESULT_FAIL;
#endif
    }
    else if (strcmp("set_attribute", pp->parent_promise_type->name) == 0)
    {
        Attributes a = GetInsertionAttributes(ctx, pp);
#ifdef HAVE_LIBXML2
        EditContext *edcontext = param;
        PromiseResult result = VerifyAttributeSet(ctx, a, pp, edcontext);
        return result;
#else
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Cannot edit XML files without LIBXML2");
        return PROMISE_RESULT_FAIL;
#endif
    }
    else if (strcmp("delete_text", pp->parent_promise_type->name) == 0)
    {
        Attributes a = GetDeletionAttributes(ctx, pp);
#ifdef HAVE_LIBXML2
        EditContext *edcontext = param;
        PromiseResult result = VerifyTextDeletions(ctx, a, pp, edcontext);
        return result;
#else
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Cannot edit XML files without LIBXML2");
        return PROMISE_RESULT_FAIL;
#endif
    }
    else if (strcmp("set_text", pp->parent_promise_type->name) == 0)
    {
        Attributes a = GetInsertionAttributes(ctx, pp);
#ifdef HAVE_LIBXML2
        EditContext *edcontext = param;
        PromiseResult result = VerifyTextSet(ctx, a, pp, edcontext);
        return result;
#else
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Cannot edit XML files without LIBXML2");
        return PROMISE_RESULT_FAIL;
#endif
    }
    else if (strcmp("insert_text", pp->parent_promise_type->name) == 0)
    {
        Attributes a = GetInsertionAttributes(ctx, pp);
#ifdef HAVE_LIBXML2
        EditContext *edcontext = param;
        PromiseResult result = VerifyTextInsertions(ctx, a, pp, edcontext);
        return result;
#else
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Cannot edit XML files without LIBXML2");
        return PROMISE_RESULT_FAIL;
#endif
    }
    else if (strcmp("reports", pp->parent_promise_type->name) == 0)
    {
        return VerifyReportPromise(ctx, pp);
    }

    return PROMISE_RESULT_NOOP;
}

/***************************************************************************/

#ifdef HAVE_LIBXML2

/***************************************************************************/

static bool VerifyXPathBuild(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlDocPtr doc = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE], rawxpath[CF_BUFSIZE] = { 0 };

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (a.xml.havebuildxpath)
    {
        strcpy(rawxpath, a.xml.build_xpath);
    }
    else
    {
        strcpy(rawxpath, pp->promiser);
    }

    if (!SanityCheckXPathBuild(ctx, a, pp, result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised XPath build: '%s', breaks its own promises", rawxpath);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    if ((doc = edcontext->xmldoc) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to load XML document");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "buildxpath-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return false;
    }

    //build XPath in an empty file
    if (!xmlDocGetRootElement(doc))
    {
        if (BuildXPathInFile(ctx, rawxpath, doc, a, pp, edcontext, result))
        {
            (edcontext->num_edits)++;
        }
    }
    //build XPath in a nonempty file
    else if (BuildXPathInNode(ctx, rawxpath, doc, a, pp, edcontext, result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
    return true;
}

/***************************************************************************/

static PromiseResult VerifyTreeDeletions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTreeDeletions(a))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised tree deletion '%s' is inconsistent", pp->promiser);
        return PROMISE_RESULT_INTERRUPTED;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (a.xml.havebuildxpath && !VerifyXPathBuild(ctx, a, pp, edcontext, &result))
    {
        return result;
    }

    if ((doc = edcontext->xmldoc) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to load XML document");
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (!XmlSelectNode(ctx, a.xml.select_xpath, doc, &docnode, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
            "The promised XPath pattern '%s', was NOT successful when selecting an edit node, in XML document '%s)",
             a.xml.select_xpath, edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "deletetree-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return result;
    }

    if (DeleteTreeInNode(ctx, pp->promiser, doc, docnode, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);

    return result;
}

/***************************************************************************/

static PromiseResult VerifyTreeInsertions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!SanityCheckTreeInsertions(a, edcontext))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised tree insertion '%s' breaks its own promises", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(ctx, a, pp, edcontext, &result))
    {
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if ((doc = edcontext->xmldoc) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to load XML document");
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    //if file is not empty: select an edit node, for tree insertion
    if (a.xml.haveselectxpath && !XmlSelectNode(ctx, a.xml.select_xpath, doc, &docnode, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised XPath pattern '%s', was NOT successful when selecting an edit node, in XML document '%s'",
             a.xml.select_xpath, edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "inserttree-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return result;
    }

    //insert tree into empty file or selected node
    if (!a.xml.haveselectxpath)
    {
        if (InsertTreeInFile(ctx, pp->promiser, doc, a, pp, edcontext, &result))
        {
            (edcontext->num_edits)++;
        }
    }
    else if (InsertTreeInNode(ctx, pp->promiser, doc, docnode, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
    return result;
}

/***************************************************************************/

static PromiseResult VerifyAttributeDeletions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!SanityCheckAttributeDeletions(a))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised attribute deletion '%s', is inconsistent", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(ctx, a, pp, edcontext, &result))
    {
        return result;
    }

    if ((doc = edcontext->xmldoc) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to load XML document");
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (!XmlSelectNode(ctx, a.xml.select_xpath, doc, &docnode, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
            "The promised XPath pattern '%s', was NOT successful when selecting an edit node, in XML document '%s'",
             a.xml.select_xpath, edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "deleteattribute-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return result;
    }

    if (DeleteAttributeInNode(ctx, pp->promiser, docnode, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
    return result;
}

/***************************************************************************/

static PromiseResult VerifyAttributeSet(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!SanityCheckAttributeSet(a))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised attribute set '%s', breaks its own promises", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(ctx, a, pp, edcontext, &result))
    {
        return result;
    }

    if ((doc = edcontext->xmldoc) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to load XML document");
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (!XmlSelectNode(ctx, a.xml.select_xpath, doc, &docnode, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
            "The promised XPath pattern '%s', was NOT successful when selecting an edit node, in XML document '%s'",
             a.xml.select_xpath, edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "setattribute-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return result;
    }

    if (SetAttributeInNode(ctx, pp->promiser, a.xml.attribute_value, docnode, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
    return result;
}

/***************************************************************************/

static PromiseResult VerifyTextDeletions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!SanityCheckTextDeletions(a))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised text deletion '%s' is inconsistent", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(ctx, a, pp, edcontext, &result))
    {
        return result;
    }

    if ((doc = edcontext->xmldoc) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to load XML document");
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (!XmlSelectNode(ctx, a.xml.select_xpath, doc, &docnode, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
            "The promised XPath pattern '%s', was NOT successful when selecting an edit node, in XML document '%s'",
             a.xml.select_xpath, edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "deletetext-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return result;
    }

    if (DeleteTextInNode(ctx, pp->promiser, doc, docnode, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
    return result;
}

/***************************************************************************/

static PromiseResult VerifyTextSet(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!SanityCheckTextSet(a))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised text set '%s' breaks its own promises", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(ctx, a, pp, edcontext, &result))
    {
        return result;
    }

    if ((doc = edcontext->xmldoc) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to load XML document");
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (!XmlSelectNode(ctx, a.xml.select_xpath, doc, &docnode, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
            "The promised XPath pattern '%s', was NOT successful when selecting an edit node, in XML document '%s'",
             a.xml.select_xpath, edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "settext-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return result;
    }

    if (SetTextInNode(ctx, pp->promiser, doc, docnode, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
    return result;
}

/***************************************************************************/

static PromiseResult VerifyTextInsertions(EvalContext *ctx, Attributes a, const Promise *pp, EditContext *edcontext)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!SanityCheckTextInsertions(a))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised text insertion '%s' breaks its own promises", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(ctx, a, pp, edcontext, &result))
    {
        return result;
    }

    if ((doc = edcontext->xmldoc) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to load XML document");
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    if (!XmlSelectNode(ctx, a.xml.select_xpath, doc, &docnode, a, pp, edcontext, &result))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
            "The promised XPath pattern '%s', was NOT successful when selecting an edit node, in XML document '%s'",
             a.xml.select_xpath, edcontext->filename);
        result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
        return result;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "inserttext-%s-%s", pp->promiser, edcontext->filename);
    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, true);

    if (thislock.lock == NULL)
    {
        return result;
    }

    if (InsertTextInNode(ctx, pp->promiser, doc, docnode, a, pp, edcontext, &result))
    {
        (edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
    return result;
}

/***************************************************************************/

/*

This should provide pointers to the edit node within the XML document.
It returns true if a match was identified, else false.

If no such node matches, docnode should point to NULL

*/
static bool XmlSelectNode(EvalContext *ctx, char *rawxpath, xmlDocPtr doc, xmlNodePtr *docnode, Attributes a,
                          const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlNodePtr cur = NULL;
    xmlXPathContextPtr xpathCtx = NULL;
    xmlXPathObjectPtr xpathObj = NULL;
    xmlNodeSetPtr nodes = NULL;
    const xmlChar* xpathExpr = NULL;
    int i, size = 0;
    bool valid = true;

    *docnode = NULL;

    if (!XPathVerifyConvergence(rawxpath))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "select_xpath expression '%s', is NOT convergent", rawxpath);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    if ((xpathExpr = CharToXmlChar(rawxpath)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to create new XPath expression '%s'", rawxpath);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    if ((xpathCtx = xmlXPathNewContext(doc)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to create new XPath context '%s'", rawxpath);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    if ((xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to evaluate XPath expression '%s'", xpathExpr);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        xmlXPathFreeContext(xpathCtx); 
        return false;
    }

    nodes = xpathObj->nodesetval;

    if ((size = nodes ? nodes->nodeNr : 0) == 0)
    {
        valid = false;
    }

    if (size > 1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Current select_xpath expression '%s', returns (%d) edit nodes in XML document '%s', please modify expression to select a unique edit node",
             xpathExpr, size, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        valid = false;
    }

    //select first matching node
    if (valid)
    {
        for (i = 0; i < size; ++i)
        {
            if (nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
            {
                cur = nodes->nodeTab[i];
                break;
            }
        }

        if (cur == NULL)
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a,
                 "The promised XPath pattern '%s', was NOT found when selecting an edit node, in XML document '%s'",
                 xpathExpr, edcontext->filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
            valid = false;
        }
    }

    *docnode = cur;

    xmlXPathFreeContext(xpathCtx);
    xmlXPathFreeObject(xpathObj);

    return valid;
}

/***************************************************************************/

static bool BuildXPathInFile(EvalContext *ctx, char rawxpath[CF_BUFSIZE], xmlDocPtr doc, Attributes a,
                             const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlNodePtr docnode = NULL, head = NULL;
    char copyxpath[CF_BUFSIZE] = { 0 };

    strcpy(copyxpath, rawxpath);

    if (xmlDocGetRootElement(doc))
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "The promised XML document  '%s' already exists and contains a root element (promise kept)",
             edcontext->filename);
        return false;
    }

    //set rootnode
    if ((docnode = XPathHeadExtractNode(ctx, copyxpath, a, pp, result)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "Unable to extract root node from XPath '%s', to be inserted into an empty XML document '%s'",
             rawxpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    if (docnode == NULL || (docnode->name) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The extracted root node, from XPath '%s', to be inserted into an empty XML document '%s', is empty",
             rawxpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //insert the content into new XML document, beginning from root node
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Building XPath '%s', into an empty XML document '%s'",
         rawxpath, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    if (xmlDocSetRootElement(doc, docnode) != NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised XPath '%s' was NOT built successfully into an empty XML document '%s'",
             rawxpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    XPathRemoveHead(copyxpath);

    //extract and insert nodes from tail
    while ((strlen(copyxpath) > 0) && ((head = XPathHeadExtractNode(ctx, copyxpath, a, pp, result)) != NULL))
    {
        xmlAddChild(docnode, head);
        docnode = head;
        XPathRemoveHead(copyxpath);
    }

    return true;
}

/***************************************************************************/

static bool BuildXPathInNode(EvalContext *ctx, char rawxpath[CF_BUFSIZE], xmlDocPtr doc, Attributes a,
                             const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlNodePtr docnode = NULL,  head = NULL, tail = NULL;
    char copyxpath[CF_BUFSIZE] = { 0 };

    strcpy(copyxpath, rawxpath);

    //build XPath from tail while locating insertion node
    while ((strlen(copyxpath) > 0) && (!XmlSelectNode(ctx, copyxpath, doc, &docnode, a, pp, edcontext, result)))
    {
        if (XPathHasTail (copyxpath))
        {
            head = XPathTailExtractNode(ctx, copyxpath, a, pp, result);
            XPathRemoveTail(copyxpath);
        }
        else
        {
            head = XPathHeadExtractNode(ctx, copyxpath, a, pp, result);
            XPathRemoveHead(copyxpath);
        }

        if (head && tail)
        {
            xmlAddChild(head, tail);
        }
        tail = head;
    }

    //insert the new tree into selected node in XML document
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Building XPath '%s', in XML document '%s'",
         rawxpath, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    if (docnode != NULL)
    {
        xmlAddChild(docnode, tail);
    }
    //insert the new tree into root, in the case where unique node was not found, in XML document
    else
    {
        docnode = xmlDocGetRootElement(doc);
        xmlAddChild(docnode, tail);
    }

    return true;
}

/***************************************************************************/

static bool InsertTreeInFile(EvalContext *ctx, char *rawtree, xmlDocPtr doc, Attributes a,
                             const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlNodePtr treenode = NULL, rootnode = NULL;
    xmlChar *buf = NULL;

    //for parsing subtree from memory
    if ((buf = CharToXmlChar(rawtree)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Tree to be inserted '%s' into an empty XML document '%s', was NOT successfully loaded into an XML buffer",
             rawtree, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Tree to be inserted '%s' into an empty XML document '%s', was NOT parsed successfully",
             rawtree, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    if (treenode == NULL || (treenode->name) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised tree to be inserted '%s' into an empty XML document '%s', is empty",
             rawtree, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify treenode does not already exist inside docnode
    if ((rootnode = xmlDocGetRootElement(doc)) != NULL)
    {
        if (!XmlNodesCompare(treenode, rootnode, a, pp))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
                 "The promised tree '%s' is to be inserted into an empty XML document '%s',"
                 " however XML document is NOT empty and tree to be inserted does NOT match existing content."
                 " If you would like to insert into a non-empty XML document, please specify select_xpath expression",
                 rawtree, edcontext->filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "The promised XML document '%s' already exists and contains a root element (promise kept)",
                 edcontext->filename);
        }

        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
             "Need to insert the promised tree '%s' into an empty XML document '%s' - but only a warning was promised",
             rawtree, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return true;
    }

    //insert the content into new XML document
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Inserting tree '%s' into an empty XML document '%s'",
         rawtree, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    if (xmlDocSetRootElement(doc, treenode) != NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised tree '%s' was NOT inserted successfully, into an empty XML document '%s'",
             rawtree, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify node was inserted
    if (((rootnode = xmlDocGetRootElement(doc)) == NULL) || !XmlNodesCompare(treenode, rootnode, a, pp))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised tree '%s' was NOT inserted successfully, into an empty XML document '%s'",
             rawtree, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool DeleteTreeInNode(EvalContext *ctx, char *rawtree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a,
                             const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    //for parsing subtree from memory
    const xmlChar *buf = CharToXmlChar(rawtree);
    if (!buf)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Tree to be deleted '%s' at XPath '%s' in XML document '%s', was NOT successfully loaded into an XML buffer",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //parse the subtree
    xmlNodePtr treenode = NULL;
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Tree to be deleted '%s' at XPath '%s' in XML document '%s', was NOT parsed successfully",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        xmlFreeNode(treenode);
        return false;
    }

    //verify treenode exists inside docnode
    xmlNodePtr deletetree = NULL;
    if ((deletetree = XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "The promised tree to be deleted '%s' does NOT exist, at XPath '%s' in XML document '%s' (promise kept)",
             rawtree, a.xml.select_xpath, edcontext->filename);
        xmlFreeNode(treenode);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
             "Need to delete the promised tree '%s' at XPath '%s' in XML document '%s' - but only a warning was promised",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        xmlFreeNode(treenode);
        xmlFreeNode(deletetree);
        return true;
    }

    //remove the subtree from XML document
    cfPS(ctx,
         LOG_LEVEL_VERBOSE,
         PROMISE_RESULT_CHANGE,
         pp,
         a,
         "Deleting tree '%s' at XPath '%s' in XML document '%s'",
         rawtree,
         a.xml.select_xpath,
         edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    xmlUnlinkNode(deletetree);
    xmlFreeNode(deletetree);

    //verify treenode no longer exists inside docnode
    {
        xmlNodePtr ret = XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp);
        if (ret)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
                 "The promised tree to be deleted '%s' was NOT successfully deleted, at XPath '%s' in XML document '%s'",
                 rawtree, a.xml.select_xpath, edcontext->filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
            xmlFreeNode(treenode);
            xmlFreeNode(ret);
            return false;
        }
    }

    xmlFreeNode(treenode);
    return true;
}

/***************************************************************************/

static bool InsertTreeInNode(EvalContext *ctx, char *rawtree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a,
                             const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlNodePtr treenode = NULL;
    xmlChar *buf = NULL;

    //for parsing subtree from memory
    if ((buf = CharToXmlChar(rawtree)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Tree to be inserted '%s' at XPath '%s' in XML document '%s', was NOT successfully loaded into an XML buffer",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Tree to be inserted '%s' at XPath '%s' in XML document '%s', was NOT parsed successfully",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    if (treenode == NULL || (treenode->name) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a, "The promised tree to be inserted '%s' at XPath '%s' in XML document '%s', is empty",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify treenode does not already exist inside docnode
    if (XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp))
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "The promised tree to be inserted '%s' already exists, at XPath '%s' in XML document '%s' (promise kept)",
             rawtree, a.xml.select_xpath, edcontext->filename);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
             "Need to insert the promised tree '%s' at XPath '%s' in XML document '%s' - but only a warning was promised",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return true;
    }

    //insert the subtree into XML document
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Inserting tree '%s' at XPath '%s' in XML document '%s'",
         rawtree, a.xml.select_xpath, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    if (!xmlAddChild(docnode, treenode))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised tree '%s' was NOT inserted successfully, at XPath '%s' in XML document '%s'",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify node was inserted
    if (!XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised tree '%s' was NOT inserted successfully, at XPath '%s' in XML document '%s'",
             rawtree, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool DeleteAttributeInNode(EvalContext *ctx, char *rawname, xmlNodePtr docnode, Attributes a,
                                  const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlAttrPtr attr = NULL;
    xmlChar *name = NULL;

    if ((name = CharToXmlChar(rawname)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Name of attribute to be deleted '%s', at XPath '%s' in XML document '%s', was NOT successfully loaded into an XML buffer",
             rawname, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify attribute exists inside docnode
    if ((attr = xmlHasProp(docnode, name)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
             "The promised attribute to be deleted '%s', does NOT exist, at XPath '%s' in XML document '%s' (promise kept)",
             rawname, a.xml.select_xpath, edcontext->filename);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
             "Need to delete the promised attribute '%s', at XPath '%s' in XML document '%s' - but only a warning was promised",
             rawname, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return true;
    }

    //delete attribute from docnode
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Deleting attribute '%s', at XPath '%s' in XML document '%s'",
             rawname, a.xml.select_xpath, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    if ((xmlRemoveProp(attr)) == -1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised attribute to be deleted '%s', was NOT deleted successfully, at XPath '%s' in XML document '%s'.",
             rawname, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify attribute no longer exists inside docnode
    if ((attr = xmlHasProp(docnode, name)) != NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised attribute to be deleted '%s', was NOT deleted successfully, at XPath '%s' in XML document '%s'",
             rawname, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SetAttributeInNode(EvalContext *ctx, char *rawname, char *rawvalue, xmlNodePtr docnode, Attributes a,
                               const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlAttrPtr attr = NULL;
    xmlChar *name = NULL;
    xmlChar *value = NULL;

    if ((name = CharToXmlChar(rawname)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Name of attribute to be set '%s', at XPath '%s' in XML document '%s', was NOT successfully loaded into an XML buffer",
             rawname, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    if ((value = CharToXmlChar(rawvalue)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Value of attribute to be set '%s', at XPath '%s' in XML document '%s', was NOT successfully loaded into an XML buffer",
             rawvalue, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify attribute does not already exist inside docnode
    if ((attr = XmlVerifyAttributeInNode(name, value, docnode)) != NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
             "The promised attribute to be set, with name '%s' and value '%s', already exists, at XPath '%s' in XML document '%s' (promise kept)",
             rawname, rawvalue, a.xml.select_xpath, edcontext->filename);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
             "Need to set the promised attribute, with name '%s' and value '%s', at XPath '%s' in XML document '%s' - but only a warning was promised",
             rawname, rawvalue, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return true;
    }

    //set attribute in docnode
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Setting attribute with name '%s' and value '%s', at XPath '%s' in XML document '%s'",
         rawname, rawvalue, a.xml.select_xpath, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
    if ((attr = xmlSetProp(docnode, name, value)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised attribute to be set, with name '%s' and value '%s', was NOT successfully set, at XPath '%s' in XML document '%s'",
             rawname, rawvalue, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify attribute was inserted
    if ((attr = XmlVerifyAttributeInNode(name, value, docnode)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised attribute to be set, with name '%s' and value '%s', was NOT successfully set, at XPath '%s' in XML document '%s'",
             rawname, rawvalue, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool DeleteTextInNode(EvalContext *ctx, char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a,
                             const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlNodePtr elemnode, copynode;
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Text to be deleted '%s' at XPath '%s' in XML document '%s', was NOT successfully loaded into an XML buffer",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify text exists inside docnode
    if (!XmlVerifyTextInNodeSubstring(text, docnode))
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
             "The promised text to be deleted '%s' does NOT exist, at XPath '%s' in XML document '%s' (promise kept)",
             rawtext, a.xml.select_xpath, edcontext->filename);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
             "Need to delete the promised text '%s' at XPath '%s' in XML document '%s' - but only a warning was promised",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return true;
    }

    //delete text from docnode
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Deleting text '%s' at XPath '%s' in XML document '%s'",
         rawtext, a.xml.select_xpath, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

    //node contains text
    if (xmlNodeIsText(docnode->children))
    {
        xmlNodeSetContent(docnode->children, "");
    }
    //node does not contain text
    else
    {
        //remove and set aside the elements in the node
        elemnode = xmlFirstElementChild(docnode);
        copynode = xmlDocCopyNodeList(doc, elemnode);

        xmlNodeSetContent(docnode, "");

        //re-insert elements after the inserted text
        xmlAddChildList(docnode, copynode);
    }

    //verify text no longer exists inside docnode
    if (XmlVerifyTextInNodeSubstring(text, docnode))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised text '%s' was NOT deleted successfully, at XPath '%s' in XML document '%s'",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SetTextInNode(EvalContext *ctx, char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a,
                          const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlNodePtr elemnode, copynode;
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Text to be set '%s' at XPath '%s' in XML document '%s', was NOT successfully loaded into an XML buffer",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify text does not exist inside docnode
    if (XmlVerifyTextInNodeExact(text, docnode))
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
             "The promised text to be set '%s' already exists, at XPath '%s' in XML document '%s' (promise kept)",
             rawtext, a.xml.select_xpath, edcontext->filename);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
             "Need to set the promised text '%s' at XPath '%s' in XML document '%s' - but only a warning was promised",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return true;
    }

    //set text in docnode
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Setting text '%s' at XPath '%s' in XML document '%s'",
         rawtext, a.xml.select_xpath, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

    //node already contains text
    if (xmlNodeIsText(docnode->children))
    {
        xmlNodeSetContent(docnode->children, text);
    }
    //node does not contain text
    else
    {
        //remove and set aside the elements in the node
        elemnode = xmlFirstElementChild(docnode);
        copynode = xmlDocCopyNodeList(doc, elemnode);

        xmlNodeSetContent(docnode, text);

        //re-insert elements after the inserted text
        xmlAddChildList(docnode, copynode);
    }

    //verify text was inserted
    if (!XmlVerifyTextInNodeExact(text, docnode))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised text '%s' was NOT set successfully, at XPath '%s' in XML document '%s'",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool InsertTextInNode(EvalContext *ctx, char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a,
                             const Promise *pp, EditContext *edcontext, PromiseResult *result)
{
    xmlNodePtr elemnode, copynode;
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Text to be inserted '%s' at XPath '%s' in XML document '%s', was NOT successfully loaded into an XML buffer",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    //verify text does not exist inside docnode
    if (XmlVerifyTextInNodeSubstring(text, docnode))
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
             "The promised text to be inserted '%s' already exists, at XPath '%s' in XML document '%s' (promise kept)",
             rawtext, a.xml.select_xpath, edcontext->filename);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
             "Need to insert the promised text '%s' at XPath '%s' in XML document '%s' - but only a warning was promised",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return true;
    }

    //insert text into docnode
    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Inserting text '%s' at XPath '%s' in XML document '%s'",
         rawtext, a.xml.select_xpath, edcontext->filename);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

    //node already contains text
    if (xmlNodeIsText(docnode->children))
    {
        xmlNodeAddContent(docnode->children, text);
    }
    //node does not contain text
    else
    {
        //remove and set aside the elements in the node
        elemnode = xmlFirstElementChild(docnode);
        copynode = xmlDocCopyNodeList(doc, elemnode);

        xmlNodeSetContent(docnode, "");
        xmlNodeAddContent(docnode, text);

        //re-insert elements after the inserted text
        xmlAddChildList(docnode, copynode);
    }

    //verify text was inserted
    if (!XmlVerifyTextInNodeSubstring(text, docnode))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "The promised text '%s' was NOT inserted successfully, at XPath '%s' in XML document '%s'",
             rawtext, a.xml.select_xpath, edcontext->filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckXPathBuild(EvalContext *ctx, Attributes a, const Promise *pp, PromiseResult *result)
{
    char rawxpath[CF_BUFSIZE] = { 0 };

    if (a.xml.havebuildxpath)
    {
        strcpy(rawxpath, a.xml.build_xpath);
    }
    else
    {
        strcpy(rawxpath, pp->promiser);
    }

    if ((strcmp("build_xpath", pp->parent_promise_type->name) == 0) && (a.xml.havebuildxpath))
    {
        Log(LOG_LEVEL_ERR, "Attribute: build_xpath is not allowed within bundle: build_xpath");
        return false;
    }

    if (a.xml.haveselectxpath && !a.xml.havebuildxpath)
    {
        Log(LOG_LEVEL_ERR, "XPath build does not require select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyBuildSyntax(ctx, rawxpath, a, pp, result))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTreeDeletions(Attributes a)
{
    if (!a.xml.haveselectxpath)
    {
        Log(LOG_LEVEL_ERR,
              "Tree deletion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTreeInsertions(Attributes a, EditContext *edcontext)
{
    if ((a.xml.haveselectxpath && !a.xml.havebuildxpath && !xmlDocGetRootElement(edcontext->xmldoc)))
    {
        Log(LOG_LEVEL_ERR,
              "Tree insertion into an empty file, using select_xpath, does not make sense");
        return false;
    }
    else if ((!a.xml.haveselectxpath &&  a.xml.havebuildxpath))
    {
        Log(LOG_LEVEL_ERR,
              "Tree insertion requires select_xpath to be specified, unless inserting into an empty file");
        return false;
    }

    if (a.xml.haveselectxpath && !XPathVerifyConvergence(a.xml.select_xpath))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckAttributeDeletions(Attributes a)
{
    if (!(a.xml.haveselectxpath))
    {
        Log(LOG_LEVEL_ERR, "Attribute deletion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckAttributeSet(Attributes a)
{
    if (!(a.xml.haveselectxpath))
    {
        Log(LOG_LEVEL_ERR, "Attribute insertion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTextDeletions(Attributes a)
{
    if (!(a.xml.haveselectxpath))
    {
        Log(LOG_LEVEL_ERR, "Tree insertion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTextSet(Attributes a)
{
    if (!(a.xml.haveselectxpath))
    {
        Log(LOG_LEVEL_ERR, "Tree insertion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTextInsertions(Attributes a)
{
    if (!(a.xml.haveselectxpath))
    {
        Log(LOG_LEVEL_ERR, "Tree insertion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

int XmlCompareToFile(xmlDocPtr doc, char *file, EditDefaults edits)
/* returns true if XML on disk is identical to XML in memory */
{
    struct stat statbuf;
    xmlDocPtr cmpdoc = NULL;

    if (stat(file, &statbuf) == -1)
    {
        return false;
    }

    if (doc == NULL && statbuf.st_size == 0)
    {
        return true;
    }

    if (doc == NULL)
    {
        return false;
    }

    if (!LoadFileAsXmlDoc(&cmpdoc, file, edits))
    {
        return false;
    }

    if (!XmlDocsEqualMem(cmpdoc, doc))
    {
        xmlFreeDoc(cmpdoc);
        return false;
    }

    xmlFreeDoc(cmpdoc);

    return true;
}

/*********************************************************************/

static bool XmlDocsEqualMem(xmlDocPtr doc1, xmlDocPtr doc2)
{
    xmlChar *mem1;
    xmlChar *mem2;
    int memsize1;
    int memsize2;
    int equal = true;

    if (!doc1 && !doc2)
    {
        return true;
    }

    if (!doc1 || !doc2)
    {
        return false;
    }

    xmlDocDumpMemory(doc1, &mem1, &memsize1);
    xmlDocDumpMemory(doc2, &mem2, &memsize2);

    if (!xmlStrEqual(mem1, mem2))
    {
        equal = false;
    }

    xmlFree(mem1);
    xmlFree(mem2);

    return equal;
}

/***************************************************************************/

static bool XmlNodesCompare(const xmlNodePtr node1, const xmlNodePtr node2, Attributes a, const Promise *pp)
/* Does node1 contain all content(tag/attributes/text/nodes) found in node2? */
{
    int compare = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    if (!XmlNodesCompareTags(node1, node2))
    {
        compare = false;
    }

    if (compare && !XmlNodesCompareAttributes(node1, node2))
    {
        compare = false;
    }

    if (compare && !XmlNodesCompareText(node1, node2))
    {
        compare = false;
    }

    if (compare && !XmlNodesCompareNodes(node1, node2, a, pp))
    {
        compare = false;
    }

    return compare;
}

/*********************************************************************/

static bool XmlNodesCompareAttributes(const xmlNodePtr node1, const xmlNodePtr node2)
/* Does node1 contain same attributes found in node2? */
{
    xmlAttrPtr attr1 = NULL;
    xmlAttrPtr attr2 = NULL;
    xmlChar *value = NULL;
    int count1, count2;
    int compare = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    if ((node1->properties) == NULL && (node2->properties) == NULL)
    {
        return true;
    }

    if ((node1->properties) == NULL || (node2->properties) == NULL)
    {
        return false;
    }

    count1 = XmlAttributeCount(node1);
    count2 = XmlAttributeCount(node2);

    if (count1 != count2)
    {
        return false;
    }

    //get attribute list from node1 and node2
    attr1 = node1->properties;
    attr2 = node2->properties;

    //check that each attribute in node1 is in node2
    while (attr1)
    {
        value = xmlNodeGetContent(attr1->children);

        if ((XmlVerifyAttributeInNode(attr1->name, value, node2)) == NULL)
        {
            xmlFree(value);
            compare = false;
            break;
        }
        xmlFree(value);

        attr1 = attr1->next;
        attr2 = attr2->next;
    }

    return compare;
}

/*********************************************************************/

static bool XmlNodesCompareNodes(const xmlNodePtr node1, const xmlNodePtr node2, Attributes a, const Promise *pp)
/* Does node1 contain same nodes found in node2? */
{
    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    int count1 = xmlChildElementCount(node1);
    int count2 = xmlChildElementCount(node2);

    if (count1 != count2)
    {
        return false;
    }

    xmlNodePtr child1 = xmlFirstElementChild(node1);
    bool compare = true;
    while (child1 != NULL)
    {
        if (!XmlVerifyNodeInNodeExact(child1, node2, a, pp))
        {
            compare = false;
            break;
        }
        child1 = xmlNextElementSibling(child1);
    }

    return compare;
}

/*********************************************************************/

static bool XmlNodesCompareTags(const xmlNodePtr node1, const xmlNodePtr node2)
/* Does node1 contain same tag found in node2? */
{
    int compare = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    if ((node1->name) == NULL && (node2->name) == NULL)
    {
        return true;
    }

    if ((node1->name) == NULL || (node2->name) == NULL)
    {
        return false;
    }

    //check tag in node1 is the same as tag in node2
    if (!xmlStrEqual(node1->name, node2->name))
    {
        compare = false;
    }

    return compare;
}

/*********************************************************************/

static bool XmlNodesCompareText(xmlNodePtr node1, xmlNodePtr node2)
/* Does node1 contain same text found in node2? */
{
    xmlChar *text1 = NULL, *text2 = NULL;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    //get text from nodes
    text1 = xmlNodeGetContent(node1->children);
    text2 = xmlNodeGetContent(node2->children);

    if (!text1 && !text2)
    {
        return true;
    }

    if (!text2)
    {
        return false;
    }

    //check text in node1 is the same as text in node2
    if (!xmlStrEqual(text1, text2))
    {
        return false;
    }

    return true;
}

/*********************************************************************/

static bool XmlNodesSubset(const xmlNodePtr node1, const xmlNodePtr node2, Attributes a, const Promise *pp)
/* Does node1 contain matching subset of content(tag/attributes/text/nodes) found in node2? */
{
    int subset = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node2)
    {
        return false;
    }

    if (!XmlNodesCompareTags(node1, node2))
    {
        subset = false;
    }

    if (subset && !XmlNodesSubsetOfAttributes(node1, node2))
    {
        subset = false;
    }

    if (subset && !XmlNodesSubstringOfText(node1, node2))
    {
        subset = false;
    }

    if (subset && !XmlNodesSubsetOfNodes(node1, node2, a, pp))
    {
        subset = false;
    }

    return subset;
}

/*********************************************************************/

static bool XmlNodesSubsetOfAttributes(const xmlNodePtr node1, const xmlNodePtr node2)
/* Does node1 contain matching subset of attributes found in node2? */
{
    xmlAttrPtr attr1 = NULL;
    int subset = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    if ((node1->properties) == NULL && (node2->properties) == NULL)
    {
        return true;
    }

    if ((node2->properties) == NULL)
    {
        return false;
    }

    //get attribute list from node1
    attr1 = node1->properties;

    //check that each attribute in node1 is in node2
    while (attr1)
    {
        xmlChar *value = xmlNodeGetContent(attr1->children);

        if ((XmlVerifyAttributeInNode(attr1->name, value, node2)) == NULL)
        {
            subset = false;
            xmlFree(value);
            break;
        }

        attr1 = attr1->next;
        xmlFree(value);
    }

    return subset;
}

/*********************************************************************/

static bool XmlNodesSubsetOfNodes(const xmlNodePtr node1, const xmlNodePtr node2, Attributes a, const Promise *pp)
/* Does node1 contain matching subset of nodes found in node2? */
{
    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    //get node list from node1 and node2
    xmlNodePtr child1 = xmlFirstElementChild(node1);
    bool subset = true;
    while (child1 != NULL)
    {
        if (!XmlVerifyNodeInNodeExact(child1, node2, a, pp))
        {
            subset = false;
            break;
        }
        child1 = xmlNextElementSibling(child1);
    }

    return subset;
}

/*********************************************************************/

static bool XmlNodesSubstringOfText(const xmlNodePtr node1, const xmlNodePtr node2)
/* Does node1 contain matching substring of text found in node2? */
{
    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    xmlChar *text1 = xmlNodeGetContent(node1->children);
    if (!text1)
    {
        return true;
    }

    xmlChar *text2 = xmlNodeGetContent(node2->children);
    if (!text2)
    {
        xmlFree(text1);
        return false;
    }

    if (!xmlStrstr(text2, text1))
    {
        xmlFree(text1);
        xmlFree(text2);
        return false;
    }

    xmlFree(text1);
    xmlFree(text2);
    return true;
}

/*********************************************************************/

xmlAttrPtr XmlVerifyAttributeInNode(const xmlChar *name, xmlChar *value, const xmlNodePtr node)
/* Does node contain an attribute with given name and value?
   Returns a pointer to attribute found in node or NULL */
{
    xmlAttrPtr attr2 = NULL;
    xmlChar *value2 = NULL;

    if ((node == NULL) || (name == NULL) || (node->properties == NULL))
    {
        return NULL;
    }

    //get attribute with matching name from node, if it exists
    if ((attr2 = xmlHasProp(node, name)) == NULL)
    {
        return NULL;
    }

    //compare values
    value2 = xmlNodeGetContent(attr2->children);

    if (!xmlStrEqual(value, value2))
    {
        xmlFree(value2);
        return NULL;
    }

    xmlFree(value2);
    return attr2;
}

/*********************************************************************/

bool XmlVerifyTextInNodeExact(const xmlChar *text, const xmlNodePtr node)
/* Does node contain: text content exactly matching the givin string text?
   Returns a pointer to text content in node or NULL */
{
    if (node == NULL)
    {
        return false;
    }

    xmlChar *text2 = xmlNodeGetContent(node->children);

    if (!xmlStrEqual(text2, text))
    {
        xmlFree(text2);
        return false;
    }

    xmlFree(text2);
    return true;
}

/*********************************************************************/

bool XmlVerifyTextInNodeSubstring(const xmlChar *text, xmlNodePtr node)
/* Does node contain: text content, contains substring, matching the given string of text?
   Returns a pointer to text content in node or NULL */
{
    if (!node)
    {
        return false;
    }

    xmlChar *text2 = xmlNodeGetContent(node->children);

    if (!xmlStrstr(text2, text))
    {
        xmlFree(text2);
        return false;
    }

    xmlFree(text2);
    return true;
}

/*********************************************************************/

static bool XmlVerifyNodeInNodeExact(const xmlNodePtr node1, const xmlNodePtr node2, Attributes a, const Promise *pp)
/* Does node2 contain a node with content matching all content in node1?
   Returns a pointer to node found in node2 or NULL */
{

    if ((node1 == NULL) || (node2 == NULL))
    {
        return false;
    }

    xmlNodePtr comparenode = xmlFirstElementChild(node2);

    while (comparenode != NULL)
    {
        if (XmlNodesCompare(node1, comparenode, a, pp))
        {
            return true;
        }

        comparenode = xmlNextElementSibling(comparenode);
    }

    return false;
}

/*********************************************************************/

xmlNodePtr XmlVerifyNodeInNodeSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, const Promise *pp)
/* Does node2 contain: node with subset of content matching all content in node1?
   Returns a pointer to node found in node2 or NULL */
{
    if ((node1 == NULL) || (node2 == NULL))
    {
        return NULL;
    }

    xmlNodePtr comparenode = xmlFirstElementChild(node2);

    while (comparenode != NULL)
    {
        if (XmlNodesSubset(node1, comparenode, a, pp))
        {
            return comparenode;
        }

        comparenode = xmlNextElementSibling(comparenode);
    }

    return NULL;
}

/*********************************************************************/

xmlNodePtr PredicateExtractNode(char predicate[CF_BUFSIZE])
{
    xmlNodePtr node = NULL;
    xmlChar *name = NULL;
    xmlChar *value = NULL;
    char rawname[CF_BUFSIZE] = { 0 }, rawvalue[CF_BUFSIZE] = { 0 }, *tok;

    char *running_start = xstrdup(predicate);
    char *running = running_start;

    //extract node name
    tok = strsep(&running, "| \"\'=");
    while (strcmp(tok, "") == 0)
    {
        tok = strsep(&running, "| \"\'=");
    }
    strcpy(rawname, tok);
    name = CharToXmlChar(rawname);

    //extract node value
    tok = strsep(&running, " \"\'=");
    while (strcmp(tok, "") == 0)
    {
        tok = strsep(&running, " \"\'=");
    }
    strcpy(rawvalue, tok);
    value = CharToXmlChar(rawvalue);

    //create a new node with name and value
    node = xmlNewNode(NULL, name);
    xmlNodeSetContent(node, value);

    free(running_start);

    return node;
}

/*********************************************************************/

static bool PredicateRemoveHead(char predicate[CF_BUFSIZE])
{
    char copypred[CF_BUFSIZE] = { 0 }, *tail = NULL;

    strcpy(copypred, predicate);
    memset(predicate, 0, sizeof(char)*CF_BUFSIZE);

    if (PredicateHasTail(copypred))
    {
        tail =  strchr(copypred+1, '|');
        strcpy(predicate, tail);
    }

    return true;
}

/*********************************************************************/

xmlNodePtr XPathHeadExtractNode(EvalContext *ctx, char xpath[CF_BUFSIZE], Attributes a, const Promise *pp, PromiseResult *result)
{
    xmlNodePtr node = NULL;
    char head[CF_BUFSIZE] = {0}, *tok = NULL;

    char *running_start = xstrdup(xpath);
    char *running = running_start;

    //extract head substring from xpath
    tok = strsep(&running, "/");
    while (strcmp(tok, "") == 0)
    {
        tok = strsep(&running, "/");
    }
    strcpy(head, tok);

    if ((node = XPathSegmentExtractNode(head)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Could not extract node '%s'", head);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
    }

    free(running_start);

    return node;
}

/*********************************************************************/

xmlNodePtr XPathTailExtractNode(EvalContext *ctx, char xpath[CF_BUFSIZE], Attributes a, const Promise *pp, PromiseResult *result)
{
    xmlNodePtr node = NULL;
    char copyxpath[CF_BUFSIZE] = {0}, tail[CF_BUFSIZE] = {0}, *tok = NULL;

    strcpy(copyxpath, xpath);

    //extract tail substring from xpath
    tok =  XPathGetTail(copyxpath);
    strcpy(tail, tok);

    if ((node = XPathSegmentExtractNode(tail)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Could not extract node '%s'", tail);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
    }

    return node;
}

/*********************************************************************/

xmlNodePtr XPathSegmentExtractNode(char segment[CF_BUFSIZE])
{
    xmlNodePtr node = NULL, prednode = NULL;
    xmlChar *name = NULL, *attrname = NULL, *attrvalue = NULL;
    char predicate[CF_BUFSIZE] = { 0 }, rawname[CF_BUFSIZE] = { 0 }, rawvalue[CF_BUFSIZE] = { 0 }, *tok;
    int hasname = false;

    char *running_start = xstrdup(segment);
    char *running = running_start;

    //extract name and predicate substrings from segment
    if (XPathHeadContainsNode(segment))
    {
        //extract node name
        tok = strsep(&running, " []");
        while (strcmp(tok, "") == 0)
        {
            tok = strsep(&running, " []");
        }
        strcpy(rawname, tok);
        name = CharToXmlChar(rawname);

        //create a new node with name
        node = xmlNewNode(NULL, name);
        hasname = true;
    }

    //extract attributes and nodes from predicate
    if (hasname && XPathHeadContainsPredicate(segment))
    {
        //extract predicate
        tok = strsep(&running, "[]");
        while (strcmp(tok, "") == 0)
        {
            tok = strsep(&running, "[]");
        }
        strcpy(predicate, tok);

        while (strlen(predicate) > 0)
        {
            if (PredicateHeadContainsNode(predicate))
            {
                //create a new node within node
                prednode = PredicateExtractNode(predicate);
                xmlAddChild(node, prednode);
            }
            else if (PredicateHeadContainsAttribute(predicate))
            {
                char *running_start2 = xstrdup(predicate);
                char *running2 = running_start2;

                //extract attribute name
                tok = strsep(&running2, "| @\"\'=");
                while (strcmp(tok, "") == 0)
                {
                    tok = strsep(&running2, "| @\"\'=");
                }
                strcpy(rawname, tok);
                attrname = CharToXmlChar(rawname);

                //extract attribute value
                tok = strsep(&running2, "| @\"\'=");
                while (strcmp(tok, "") == 0)
                {
                    tok = strsep(&running2, "| @\"\'=");
                }
                strcpy(rawvalue, tok);
                attrvalue = CharToXmlChar(rawvalue);

                //create a new attribute within node
                xmlNewProp(node, attrname, attrvalue);

                free(running_start2);
            }

            if (PredicateHasTail(predicate))
            {
                PredicateRemoveHead(predicate);
            }
            else
            {
                memset(predicate, 0, sizeof(char)*CF_BUFSIZE);
            }
        }
    }

    free(running_start);

    return node;
}

/*********************************************************************/

char* XPathGetTail(char xpath[CF_BUFSIZE])
{
    char tmpstr[CF_BUFSIZE] = {0}, *tok = NULL;

    char *running_start = xstrdup(xpath);
    char *running = running_start;

    memset(xpath, 0, sizeof(char)*CF_BUFSIZE);

    if (XPathHasTail(running))
    {
        //extract and discard xpath head
        tok = strsep(&running, "/");
        while (strcmp(tok, "") == 0)
        {
            tok = strsep(&running, "/");
        }

        //extract xpath tail
        while ((tok = strsep(&running, "/")) != NULL)
        {
            while (strcmp(tok, "") == 0)
            {
                tok = strsep(&running, "/");
            }
            if (tok)
            {
                strcpy(tmpstr, tok);
            }
        }
        strcpy(xpath, tmpstr);
    }

    free(running_start);

    return xpath;
}

/*********************************************************************/

static bool XPathRemoveHead(char xpath[CF_BUFSIZE])
{
    char copyxpath[CF_BUFSIZE] = { 0 }, *tail = NULL;

    strcpy(copyxpath, xpath);
    memset(xpath, 0, sizeof(char)*CF_BUFSIZE);

    if (XPathHasTail(copyxpath))
    {
        tail =  strchr(copyxpath+1, '/');
        strcpy(xpath, tail);
    }

    return true;
}

/*********************************************************************/

static bool XPathRemoveTail(char xpath[CF_BUFSIZE])
{
    char copyxpath[CF_BUFSIZE] = { 0 }, *tail = NULL;
    int len = 0;

    strcpy(copyxpath, xpath);
    memset(xpath, 0, sizeof(char)*CF_BUFSIZE);

    if (XPathHasTail(copyxpath))
    {
        tail = strrchr(copyxpath, '/');
        len = tail-copyxpath;
        copyxpath[len] = '\0';
        strcpy(xpath, copyxpath);
    }

    return true;
}

/*********************************************************************/

static bool PredicateHasTail(char *predicate)
{
    const char *regexp = "^\\s*\\[?\\s*@?\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?\\s*\\|";

    return (ContainsRegex(predicate, regexp));
}

/*********************************************************************/

static bool PredicateHeadContainsAttribute(char *predicate)
{
    const char *regexp = "^\\s*\\[?\\|?(\\s*@\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?)(\\s*(\\||\\]))?"; // i.e. @name='value' or @name = value or @name ="value"

    return (ContainsRegex(predicate, regexp));
}

/*********************************************************************/

static bool PredicateHeadContainsNode(char *predicate)
{
    const char *regexp = "^\\s*\\[?\\|?(\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?)(\\s*(\\||\\]))?";  // i.e. name='value' or name = value or name ="value"

    return (ContainsRegex(predicate, regexp));
}

/*********************************************************************/

static bool XPathHasTail(char *head)
{
    const char *regexp = "^\\s*\\/?\\s*(\\w|-|\\.)+(\\s*::\\s*(\\w|-|\\.)+\\s*)*\\s*(\\[[^\\[\\]\\/]*\\])?\\s*\\/";

    return (ContainsRegex(head, regexp));
}

/*********************************************************************/

static bool XPathHeadContainsNode(char *head)
{
    const char *regexp = "^(\\/)?(\\w|-|\\.)+((\\s*::\\s*)?(\\w|-|\\.)+)*";

    return (ContainsRegex(head, regexp));
}

/*********************************************************************/

static bool XPathHeadContainsPredicate(char *head)
{
    const char *regexp = "^\\s*\\/?\\s*(\\w|-|\\.)+(\\s*::\\s*(\\w|-|\\.)+)*\\s*\\"       // name
        // [ name='value' | @name = "value" | name = value]
        "[\\s*@?\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?\\s*(\\s*\\|\\s*)?(\\s*@?\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?\\s*)*\\]";

    return (ContainsRegex(head, regexp));
}

/*********************************************************************/

static bool XPathVerifyBuildSyntax(EvalContext *ctx, const char* xpath, Attributes a, const Promise *pp, PromiseResult *result)
/*verify that XPath does not specify position wrt sibling-axis (such as):[#] [last()] [position()] following-sibling:: preceding-sibling:: */
{
    char regexp[CF_BUFSIZE] = {'\0'};

    //check for convergence
    if (!XPathVerifyConvergence(xpath))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Promiser expression (%s) is not convergent", xpath);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }

    // /name[ name = value | @name='value'| . . . | @name = "value" ]/. . .
    strcpy (regexp, "^(\\/(( |\\t)*(\\w|-|\\.)+( |\\t)*)"
    "(\\[( |\\t)*@?(\\w|-|\\.)+( |\\t)*=( |\\t)*(\'|\")?(\\w|-|\\.)+(\'|\")?( |\\t)*"
    "(\\|( |\\t)*@?(\\w|-|\\.)+( |\\t)*=( |\\t)*(\'|\")?(\\w|-|\\.)+(\'|\")?( |\\t)*)*\\])?)*$");

    if (!ContainsRegex(xpath, regexp))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_INTERRUPTED, pp, a,
             "Promiser expression '%s' contains syntax that is not supported for xpath_build. "
             "Please refer to users manual for supported syntax specifications.", xpath);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_INTERRUPTED);
        return false;
    }
    return true;
}

/*********************************************************************/

static bool XPathVerifyConvergence(const char* xpath)
/*verify that XPath does not specify position wrt sibling-axis (such as):[#] [last()] [position()] following-sibling:: preceding-sibling:: */
{
    char regexp[CF_BUFSIZE] = {'\0'};

    //check in predicate
    strcpy (regexp, "\\[\\s*([^\\[\\]]*\\s*(\\||(or)|(and)))?\\s*"     // [ (stuff) (|/or/and)
        // position() (=/!=/</<=/>/>=)
        "((position)\\s*\\(\\s*\\)\\s*((=)|(!=)|(<)|(<=)|(>)|(>=))\\s*)?\\s*"
        // (number) | (number) (+/-/*/div/mod) (number) | last() | last() (+/-/*/div/mod) (number)
        "(((\\d+)\\s*|((last)\\s*\\(\\s*\\)\\s*))(((\\+)|(-)|(\\*)|(div)|(mod))\\s*(\\d+)\\s*)*)\\s*"
        // (|/or/and) (stuff) ]
        "((\\||(or)|(and))[^\\[\\]]*)?\\]"
        // following:: preceding:: following-sibling:: preceding-sibling::
        "|((following)|(preceding))(-sibling)?\\s*(::)");

    if (ContainsRegex(xpath, regexp))
    {
        return false;
    }
    return true;
}

/*********************************************************************/

xmlChar *CharToXmlChar(char c[CF_BUFSIZE])
{
    return BAD_CAST c;
}

/*********************************************************************/

static bool ContainsRegex(const char* rawstring, const char* regex)
{
    int ovector[OVECCOUNT], rc;
    const char *errorstr;
    int erroffset;

    pcre *rx = pcre_compile(regex, 0, &errorstr, &erroffset, NULL);

    if ((rc = pcre_exec(rx, NULL, rawstring, strlen(rawstring), 0, 0, ovector, OVECCOUNT)) >= 0)
    {
        pcre_free(rx);
        return true;
    }

    pcre_free(rx);
    return false;
}

/*********************************************************************/

static int XmlAttributeCount(const xmlNodePtr node)
{
    if (!node)
    {
        return 0;
    }

    xmlAttrPtr attr = node->properties;

    int count = 0;
    while (attr)
    {
        count++;
        attr = attr->next;
    }

    return count;
}

/*********************************************************************/

#endif
