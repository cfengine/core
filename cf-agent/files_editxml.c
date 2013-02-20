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

#include "env_context.h"
#include "promises.h"
#include "files_names.h"
#include "files_edit.h"
#include "vars.h"
#include "item_lib.h"
#include "sort.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "scope.h"
#include "files_interfaces.h"
#include "attributes.h"
#include "cfstream.h"
#include "transaction.h"
#include "logging.h"
#include "policy.h"

enum editxmltypesequence
{
    elx_vars,
    elx_classes,
    elx_delete,
    elx_insert,
    elx_none
};

char *EDITXMLTYPESEQUENCE[] =
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

static void EditXmlClassBanner(enum editxmltypesequence type);
static void KeepEditXmlPromise(Promise *pp);
#ifdef HAVE_LIBXML2
static bool VerifyXPathBuild(Attributes a, Promise *pp);
static void VerifyTreeDeletions(Attributes a, Promise *pp);
static void VerifyTreeInsertions(Attributes a, Promise *pp);
static void VerifyAttributeDeletions(Attributes a, Promise *pp);
static void VerifyAttributeSet(Attributes a, Promise *pp);
static void VerifyTextDeletions(Attributes a, Promise *pp);
static void VerifyTextSet(Attributes a, Promise *pp);
static void VerifyTextInsertions(Attributes a, Promise *pp);
static bool XmlSelectNode(char *xpath, xmlDocPtr doc, xmlNodePtr *docnode, Attributes a, Promise *pp);
static bool BuildXPathInFile(char xpath[CF_BUFSIZE], xmlDocPtr doc, Attributes a, Promise *pp);
static bool BuildXPathInNode(char xpath[CF_BUFSIZE], xmlDocPtr doc, Attributes a, Promise *pp);
static bool DeleteTreeInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool InsertTreeInFile(char *root, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool InsertTreeInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool DeleteAttributeInNode(char *attrname, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool SetAttributeInNode(char *attrname, char *attrvalue, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool DeleteTextInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool SetTextInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool InsertTextInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool SanityCheckXPathBuild(Attributes a, Promise *pp);
static bool SanityCheckTreeDeletions(Attributes a, Promise *pp);
static bool SanityCheckTreeInsertions(Attributes a, Promise *pp);
static bool SanityCheckAttributeDeletions(Attributes a, Promise *pp);
static bool SanityCheckAttributeSet(Attributes a, Promise *pp);
static bool SanityCheckTextDeletions(Attributes a, Promise *pp);
static bool SanityCheckTextSet(Attributes a, Promise *pp);
static bool SanityCheckTextInsertions(Attributes a, Promise *pp);

static bool XmlDocsEqualMem(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp);
static bool XmlNodesCompare(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static bool XmlNodesCompareAttributes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static bool XmlNodesCompareNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static bool XmlNodesCompareTags(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static bool XmlNodesCompareText(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static bool XmlNodesSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static bool XmlNodesSubsetOfAttributes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static bool XmlNodesSubsetOfNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static bool XmlNodesSubstringOfText(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static xmlAttrPtr XmlVerifyAttributeInNode(const xmlChar *attrname, xmlChar *attrvalue, xmlNodePtr node, Attributes a, Promise *pp);
static xmlChar* XmlVerifyTextInNodeExact(const xmlChar *text, xmlNodePtr node, Attributes a, Promise *pp);
static xmlChar* XmlVerifyTextInNodeSubstring(const xmlChar *text, xmlNodePtr node, Attributes a, Promise *pp);
static xmlNodePtr XmlVerifyNodeInNodeExact(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static xmlNodePtr XmlVerifyNodeInNodeSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);

//xpath build functionality
static xmlNodePtr PredicateExtractNode(char predicate[CF_BUFSIZE], Attributes a, Promise *pp);
static bool PredicateRemoveHead(char xpath[CF_BUFSIZE], Attributes a, Promise *pp);

static xmlNodePtr XPathHeadExtractNode(char xpath[CF_BUFSIZE], Attributes a, Promise *pp);
static xmlNodePtr XPathTailExtractNode(char xpath[CF_BUFSIZE], Attributes a, Promise *pp);
static xmlNodePtr XPathSegmentExtractNode(char segment[CF_BUFSIZE], Attributes a, Promise *pp);
static char* XPathGetTail(char xpath[CF_BUFSIZE], Attributes a, Promise *pp);
static bool XPathRemoveHead(char xpath[CF_BUFSIZE], Attributes a, Promise *pp);
static bool XPathRemoveTail(char xpath[CF_BUFSIZE], Attributes a, Promise *pp);

//verification using PCRE - ContainsRegex
static bool PredicateHasTail(char *predicate, Attributes a, Promise *pp);
static bool PredicateHeadContainsAttribute(char *predicate, Attributes a, Promise *pp);
static bool PredicateHeadContainsNode(char *predicate, Attributes a, Promise *pp);
static bool XPathHasTail(char *head, Attributes a, Promise *pp);
static bool XPathHeadContainsNode(char *head, Attributes a, Promise *pp);
static bool XPathHeadContainsPredicate(char *head, Attributes a, Promise *pp);
static bool XPathVerifyBuildSyntax(const char* xpath, Attributes a, Promise *pp);
static bool XPathVerifyConvergence(const char* xpath, Attributes a, Promise *pp);

//helper functions
static xmlChar *CharToXmlChar(char c[CF_BUFSIZE]);
static bool ContainsRegex(const char* rawstring, const char* regex);
static int XmlAttributeCount(xmlNodePtr node, Attributes a, Promise *pp);

#endif

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ScheduleEditXmlOperations(char *filename, Bundle *bp, Attributes a, Promise *parentp,
                               const ReportContext *report_context)
{
    enum editxmltypesequence type;
    SubType *sp;
    char lockname[CF_BUFSIZE];
    const char *bp_stack = THIS_BUNDLE;
    CfLock thislock;
    int pass;

    snprintf(lockname, CF_BUFSIZE - 1, "masterfilelock-%s", filename);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, parentp, true);

    if (thislock.lock == NULL)
    {
        return false;
    }

    NewScope("edit");
    NewScalar("edit", "filename", filename, DATA_TYPE_STRING);

/* Reset the done state for every call here, since bundle is reusable */

    for (type = 0; EDITXMLTYPESEQUENCE[type] != NULL; type++)
    {
        if ((sp = BundleGetSubType(bp, EDITXMLTYPESEQUENCE[type])) == NULL)
        {
            continue;
        }

        for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
        {
            Promise *pp = SeqAt(sp->promises, ppi);
            pp->donep = false;
        }
    }

    for (pass = 1; pass < CF_DONEPASSES; pass++)
    {
        for (type = 0; EDITXMLTYPESEQUENCE[type] != NULL; type++)
        {
            EditXmlClassBanner(type);

            if ((sp = BundleGetSubType(bp, EDITXMLTYPESEQUENCE[type])) == NULL)
            {
                continue;
            }

            BannerSubSubType(bp->name, sp->name);
            THIS_BUNDLE = bp->name;
            SetScope(bp->name);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                pp->edcontext = parentp->edcontext;
                pp->this_server = filename;
                pp->donep = &(pp->done);

                ExpandPromise(AGENT_TYPE_AGENT, bp->name, pp, KeepEditXmlPromise, report_context);

                if (Abort())
                {
                    THIS_BUNDLE = bp_stack;
                    DeleteScope("edit");
                    YieldCurrentLock(thislock);
                    return false;
                }
            }
        }
    }

    DeleteScope("edit");
    SetScope(parentp->bundle);
    THIS_BUNDLE = bp_stack;
    YieldCurrentLock(thislock);
    return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static void EditXmlClassBanner(enum editxmltypesequence type)
{
    if (type != elx_delete)     /* Just parsed all local classes */
    {
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "     ??  Private class context\n");

    AlphaListIterator i = AlphaListIteratorInit(&VADDCLASSES);

    for (const Item *ip = AlphaListIteratorNext(&i); ip != NULL; ip = AlphaListIteratorNext(&i))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "     ??       %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

/***************************************************************************/

static void KeepEditXmlPromise(Promise *pp)
{
    char *sp = NULL;
    Attributes a = { {0} };

    if (!IsDefinedClass(pp->classes, pp->ns))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Skipping whole next edit promise, as context %s is not relevant\n", pp->classes);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
        return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping whole next edit promise (%s), as var-context %s is not relevant\n",
              pp->promiser, sp);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    PromiseBanner(pp);

    if (strcmp("classes", pp->agentsubtype) == 0)
    {
        KeepClassContextPromise(pp);
        return;
    }

    if (strcmp("build_xpath", pp->agentsubtype) == 0)
    {
        a = GetInsertionAttributes(pp);
#ifdef HAVE_LIBXML2
        xmlInitParser();
        VerifyXPathBuild(a, pp);
        xmlCleanupParser();
#else
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "KeepEditXmlPromise", pp, a, " !! Cannot edit XML files without LIBXML2\n");
#endif
        return;
    }

    if (strcmp("delete_tree", pp->agentsubtype) == 0)
    {
        a = GetDeletionAttributes(pp);
#ifdef HAVE_LIBXML2
        xmlInitParser();
        VerifyTreeDeletions(a, pp);
        xmlCleanupParser();
#else
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "KeepEditXmlPromise", pp, a, " !! Cannot edit XML files without LIBXML2\n");
#endif
        return;
    }

    if (strcmp("insert_tree", pp->agentsubtype) == 0)
    {
        a = GetInsertionAttributes(pp);
#ifdef HAVE_LIBXML2
        xmlInitParser();
        VerifyTreeInsertions(a, pp);
        xmlCleanupParser();
#else
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "KeepEditXmlPromise", pp, a, " !! Cannot edit XML files without LIBXML2\n");
#endif
        return;
    }

    if (strcmp("delete_attribute", pp->agentsubtype) == 0)
    {
        a = GetDeletionAttributes(pp);
#ifdef HAVE_LIBXML2
        xmlInitParser();
        VerifyAttributeDeletions(a, pp);
        xmlCleanupParser();
#else
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "KeepEditXmlPromise", pp, a, " !! Cannot edit XML files without LIBXML2\n");
#endif
        return;
    }

    if (strcmp("set_attribute", pp->agentsubtype) == 0)
    {
        a = GetInsertionAttributes(pp);
#ifdef HAVE_LIBXML2
        xmlInitParser();
        VerifyAttributeSet(a, pp);
        xmlCleanupParser();
#else
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "KeepEditXmlPromise", pp, a, " !! Cannot edit XML files without LIBXML2\n");
#endif
        return;
    }

    if (strcmp("delete_text", pp->agentsubtype) == 0)
    {
        a = GetDeletionAttributes(pp);
#ifdef HAVE_LIBXML2
        xmlInitParser();
        VerifyTextDeletions(a, pp);
        xmlCleanupParser();
#else
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "KeepEditXmlPromise", pp, a, " !! Cannot edit XML files without LIBXML2\n");
#endif
        return;
    }

    if (strcmp("set_text", pp->agentsubtype) == 0)
    {
        a = GetInsertionAttributes(pp);
#ifdef HAVE_LIBXML2
        xmlInitParser();
        VerifyTextSet(a, pp);
        xmlCleanupParser();
#else
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "KeepEditXmlPromise", pp, a, " !! Cannot edit XML files without LIBXML2\n");
#endif
        return;
    }

    if (strcmp("insert_text", pp->agentsubtype) == 0)
    {
        a = GetInsertionAttributes(pp);
#ifdef HAVE_LIBXML2
        xmlInitParser();
        VerifyTextInsertions(a, pp);
        xmlCleanupParser();
#else
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "KeepEditXmlPromise", pp, a, " !! Cannot edit XML files without LIBXML2\n");
#endif
        return;
    }

    if (strcmp("reports", pp->agentsubtype) == 0)
    {
        VerifyReportPromise(pp);
        return;
    }
}

/***************************************************************************/

#ifdef HAVE_LIBXML2

/***************************************************************************/

static bool VerifyXPathBuild(Attributes a, Promise *pp)
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

    if (!SanityCheckXPathBuild(a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised XPath build: \"%s\", breaks its own promises", rawxpath);
        return false;
    }

    if ((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to load XML document");
        return false;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "buildxpath-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return false;
    }

    //build XPath in an empty file
    if (!xmlDocGetRootElement(doc))
    {
        if (BuildXPathInFile(rawxpath, doc, a, pp))
        {
            (pp->edcontext->num_edits)++;
        }
    }
    //build XPath in a nonempty file
    else if (BuildXPathInNode(rawxpath, doc, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
    return true;
}

/***************************************************************************/

static void VerifyTreeDeletions(Attributes a, Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTreeDeletions(a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised tree deletion:\n\"%s\"\nis inconsistent", pp->promiser);
        return;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(a, pp))
    {
        return;
    }

    if ((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to load XML document");
        return;
    }

    if (!XmlSelectNode(a.xml.select_xpath, doc, &docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
            " !! The promised XPath pattern: \"%s\", was NOT successful when selecting an edit node, in XML document(%s)",
             a.xml.select_xpath, pp->this_server);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "deletetree-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (DeleteTreeInNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyTreeInsertions(Attributes a, Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTreeInsertions(a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised tree insertion:\n\"%s\"\nbreaks its own promises", pp->promiser);
        return;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(a, pp))
    {
        return;
    }

    if ((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to load XML document");
        return;
    }

    //if file is not empty: select an edit node, for tree insertion
    if (a.xml.haveselectxpath && !XmlSelectNode(a.xml.select_xpath, doc, &docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
            " !! The promised XPath pattern: \"%s\", was NOT successful when selecting an edit node, in XML document(%s)",
             a.xml.select_xpath, pp->this_server);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "inserttree-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    //insert tree into empty file or selected node
    if (!a.xml.haveselectxpath)
    {
        if (InsertTreeInFile(pp->promiser, doc, docnode, a, pp))
        {
            (pp->edcontext->num_edits)++;
        }
    }
    else if (InsertTreeInNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyAttributeDeletions(Attributes a, Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckAttributeDeletions(a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised attribute deletion: \"%s\", is inconsistent", pp->promiser);
        return;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(a, pp))
    {
        return;
    }

    if ((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to load XML document");
        return;
    }

    if (!XmlSelectNode(a.xml.select_xpath, doc, &docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
            " !! The promised XPath pattern: \"%s\", was NOT successful when selecting an edit node, in XML document(%s)",
             a.xml.select_xpath, pp->this_server);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "deleteattribute-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (DeleteAttributeInNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyAttributeSet(Attributes a, Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckAttributeSet(a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised attribute set: \"%s\", breaks its own promises", pp->promiser);
        return;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(a, pp))
    {
        return;
    }

    if ((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to load XML document");
        return;
    }

    if (!XmlSelectNode(a.xml.select_xpath, doc, &docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
            " !! The promised XPath pattern: \"%s\", was NOT successful when selecting an edit node, in XML document(%s)",
             a.xml.select_xpath, pp->this_server);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "setattribute-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (SetAttributeInNode(pp->promiser, a.xml.attribute_value, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyTextDeletions(Attributes a, Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTextDeletions(a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised text deletion:\n\"%s\"\nis inconsistent", pp->promiser);
        return;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(a, pp))
    {
        return;
    }

    if ((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " !! Unable to load XML document");
        return;
    }

    if (!XmlSelectNode(a.xml.select_xpath, doc, &docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
            " !! The promised XPath pattern: \"%s\", was NOT successful when selecting an edit node, in XML document(%s)",
             a.xml.select_xpath, pp->this_server);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "deletetext-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (DeleteTextInNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyTextSet(Attributes a, Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTextSet(a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised text set:\n\"%s\"\nbreaks its own promises", pp->promiser);
        return;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(a, pp))
    {
        return;
    }

    if ((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " !! Unable to load XML document");
        return;
    }

    if (!XmlSelectNode(a.xml.select_xpath, doc, &docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
            " !! The promised XPath pattern: \"%s\", was NOT successful when selecting an edit node, in XML document(%s)",
             a.xml.select_xpath, pp->this_server);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "settext-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (SetTextInNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyTextInsertions(Attributes a, Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTextInsertions(a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised text insertion:\n\"%s\"\nbreaks its own promises", pp->promiser);
        return;
    }

    if (a.xml.havebuildxpath && !VerifyXPathBuild(a, pp))
    {
        return;
    }

    if ((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " !! Unable to load XML document");
        return;
    }

    if (!XmlSelectNode(a.xml.select_xpath, doc, &docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
            " !! The promised XPath pattern: \"%s\", was NOT successful when selecting an edit node, in XML document(%s)",
             a.xml.select_xpath, pp->this_server);
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "inserttext-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (InsertTextInNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

/*

This should provide pointers to the edit node within the XML document.
It returns true if a match was identified, else false.

If no such node matches, docnode should point to NULL

*/
static bool XmlSelectNode(char *rawxpath, xmlDocPtr doc, xmlNodePtr *docnode, Attributes a, Promise *pp)
{
    xmlNodePtr cur = NULL;
    xmlXPathContextPtr xpathCtx = NULL;
    xmlXPathObjectPtr xpathObj = NULL;
    xmlNodeSetPtr nodes = NULL;
    const xmlChar* xpathExpr = NULL;
    int i, size = 0;
    bool valid = true;

    *docnode = NULL;

    if (!XPathVerifyConvergence(rawxpath, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! select_xpath expression: \"%s\", is NOT convergent", rawxpath);
        return false;
    }

    if ((xpathExpr = CharToXmlChar(rawxpath)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to create new XPath expression: \"%s\"", rawxpath);
        return false;
    }

    if ((xpathCtx = xmlXPathNewContext(doc)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to create new XPath context: \"%s\"", rawxpath);
        return false;
    }

    if ((xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to evaluate XPath expression \"%s\"", xpathExpr);
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
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Current select_xpath expression: \"%s\", returns (%d) edit nodes in XML document(%s), please modify expression to select a unique edit node",
             xpathExpr, size, pp->this_server);
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
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a,
                 " !! The promised XPath pattern: \"%s\", was NOT found when selecting an edit node, in XML document(%s)",
                 xpathExpr, pp->this_server);
            valid = false;
        }
    }

    *docnode = cur;

    xmlXPathFreeContext(xpathCtx);
    xmlXPathFreeObject(xpathObj);

    return valid;
}

/***************************************************************************/

static bool BuildXPathInFile(char rawxpath[CF_BUFSIZE], xmlDocPtr doc, Attributes a, Promise *pp)
{
    xmlNodePtr docnode = NULL, head = NULL;
    char copyxpath[CF_BUFSIZE] = { 0 };

    strcpy(copyxpath, rawxpath);

    if (xmlDocGetRootElement(doc))
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " !! The promised XML document (%s) already exists and contains a root element (promise kept)",
             pp->this_server);
        return false;
    }

    //set rootnode
    if ((docnode = XPathHeadExtractNode(copyxpath, a, pp)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! Unable to extract root node from XPath (%s), to be inserted into an empty XML document (%s)",
             rawxpath, pp->this_server);
        return false;
    }

    if (docnode == NULL || (docnode->name) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! The extracted root node, from XPath (%s), to be inserted into an empty XML document (%s), is empty",
             rawxpath, pp->this_server);
        return false;
    }

    //insert the content into new XML document, beginning from root node
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Building XPath: \"%s\", into an empty XML document (%s)",
         rawxpath, pp->this_server);
    if (xmlDocSetRootElement(doc, docnode) != NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised XPath \"%s\", was NOT built successfully into an empty XML document (%s)",
             rawxpath, pp->this_server);
        return false;
    }

    XPathRemoveHead(copyxpath, a, pp);

    //extract and insert nodes from tail
    while ((strlen(copyxpath) > 0) && ((head = XPathHeadExtractNode(copyxpath, a, pp)) != NULL))
    {
        xmlAddChild(docnode, head);
        docnode = head;
        XPathRemoveHead(copyxpath, a, pp);
    }

    return true;
}

/***************************************************************************/

static bool BuildXPathInNode(char rawxpath[CF_BUFSIZE], xmlDocPtr doc, Attributes a, Promise *pp)
{
    xmlNodePtr docnode = NULL,  head = NULL, tail = NULL;
    char copyxpath[CF_BUFSIZE] = { 0 };

    strcpy(copyxpath, rawxpath);

    //build XPath from tail while locating insertion node
    while ((strlen(copyxpath) > 0) && (!XmlSelectNode(copyxpath, doc, &docnode, a, pp)))
    {
        if (XPathHasTail (copyxpath, a, pp))
        {
            head = XPathTailExtractNode(copyxpath, a, pp);
            XPathRemoveTail(copyxpath, a, pp);
        }
        else
        {
            head = XPathHeadExtractNode(copyxpath, a, pp);
            XPathRemoveHead(copyxpath, a, pp);
        }

        if (head && tail)
        {
            xmlAddChild(head, tail);
        }
        tail = head;
    }

    //insert the new tree into selected node in XML document
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Building XPath: \"%s\", in XML document (%s)",
         rawxpath, pp->this_server);
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

static bool InsertTreeInFile(char *rawtree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr treenode = NULL, rootnode = NULL;
    xmlChar *buf = NULL;

    //for parsing subtree from memory
    if ((buf = CharToXmlChar(rawtree)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted:\n\"%s\"\ninto an empty XML document (%s), was NOT successfully loaded into an XML buffer",
             rawtree, pp->this_server);
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted:\n\"%s\"\ninto an empty XML document (%s), was NOT parsed successfully",
             rawtree, pp->this_server);
        return false;
    }

    if (treenode == NULL || (treenode->name) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised tree to be inserted:\n\"%s\"\ninto an empty XML document (%s), is empty",
             rawtree, pp->this_server);
        return false;
    }

    //verify treenode does not already exist inside docnode
    if ((rootnode = xmlDocGetRootElement(doc)) != NULL)
    {
        if (!XmlNodesCompare(treenode, rootnode, a, pp))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
                 " !! The promised tree:\n\"%s\"\nis to be inserted into an empty XML document (%s),"
                 " however XML document is NOT empty and tree to be inserted does NOT match existing content."
                 " If you would like to insert into a non-empty XML document, please specify select_xpath expression",
                 rawtree, pp->this_server);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " !! The promised XML document (%s) already exists and contains a root element (promise kept)",
                 pp->this_server);
        }

        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
             " -> Need to insert the promised tree:\n\"%s\"\ninto an empty XML document (%s) - but only a warning was promised",
             rawtree, pp->this_server);
        return true;
    }

    //insert the content into new XML document
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, "\n -> Inserting tree:\n\"%s\"\ninto an empty XML document (%s)",
         rawtree, pp->this_server);
    if (xmlDocSetRootElement(doc, treenode) != NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised tree:\n\"%s\"\nwas NOT inserted successfully, into an empty XML document (%s)",
             rawtree, pp->this_server);
        return false;
    }

    //verify node was inserted
    if (((rootnode = xmlDocGetRootElement(doc)) == NULL) || !XmlNodesCompare(treenode, rootnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised tree:\n\"%s\"\nwas NOT inserted successfully, into an empty XML document (%s)",
             rawtree, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool DeleteTreeInNode(char *rawtree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr treenode = NULL;
    xmlNodePtr deletetree = NULL;
    xmlChar *buf = NULL;

    //for parsing subtree from memory
    if ((buf = CharToXmlChar(rawtree)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Tree to be deleted:\n\"%s\"\nat XPath (%s) in XML document (%s), was NOT successfully loaded into an XML buffer",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Tree to be deleted:\n\"%s\"\nat XPath (%s) in XML document (%s), was NOT parsed successfully",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify treenode exists inside docnode
    if ((deletetree = XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " !! The promised tree to be deleted:\n\"%s\"\ndoes NOT exist, at XPath (%s) in XML document (%s) (promise kept)",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
             " -> Need to delete the promised tree:\n\"%s\"\nat XPath (%s) in XML document (%s) - but only a warning was promised",
             rawtree, a.xml.select_xpath, pp->this_server);
        return true;
    }

    //remove the subtree from XML document
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Deleting tree:\n\"%s\"\nat XPath (%s) in XML document (%s)",
         rawtree, a.xml.select_xpath, pp->this_server);
    xmlUnlinkNode(deletetree);
    xmlFreeNode(deletetree);

    //verify treenode no longer exists inside docnode
    if (XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised tree to be deleted:\n\"%s\"\nwas NOT successfully deleted, at XPath (%s) in XML document (%s)",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool InsertTreeInNode(char *rawtree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr treenode = NULL;
    xmlChar *buf = NULL;

    //for parsing subtree from memory
    if ((buf = CharToXmlChar(rawtree)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted:\n\"%s\"\nat XPath (%s) in XML document (%s), was NOT successfully loaded into an XML buffer",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted:\n\"%s\"\nat XPath (%s) in XML document (%s), was NOT parsed successfully",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if (treenode == NULL || (treenode->name) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a, " !! The promised tree to be inserted:\n\"%s\"\nat XPath (%s) in XML document (%s), is empty",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify treenode does not already exist inside docnode
    if (XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " !! The promised tree to be inserted:\n\"%s\"\nalready exists, at XPath (%s) in XML document (%s) (promise kept)",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
             " -> Need to insert the promised tree:\n\"%s\"\nat XPath (%s) in XML document (%s) - but only a warning was promised",
             rawtree, a.xml.select_xpath, pp->this_server);
        return true;
    }

    //insert the subtree into XML document
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Inserting tree:\n\"%s\"\nat XPath (%s) in XML document (%s)\n",
         rawtree, a.xml.select_xpath, pp->this_server);
    if (!xmlAddChild(docnode, treenode))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised tree:\n\"%s\"\nwas NOT inserted successfully, at XPath (%s) in XML document (%s)",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify node was inserted
    if (!XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised tree:\n\"%s\"\nwas NOT inserted successfully, at XPath (%s) in XML document (%s)",
             rawtree, a.xml.select_xpath, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool DeleteAttributeInNode(char *rawname, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlAttrPtr attr = NULL;
    xmlChar *name = NULL;

    if ((name = CharToXmlChar(rawname)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Name of attribute to be deleted: \"%s\", at XPath (%s) in XML document (%s), was NOT successfully loaded into an XML buffer",
             rawname, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify attribute exists inside docnode
    if ((attr = xmlHasProp(docnode, name)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a,
             " !! The promised attribute to be deleted: \"%s\", does NOT exist, at XPath (%s) in XML document (%s) (promise kept)",
             rawname, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
             " -> Need to delete the promised attribute: \"%s\", at XPath (%s) in XML document (%s) - but only a warning was promised",
             rawname, a.xml.select_xpath, pp->this_server);
        return true;
    }

    //delete attribute from docnode
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Deleting attribute: \"%s\", at XPath (%s) in XML document (%s)",
             rawname, a.xml.select_xpath, pp->this_server);
    if ((xmlRemoveProp(attr)) == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised attribute to be deleted: \"%s\", was NOT deleted successfully, at XPath (%s) in XML document (%s).",
             rawname, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify attribute no longer exists inside docnode
    if ((attr = xmlHasProp(docnode, name)) != NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised attribute to be deleted: \"%s\", was NOT deleted successfully, at XPath (%s) in XML document (%s)",
             rawname, a.xml.select_xpath, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SetAttributeInNode(char *rawname, char *rawvalue, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlAttrPtr attr = NULL;
    xmlChar *name = NULL;
    xmlChar *value = NULL;

    if ((name = CharToXmlChar(rawname)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Name of attribute to be set: \"%s\", at XPath (%s) in XML document (%s), was NOT successfully loaded into an XML buffer",
             rawname, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if ((value = CharToXmlChar(rawvalue)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Value of attribute to be set: \"%s\", at XPath (%s) in XML document (%s), was NOT successfully loaded into an XML buffer",
             rawvalue, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify attribute does not already exist inside docnode
    if ((attr = XmlVerifyAttributeInNode(name, value, docnode, a, pp)) != NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a,
             " !! The promised attribute to be set, with name: \"%s\" and value: \"%s\", already exists, at XPath (%s) in XML document (%s) (promise kept)",
             rawname, rawvalue, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
             " -> Need to set the promised attribute, with name: \"%s\" and value: \"%s\", at XPath (%s) in XML document (%s) - but only a warning was promised",
             rawname, rawvalue, a.xml.select_xpath, pp->this_server);
        return true;
    }

    //set attribute in docnode
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Setting attribute with name: \"%s\" and value: \"%s\", at XPath (%s) in XML document (%s)",
         rawname, rawvalue, a.xml.select_xpath, pp->this_server);
    if ((attr = xmlSetProp(docnode, name, value)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised attribute to be set, with name: \"%s\" and value: \"%s\", was NOT successfully set, at XPath (%s) in XML document (%s)",
             rawname, rawvalue, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify attribute was inserted
    if ((attr = XmlVerifyAttributeInNode(name, value, docnode, a, pp)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised attribute to be set, with name: \"%s\" and value: \"%s\", was NOT successfully set, at XPath (%s) in XML document (%s)",
             rawname, rawvalue, a.xml.select_xpath, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool DeleteTextInNode(char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr elemnode, copynode;
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Text to be deleted:\n\"%s\"\nat XPath (%s) in XML document (%s), was NOT successfully loaded into an XML buffer",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify text exists inside docnode
    if (XmlVerifyTextInNodeSubstring(text, docnode, a, pp) == NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a,
             " !! The promised text to be deleted:\n\"%s\"\ndoes NOT exist, at XPath (%s) in XML document (%s) (promise kept)",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
             " -> Need to delete the promised text:\n\"%s\"\nat XPath (%s) in XML document (%s) - but only a warning was promised",
             rawtext, a.xml.select_xpath, pp->this_server);
        return true;
    }

    //delete text from docnode
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Deleting text:\n\"%s\"\nat XPath (%s) in XML document (%s)",
         rawtext, a.xml.select_xpath, pp->this_server);

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
    if (XmlVerifyTextInNodeSubstring(text, docnode, a, pp) != NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised text:\n\"%s\"\nwas NOT deleted successfully, at XPath (%s) in XML document (%s)",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SetTextInNode(char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr elemnode, copynode;
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Text to be set:\n\"%s\"\nat XPath (%s) in XML document (%s), was NOT successfully loaded into an XML buffer",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify text does not exist inside docnode
    if (XmlVerifyTextInNodeExact(text, docnode, a, pp) != NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a,
             " !! The promised text to be set:\n\"%s\"\nalready exists, at XPath (%s) in XML document (%s) (promise kept)",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
             " -> Need to set the promised text:\n\"%s\"\nat XPath (%s) in XML document (%s) - but only a warning was promised",
             rawtext, a.xml.select_xpath, pp->this_server);
        return true;
    }

    //set text in docnode
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Setting text:\n\"%s\"\nat XPath (%s) in XML document (%s)",
         rawtext, a.xml.select_xpath, pp->this_server);

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
    if (XmlVerifyTextInNodeExact(text, docnode, a, pp) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised text:\n\"%s\"\nwas NOT set successfully, at XPath (%s) in XML document (%s)",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool InsertTextInNode(char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr elemnode, copynode;
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Text to be inserted:\n\"%s\"\nat XPath (%s) in XML document (%s), was NOT successfully loaded into an XML buffer",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    //verify text does not exist inside docnode
    if (XmlVerifyTextInNodeSubstring(text, docnode, a, pp) != NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a,
             " !! The promised text to be inserted:\n\"%s\"\nalready exists, at XPath (%s) in XML document (%s) (promise kept)",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
             " -> Need to insert the promised text:\n\"%s\"\nat XPath (%s) in XML document (%s) - but only a warning was promised",
             rawtext, a.xml.select_xpath, pp->this_server);
        return true;
    }

    //insert text into docnode
    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Inserting text:\n\"%s\"\nat XPath (%s) in XML document (%s)",
         rawtext, a.xml.select_xpath, pp->this_server);

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
    if (XmlVerifyTextInNodeSubstring(text, docnode, a, pp) == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! The promised text:\n\"%s\"\nwas NOT inserted successfully, at XPath (%s) in XML document (%s)",
             rawtext, a.xml.select_xpath, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckXPathBuild(Attributes a, Promise *pp)
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

    if ((strcmp("build_xpath", pp->agentsubtype) == 0) && (a.xml.havebuildxpath))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Attribute: build_xpath is not allowed within bundle: build_xpath");
        return false;
    }

    if (a.xml.haveselectxpath && !a.xml.havebuildxpath)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! XPath build does not require select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyBuildSyntax(rawxpath, a, pp))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTreeDeletions(Attributes a, Promise *pp)
{
    if (!a.xml.haveselectxpath)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Tree deletion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath, a, pp))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTreeInsertions(Attributes a, Promise *pp)
{
    if ((a.xml.haveselectxpath && !a.xml.havebuildxpath && !xmlDocGetRootElement(pp->edcontext->xmldoc)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Tree insertion into an empty file, using select_xpath, does not make sense");
        return false;
    }
    else if ((!a.xml.haveselectxpath &&  a.xml.havebuildxpath))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              "!! Tree insertion requires select_xpath to be specified, unless inserting into an empty file");
        return false;
    }

    if (a.xml.haveselectxpath && !XPathVerifyConvergence(a.xml.select_xpath, a, pp))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckAttributeDeletions(Attributes a, Promise *pp)
{
    if (!(a.xml.haveselectxpath))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Attribute deletion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath, a, pp))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckAttributeSet(Attributes a, Promise *pp)
{
    if (!(a.xml.haveselectxpath))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Attribute insertion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath, a, pp))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTextDeletions(Attributes a, Promise *pp)
{
    if (!(a.xml.haveselectxpath))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Tree insertion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath, a, pp))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTextSet(Attributes a, Promise *pp)
{
    if (!(a.xml.haveselectxpath))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Tree insertion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath, a, pp))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTextInsertions(Attributes a, Promise *pp)
{
    if (!(a.xml.haveselectxpath))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! Tree insertion requires select_xpath to be specified");
        return false;
    }

    if (!XPathVerifyConvergence(a.xml.select_xpath, a, pp))
    {
        return false;
    }

    return true;
}

/***************************************************************************/

int XmlCompareToFile(xmlDocPtr doc, char *file, Attributes a, Promise *pp)
/* returns true if XML on disk is identical to XML in memory */
{
    struct stat statbuf;
    xmlDocPtr cmpdoc = NULL;

    CfDebug("XmlCompareToFile(%s)\n", file);

    if (cfstat(file, &statbuf) == -1)
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

    if (!LoadFileAsXmlDoc(&cmpdoc, file, a, pp))
    {
        return false;
    }

    if (!XmlDocsEqualMem(cmpdoc, doc, (a.transaction.action == cfa_warn), a, pp))
    {
        xmlFreeDoc(cmpdoc);
        return false;
    }

    xmlFreeDoc(cmpdoc);

    return true;
}

/*********************************************************************/

static bool XmlDocsEqualMem(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp)
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

static bool XmlNodesCompare(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node1 contain all content(tag/attributes/text/nodes) found in node2? */
{
    xmlNodePtr copynode1, copynode2;
    int compare = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    if (!XmlNodesCompareTags(node1, node2, a, pp))
    {
        compare = false;
    }

    if (compare && !XmlNodesCompareAttributes(node1, node2, a, pp))
    {
        compare = false;
    }

    if (compare && !XmlNodesCompareText(node1, node2, a, pp))
    {
        compare = false;
    }

    if (compare && !XmlNodesCompareNodes(node1, node2, a, pp))
    {
        compare = false;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return compare;
}

/*********************************************************************/

static bool XmlNodesCompareAttributes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node1 contain same attributes found in node2? */
{
    xmlNodePtr copynode1, copynode2;
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

    count1 = XmlAttributeCount(node1, a, pp);
    count2 = XmlAttributeCount(node2, a, pp);

    if (count1 != count2)
    {
        return false;
    }

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    //get attribute list from node1 and node2
    attr1 = copynode1->properties;
    attr2 = copynode2->properties;

    //check that each attribute in node1 is in node2
    while (attr1)
    {
        value = xmlNodeGetContent(attr1->children);

        if ((XmlVerifyAttributeInNode(attr1->name, value, copynode2, a, pp)) == NULL)
        {
            compare = false;
            break;
        }

        attr1 = attr1->next;
        attr2 = attr2->next;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return compare;
}

/*********************************************************************/

static bool XmlNodesCompareNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node1 contain same nodes found in node2? */
{
    xmlNodePtr copynode1, copynode2;
    xmlNodePtr child1 = NULL;
    int count1, count2, compare = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    count1 = xmlChildElementCount(node1);
    count2 = xmlChildElementCount(node2);

    if (count1 != count2)
    {
        return false;
    }

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    //get node list from node1 and node2
    child1 = xmlFirstElementChild(copynode1);

    while (child1)
    {
        if (XmlVerifyNodeInNodeExact(child1, copynode2, a, pp) == NULL)
        {
            compare = false;
            break;
        }
        child1 = xmlNextElementSibling(copynode1);
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return compare;
}

/*********************************************************************/

static bool XmlNodesCompareTags(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node1 contain same tag found in node2? */
{
    xmlNodePtr copynode1, copynode2;
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

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    //check tag in node1 is the same as tag in node2
    if (!xmlStrEqual(copynode1->name, copynode2->name))
    {
        compare = false;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);
    return compare;
}

/*********************************************************************/

static bool XmlNodesCompareText(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
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

static bool XmlNodesSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node1 contain matching subset of content(tag/attributes/text/nodes) found in node2? */
{
    xmlNodePtr copynode1, copynode2;
    int subset = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node2)
    {
        return false;
    }

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    if (!XmlNodesCompareTags(node1, node2, a, pp))
    {
        subset = false;
    }

    if (subset && !XmlNodesSubsetOfAttributes(node1, node2, a, pp))
    {
        subset = false;
    }

    if (subset && !XmlNodesSubstringOfText(node1, node2, a, pp))
    {
        subset = false;
    }

    if (subset && !XmlNodesSubsetOfNodes(node1, node2, a, pp))
    {
        subset = false;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return subset;
}

/*********************************************************************/

static bool XmlNodesSubsetOfAttributes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node1 contain matching subset of attributes found in node2? */
{
    xmlNodePtr copynode1, copynode2;
    xmlAttrPtr attr1 = NULL;
    xmlChar *value = NULL;
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

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    //get attribute list from node1
    attr1 = copynode1->properties;

    //check that each attribute in node1 is in node2
    while (attr1)
    {
        value = xmlNodeGetContent(attr1->children);

        if ((XmlVerifyAttributeInNode(attr1->name, value, copynode2, a, pp)) == NULL)
        {
            subset = false;
            break;
        }

        attr1 = attr1->next;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return subset;
}

/*********************************************************************/

static bool XmlNodesSubsetOfNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node1 contain matching subset of nodes found in node2? */
{
    xmlNodePtr copynode1, copynode2;
    xmlNodePtr child1 = NULL;
    int subset = true;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    //get node list from node1 and node2
    child1 = xmlFirstElementChild(copynode1);

    while (child1)
    {
        if (XmlVerifyNodeInNodeExact(child1, copynode2, a, pp) == NULL)
        {
            subset = false;
            break;
        }
        child1 = xmlNextElementSibling(copynode1);
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return subset;
}

/*********************************************************************/

static bool XmlNodesSubstringOfText(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node1 contain matching substring of text found in node2? */
{
    xmlChar *text1, *text2;

    if (!node1 && !node2)
    {
        return true;
    }

    if (!node1 || !node2)
    {
        return false;
    }

    text1 = xmlNodeGetContent(node1->children);
    text2 = xmlNodeGetContent(node2->children);

    if (!text1)
    {
        return true;
    }

    if (!text2)
    {
        return false;
    }

    if (!xmlStrstr(text2, text1))
    {
        return false;
    }

    return true;
}

/*********************************************************************/

xmlAttrPtr XmlVerifyAttributeInNode(const xmlChar *name, xmlChar *value, xmlNodePtr node, Attributes a, Promise *pp)
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
        return NULL;
    }

    return attr2;
}

/*********************************************************************/

xmlChar* XmlVerifyTextInNodeExact(const xmlChar *text, xmlNodePtr node, Attributes a, Promise *pp)
/* Does node contain: text content exactly matching the givin string text?
   Returns a pointer to text content in node or NULL */
{
    xmlChar *text2;

    if (node == NULL)
    {
        return NULL;
    }

    text2 = xmlNodeGetContent(node->children);

    if (!xmlStrEqual(text2, text))
    {
        return NULL;
    }

    return text2;
}

/*********************************************************************/

xmlChar* XmlVerifyTextInNodeSubstring(const xmlChar *text, xmlNodePtr node, Attributes a, Promise *pp)
/* Does node contain: text content, contains substring, matching the given string of text?
   Returns a pointer to text content in node or NULL */
{
    xmlChar *text2 = NULL;

    if (node == NULL)
    {
        return NULL;
    }

    text2 = xmlNodeGetContent(node->children);

    if (!xmlStrstr(text2, text))
    {
        return NULL;
    }

    return text2;
}

/*********************************************************************/

xmlNodePtr XmlVerifyNodeInNodeExact(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node2 contain a node with content matching all content in node1?
   Returns a pointer to node found in node2 or NULL */
{
    xmlNodePtr comparenode = NULL;

    if ((node1 == NULL) || (node2 == NULL))
    {
        return NULL;
    }

    if ((comparenode = xmlFirstElementChild(node2)) == NULL)
    {
        return NULL;
    }

    while (comparenode)
    {
        if (XmlNodesCompare(node1, comparenode, a, pp))
        {

            return comparenode;
        }
        comparenode = xmlNextElementSibling(comparenode);
    }

    return NULL;
}

/*********************************************************************/

xmlNodePtr XmlVerifyNodeInNodeSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node2 contain: node with subset of content matching all content in node1?
   Returns a pointer to node found in node2 or NULL */
{
    xmlNodePtr comparenode = NULL;

    if ((node1 == NULL) || (node2 == NULL))
    {
        return comparenode;
    }

    if ((comparenode = xmlFirstElementChild(node2)) == NULL)
    {
        return comparenode;
    }

    while (comparenode)
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

xmlNodePtr PredicateExtractNode(char predicate[CF_BUFSIZE], Attributes a, Promise *pp)
{
    xmlNodePtr node = NULL;
    xmlChar *name = NULL;
    xmlChar *value = NULL;
    char rawname[CF_BUFSIZE] = { 0 }, rawvalue[CF_BUFSIZE] = { 0 }, *tok, *running;

    running = xstrdup(predicate);

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

    return node;
}

/*********************************************************************/

static bool PredicateRemoveHead(char predicate[CF_BUFSIZE], Attributes a, Promise *pp)
{
    char copypred[CF_BUFSIZE] = { 0 }, *tail = NULL;

    strcpy(copypred, predicate);
    memset(predicate, 0, sizeof(char)*CF_BUFSIZE);

    if (PredicateHasTail(copypred, a, pp))
    {
        tail =  strchr(copypred+1, '|');
        strcpy(predicate, tail);
    }

    return true;
}

/*********************************************************************/

xmlNodePtr XPathHeadExtractNode(char xpath[CF_BUFSIZE], Attributes a, Promise *pp)
{
    xmlNodePtr node = NULL;
    char head[CF_BUFSIZE] = {0}, *tok = NULL, *running;

    running = xstrdup(xpath);

    //extract head substring from xpath
    tok = strsep(&running, "/");
    while (strcmp(tok, "") == 0)
    {
        tok = strsep(&running, "/");
    }
    strcpy(head, tok);

    if ((node = XPathSegmentExtractNode(head, a, pp)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, "   !! Could not extract node: %s", head);
    }

    return node;
}

/*********************************************************************/

xmlNodePtr XPathTailExtractNode(char xpath[CF_BUFSIZE], Attributes a, Promise *pp)
{
    xmlNodePtr node = NULL;
    char copyxpath[CF_BUFSIZE] = {0}, tail[CF_BUFSIZE] = {0}, *tok = NULL;

    strcpy(copyxpath, xpath);

    //extract tail substring from xpath
    tok =  XPathGetTail(copyxpath, a, pp);
    strcpy(tail, tok);

    if ((node = XPathSegmentExtractNode(tail, a, pp)) == NULL)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, "   !! Could not extract node: %s", tail);
    }

    return node;
}

/*********************************************************************/

xmlNodePtr XPathSegmentExtractNode(char segment[CF_BUFSIZE], Attributes a, Promise *pp)
{
    xmlNodePtr node = NULL, prednode = NULL;
    xmlChar *name = NULL, *attrname = NULL, *attrvalue = NULL;
    char predicate[CF_BUFSIZE] = { 0 }, rawname[CF_BUFSIZE] = { 0 }, rawvalue[CF_BUFSIZE] = { 0 }, *tok, *running;
    int hasname = false;

    running = xstrdup(segment);

    //extract name and predicate substrings from segment
    if (XPathHeadContainsNode(segment, a, pp))
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
    if (hasname && XPathHeadContainsPredicate(segment, a, pp))
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
            if (PredicateHeadContainsNode(predicate, a, pp))
            {
                //create a new node within node
                prednode = PredicateExtractNode(predicate, a, pp);
                xmlAddChild(node, prednode);
            }
            else if (PredicateHeadContainsAttribute(predicate, a, pp))
            {
                running = xstrdup(predicate);

                //extract attribute name
                tok = strsep(&running, "| @\"\'=");
                while (strcmp(tok, "") == 0)
                {
                    tok = strsep(&running, "| @\"\'=");
                }
                strcpy(rawname, tok);
                attrname = CharToXmlChar(rawname);

                //extract attribute value
                tok = strsep(&running, "| @\"\'=");
                while (strcmp(tok, "") == 0)
                {
                    tok = strsep(&running, "| @\"\'=");
                }
                strcpy(rawvalue, tok);
                attrvalue = CharToXmlChar(rawvalue);

                //create a new attribute within node
                xmlNewProp(node, attrname, attrvalue);
            }

            if (PredicateHasTail(predicate, a, pp))
            {
                PredicateRemoveHead(predicate, a, pp);
            }
            else
            {
                memset(predicate, 0, sizeof(char)*CF_BUFSIZE);
            }
        }
    }
    return node;
}

/*********************************************************************/

char* XPathGetTail(char xpath[CF_BUFSIZE], Attributes a, Promise *pp)
{
    char tmpstr[CF_BUFSIZE] = {0}, *tok = NULL, *running;

    running = xstrdup(xpath);
    memset(xpath, 0, sizeof(char)*CF_BUFSIZE);

    if (XPathHasTail(running, a, pp))
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

    return xpath;
}

/*********************************************************************/

static bool XPathRemoveHead(char xpath[CF_BUFSIZE], Attributes a, Promise *pp)
{
    char copyxpath[CF_BUFSIZE] = { 0 }, *tail = NULL;

    strcpy(copyxpath, xpath);
    memset(xpath, 0, sizeof(char)*CF_BUFSIZE);

    if (XPathHasTail(copyxpath, a, pp))
    {
        tail =  strchr(copyxpath+1, '/');
        strcpy(xpath, tail);
    }

    return true;
}

/*********************************************************************/

static bool XPathRemoveTail(char xpath[CF_BUFSIZE], Attributes a, Promise *pp)
{
    char copyxpath[CF_BUFSIZE] = { 0 }, *tail = NULL;
    int len = 0;

    strcpy(copyxpath, xpath);
    memset(xpath, 0, sizeof(char)*CF_BUFSIZE);

    if (XPathHasTail(copyxpath, a, pp))
    {
        tail = strrchr(copyxpath, '/');
        len = tail-copyxpath;
        copyxpath[len] = '\0';
        strcpy(xpath, copyxpath);
    }

    return true;
}

/*********************************************************************/

static bool PredicateHasTail(char *predicate, Attributes a, Promise *pp)
{
    const char *regexp = "^\\s*\\[?\\s*@?\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?\\s*\\|";

    return (ContainsRegex(predicate, regexp));
}

/*********************************************************************/

static bool PredicateHeadContainsAttribute(char *predicate, Attributes a, Promise *pp)
{
    const char *regexp = "^\\s*\\[?\\|?(\\s*@\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?)(\\s*(\\||\\]))?"; // i.e. @name='value' or @name = value or @name ="value"

    return (ContainsRegex(predicate, regexp));
}

/*********************************************************************/

static bool PredicateHeadContainsNode(char *predicate, Attributes a, Promise *pp)
{
    const char *regexp = "^\\s*\\[?\\|?(\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?)(\\s*(\\||\\]))?";  // i.e. name='value' or name = value or name ="value"

    return (ContainsRegex(predicate, regexp));
}

/*********************************************************************/

static bool XPathHasTail(char *head, Attributes a, Promise *pp)
{
    const char *regexp = "^\\s*\\/?\\s*(\\w|-|\\.)+(\\s*::\\s*(\\w|-|\\.)+\\s*)*\\s*(\\[[^\\[\\]\\/]*\\])?\\s*\\/";

    return (ContainsRegex(head, regexp));
}

/*********************************************************************/

static bool XPathHeadContainsNode(char *head, Attributes a, Promise *pp)
{
    const char *regexp = "^(\\/)?(\\w|-|\\.)+((\\s*::\\s*)?(\\w|-|\\.)+)*";

    return (ContainsRegex(head, regexp));
}

/*********************************************************************/

static bool XPathHeadContainsPredicate(char *head, Attributes a, Promise *pp)
{
    const char *regexp = "^\\s*\\/?\\s*(\\w|-|\\.)+(\\s*::\\s*(\\w|-|\\.)+)*\\s*\\"       // name
        // [ name='value' | @name = "value" | name = value]
        "[\\s*@?\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?\\s*(\\s*\\|\\s*)?(\\s*@?\\s*(\\w|-|\\.)+\\s*=\\s*(\"|\')?(\\w|-|\\.)+(\"|\')?\\s*)*\\]";

    return (ContainsRegex(head, regexp));
}

/*********************************************************************/

static bool XPathVerifyBuildSyntax(const char* xpath, Attributes a, Promise *pp)
/*verify that XPath does not specify position wrt sibling-axis (such as):[#] [last()] [position()] following-sibling:: preceding-sibling:: */
{
    char regexp[CF_BUFSIZE] = {'\0'};

    //check for convergence
    if (!XPathVerifyConvergence(xpath, a, pp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Promiser expression (%s) is not convergent", xpath);
        return false;
    }

    // /name[ name = value | @name='value'| . . . | @name = "value" ]/. . .
    strcpy (regexp, "^(\\/(( |\\t)*(\\w|-|\\.)+( |\\t)*)"
    "(\\[( |\\t)*@?(\\w|-|\\.)+( |\\t)*=( |\\t)*(\'|\")?(\\w|-|\\.)+(\'|\")?( |\\t)*"
    "(\\|( |\\t)*@?(\\w|-|\\.)+( |\\t)*=( |\\t)*(\'|\")?(\\w|-|\\.)+(\'|\")?( |\\t)*)*\\])?)*$");

    if (!ContainsRegex(xpath, regexp))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, a,
             " !! Promiser expression (%s), \ncontains syntax that is not supported for xpath_build. "
             "Please refer to users manual for supported syntax specifications.", xpath);
        return false;
    }
    return true;
}

/*********************************************************************/

static bool XPathVerifyConvergence(const char* xpath, Attributes a, Promise *pp)
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

static int XmlAttributeCount(xmlNodePtr node, Attributes a, Promise *pp)
{
    xmlNodePtr copynode;
    xmlAttrPtr attr;
    int count = 0;

    if (!node)
    {
        return count;
    }

    copynode = xmlCopyNode(node, 1);

    attr = copynode->properties;

    while (attr)
    {
        count++;
        attr = attr->next;
    }

    xmlFree(copynode);

    return count;
}

/*********************************************************************/

#endif
