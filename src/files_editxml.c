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
/* File: files_editxml.c                                                     */
/*                                                                           */
/* Created: Thu Aug  9 10:16:53 2012                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"

#include "env_context.h"
#include "constraints.h"
#include "promises.h"
#include "files_names.h"
#include "vars.h"
#include "item_lib.h"
#include "sort.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"

#ifdef HAVE_PCRE_H
# include <pcre.h>
#endif

#ifdef HAVE_PCRE_PCRE_H
# include <pcre/pcre.h>
#endif

/*****************************************************************************/

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
static void VerifyTreeDeletions(Promise *pp);
static void VerifyTreeInsertions(Promise *pp);
static void VerifyAttributeDeletions(Promise *pp);
static void VerifyAttributeSet(Promise *pp);
static void VerifyTextDeletions(Promise *pp);
static void VerifyTextSet(Promise *pp);
static void VerifyTextInsertions(Promise *pp);
static bool XmlSelectNode(xmlDocPtr doc, xmlNodePtr *node, Attributes a, Promise *pp);
static bool DeleteTreeInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool InsertTreeInFile(char *root, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool InsertTreeInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool DeleteAttributeInNode(char *attrname, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool SetAttributeInNode(char *attrname, char *attrvalue, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool DeleteTextInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool SetTextInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool InsertTextInNode(char *tree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static bool SanityCheckTreeDeletions(Attributes a, Promise *pp);
static bool SanityCheckTreeInsertions(Attributes a, Promise *pp);
static bool SanityCheckAttributeDeletions(Attributes a, Promise *pp);
static bool SanityCheckAttributeSet(Attributes a);
static bool SanityCheckTextDeletions(Attributes a, Promise *pp);
static bool SanityCheckTextSet(Attributes a);
static bool SanityCheckTextInsertions(Attributes a);

static bool XmlDocsEqualContent(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp);
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
xmlAttrPtr XmlVerifyAttributeInNode(const xmlChar *attrname, xmlChar *attrvalue, xmlNodePtr node, Attributes a, Promise *pp);
xmlChar* XmlVerifyTextInNodeExact(const xmlChar *text, xmlNodePtr node, Attributes a, Promise *pp);
xmlChar* XmlVerifyTextInNodeSubstring(const xmlChar *text, xmlNodePtr node, Attributes a, Promise *pp);
xmlNodePtr XmlVerifyNodeInNodeExact(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
xmlNodePtr XmlVerifyNodeInNodeSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);


xmlChar *CharToXmlChar(char* c);
static int XmlAttributeCount(xmlNodePtr node, Attributes a, Promise *pp);
static bool XmlXPathConvergent(const char* xpath, Attributes a, Promise *pp);

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ScheduleEditXmlOperations(char *filename, Bundle *bp, Attributes a, Promise *parentp,
                               const ReportContext *report_context)
{
    enum editxmltypesequence type;
    SubType *sp;
    Promise *pp;
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
    NewScalar("edit", "filename", filename, cf_str);

/* Reset the done state for every call here, since bundle is reusable */

    for (type = 0; EDITXMLTYPESEQUENCE[type] != NULL; type++)
    {
        if ((sp = GetSubTypeForBundle(EDITXMLTYPESEQUENCE[type], bp)) == NULL)
        {
            continue;
        }

        for (pp = sp->promiselist; pp != NULL; pp = pp->next)
        {
            pp->donep = false;
        }
    }

    for (pass = 1; pass < CF_DONEPASSES; pass++)
    {
        for (type = 0; EDITXMLTYPESEQUENCE[type] != NULL; type++)
        {
            EditXmlClassBanner(type);

            if ((sp = GetSubTypeForBundle(EDITXMLTYPESEQUENCE[type], bp)) == NULL)
            {
                continue;
            }

            BannerSubSubType(bp->name, sp->name);
            THIS_BUNDLE = bp->name;
            SetScope(bp->name);

            for (pp = sp->promiselist; pp != NULL; pp = pp->next)
            {
                pp->edcontext = parentp->edcontext;
                pp->this_server = filename;
                pp->donep = &(pp->done);

                ExpandPromise(cf_agent, bp->name, pp, KeepEditXmlPromise, report_context);

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

    CfOut(cf_verbose, "", "     ??  Private class context\n");

    AlphaListIterator i = AlphaListIteratorInit(&VADDCLASSES);

    for (const Item *ip = AlphaListIteratorNext(&i); ip != NULL; ip = AlphaListIteratorNext(&i))
    {
        CfOut(cf_verbose, "", "     ??       %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "\n");
}

/***************************************************************************/

static void KeepEditXmlPromise(Promise *pp)
{
    char *sp = NULL;

    if (!IsDefinedClass(pp->classes))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
        CfOut(cf_verbose, "", "   Skipping whole next edit promise, as context %s is not relevant\n", pp->classes);
        CfOut(cf_verbose, "", "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
        return;
    }

    if (pp->done)
    {
//   return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(cf_verbose, "", "Skipping whole next edit promise (%s), as var-context %s is not relevant\n",
              pp->promiser, sp);
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    PromiseBanner(pp);

    if (strcmp("classes", pp->agentsubtype) == 0)
    {
        KeepClassContextPromise(pp);
        return;
    }

    if (strcmp("delete_tree", pp->agentsubtype) == 0)
    {
        xmlInitParser();
        VerifyTreeDeletions(pp);
        xmlCleanupParser();
        return;
    }

    if (strcmp("insert_tree", pp->agentsubtype) == 0)
    {
        xmlInitParser();
        VerifyTreeInsertions(pp);
        xmlCleanupParser();
        return;
    }

    if (strcmp("delete_attribute", pp->agentsubtype) == 0)
    {
        xmlInitParser();
        VerifyAttributeDeletions(pp);
        xmlCleanupParser();
        return;
    }

    if (strcmp("set_attribute", pp->agentsubtype) == 0)
    {
        xmlInitParser();
        VerifyAttributeSet(pp);
        xmlCleanupParser();
        return;
    }

    if (strcmp("delete_text", pp->agentsubtype) == 0)
    {
        xmlInitParser();
        VerifyTextDeletions(pp);
        xmlCleanupParser();
        return;
    }

    if (strcmp("set_text", pp->agentsubtype) == 0)
    {
        xmlInitParser();
        VerifyTextSet(pp);
        xmlCleanupParser();
        return;
    }

    if (strcmp("insert_text", pp->agentsubtype) == 0)
    {
        xmlInitParser();
        VerifyTextInsertions(pp);
        xmlCleanupParser();
        return;
    }

    if (strcmp("reports", pp->agentsubtype) == 0)
    {
        VerifyReportPromise(pp);
        return;
    }
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static void VerifyTreeDeletions(Promise *pp)
{
    xmlDocPtr doc;
    xmlNodePtr docnode;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetDeletionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTreeDeletions(a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised tree deletion (%s) is inconsistent", pp->promiser);
        return;
    }

    if((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! Unable to load xml document");
        return;
    }

    if (!XmlSelectNode(doc, &docnode, a, pp))
    {
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

static void VerifyTreeInsertions(Promise *pp)
{
    xmlDocPtr doc;
    xmlNodePtr docnode;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetInsertionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTreeInsertions(a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised tree insertion (%s) breaks its own promises",
             pp->promiser);
        return;
    }

    if((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Unable to load xml document");
        return;
    }

    //if file is not empty: select an edit node, for tree insertion
    if (a.xml.haveselectxpathregion && !XmlSelectNode(doc, &docnode, a, pp))
    {
        return;
    }

    snprintf(lockname, CF_BUFSIZE - 1, "inserttree-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    //insert tree into empty file or selected node
    if (!a.xml.haveselectxpathregion && !xmlDocGetRootElement(doc))
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

static void VerifyAttributeDeletions(Promise *pp)
{
    xmlDocPtr doc = pp->edcontext->xmldoc;
    xmlNodePtr docnode;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetDeletionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckAttributeDeletions(a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised attribute deletion (%s) is inconsistent", pp->promiser);
        return;
    }

    if(doc == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Unable to load xml document");
        return;
    }

    if (!XmlSelectNode(doc, &docnode, a, pp))
    {
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

static void VerifyAttributeSet(Promise *pp)
{
    xmlDocPtr doc = pp->edcontext->xmldoc;
    xmlNodePtr docnode;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetInsertionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckAttributeSet(a))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised attribute set (%s) breaks its own promises",
             pp->promiser);
        return;
    }

    if(doc == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Unable to load xml document");
        return;
    }

    if (!XmlSelectNode(doc, &docnode, a, pp))
    {
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

static void VerifyTextDeletions(Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetDeletionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTextDeletions(a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised text deletion (%s) is inconsistent", pp->promiser);
        return;
    }

    if((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Unable to load xml document");
        return;
    }

    if (!XmlSelectNode(doc, &docnode, a, pp))
    {
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

static void VerifyTextSet(Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetInsertionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTextSet(a))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised text set (%s) breaks its own promises",
             pp->promiser);
        return;
    }

    if((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Unable to load xml document");
        return;
    }

    if (!XmlSelectNode(doc, &docnode, a, pp))
    {
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

static void VerifyTextInsertions(Promise *pp)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr docnode = NULL;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetInsertionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTextInsertions(a))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised text insertion (%s) breaks its own promises",
             pp->promiser);
        return;
    }

    if((doc = pp->edcontext->xmldoc) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Unable to load xml document");
        return;
    }

    if (!XmlSelectNode(doc, &docnode, a, pp))
    {
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
/* Level                                                                   */
/***************************************************************************/

static bool XmlSelectNode(xmlDocPtr doc, xmlNodePtr *docnode, Attributes a, Promise *pp)
/*

This should provide pointers to the edit node within the xml document.
It returns true if a match was identified, else false.

If no such node matches, docnode should point to NULL

*/
{
    xmlNodePtr cur = NULL;
    xmlXPathContextPtr xpathCtx = NULL;
    xmlXPathObjectPtr xpathObj = NULL;
    xmlNodeSetPtr nodes = NULL;
    const xmlChar* xpathExpr = NULL;
    int i, size = 0, select = true;

    *docnode = NULL;

    if (!XmlXPathConvergent(a.xml.select_xpath_region, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! select_xpath_region expression (%s) is not convergent", a.xml.select_xpath_region);
        return false;
    }

    if ((xpathExpr = CharToXmlChar(a.xml.select_xpath_region)) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! Unable to create new XPath expression");
        return false;
    }

    if((xpathCtx = xmlXPathNewContext(doc)) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! Unable to create new XPath context");
        return false;
    }

    if((xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx)) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! Unable to evaluate xpath expression \"%s\"", xpathExpr);
        xmlXPathFreeContext(xpathCtx); 
        return false;
    }

    if ((size = (nodes = xpathObj->nodesetval) ? nodes->nodeNr : 0) == 0)
    {
        select = false;
    }

    if (size > 1)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! Current select_xpath_region expression \"%s\" returns (%d) edit nodes, please modify to select a unique edit node", xpathExpr, size);
        select = false;
    }

    //select first matching node
    if (select)
    {
        for(i = 0; i < size; ++i)
        {
            if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
            {
                cur = nodes->nodeTab[i];
                break;
            }
        }
        if(cur == NULL)
        {
            cfPS(cf_verbose, CF_INTERPT, "", pp, a,
                 " !! The promised xpath pattern (%s) was not found when selecting edit node in %s",
                 xpathExpr, pp->this_server);
            select = false;
        }
    }

    *docnode = cur;

    xmlXPathFreeContext(xpathCtx);
    xmlXPathFreeObject(xpathObj);

    return select;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static bool InsertTreeInFile(char *rawtree, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr treenode = NULL, rootnode = NULL;
    xmlChar *buf = NULL;

    //for parsing subtree from memory
    if ((buf = CharToXmlChar(rawtree)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted was not parsed successfully");
        return false;
    }

    if (treenode == NULL || (treenode->name) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! The promised tree to be inserted is empty");
        return false;
    }

    //verify treenode does not already exist inside docnode
    if ((rootnode = xmlDocGetRootElement(doc)) != NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised xmldoc (%s) already exists and contains a root element %s", pp->promiser,
             pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to create the promised xmldoc \"%s\" in %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }

    //insert the content into new xml document
    CfOut(cf_inform, "", " -> Inserting tree \"%s\" in %s", pp->promiser,
          pp->this_server);
    if (xmlDocSetRootElement(doc, treenode) != NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree (%s) was not inserted successfully in %s", pp->promiser,
             pp->this_server);
        return false;
    }

    //verify node was inserted
    if (((rootnode = xmlDocGetRootElement(doc)) == NULL) || !XmlNodesCompare(treenode, rootnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree (%s) was not inserted successfully in %s", pp->promiser,
             pp->this_server);
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
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Tree to be deleted was not successfully loaded into an xml buffer");
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Tree to be deleted was not parsed successfully");
        return false;
    }

    //verify treenode exists inside docnode
    if ((deletetree = XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp)) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree to be deleted(%s) does not exists in %s", pp->promiser,
             pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to delete the promised tree \"%s\" from %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }

    //remove the subtree from xml document
    CfOut(cf_inform, "", " -> Deleting tree \"%s\" in %s", pp->promiser,
          pp->this_server);
    xmlUnlinkNode(deletetree);
    xmlFreeNode(deletetree);

    //verify treenode no longer exists inside docnode
    if (XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree to be deleted(%s) was not successfully deleted, in %s", pp->promiser,
             pp->this_server);
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
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode) != 0)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted was not parsed successfully");
        return false;
    }

    if (treenode == NULL || (treenode->name) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! The promised tree to be inserted is empty");
        return false;
    }

    //verify treenode does not already exist inside docnode
    if (XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree (%s) already exists in %s", pp->promiser,
             pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to insert the promised tree \"%s\" to %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }

    //insert the subtree into xml document
    CfOut(cf_inform, "", " -> Inserting tree \"%s\" in %s", pp->promiser,
          pp->this_server);
    if (!xmlAddChild(docnode, treenode))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree (%s) was not inserted successfully in %s", pp->promiser,
             pp->this_server);
        return false;
    }

    //verify node was inserted
    if (!XmlVerifyNodeInNodeSubset(treenode, docnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree (%s) was not inserted successfully in %s", pp->promiser,
             pp->this_server);
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
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Attribute name to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //verify attribute exists inside docnode
    if ((attr = xmlHasProp(docnode, name)) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised attribute to be deleted (%s) was not found in edit node in %s", pp->promiser,
             pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to delete the promised attribute \"%s\" from %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }

    //delete attribute from docnode
    CfOut(cf_inform, "", " -> Deleting attribute \"%s\" in %s", pp->promiser,
          pp->this_server);
    if ((xmlRemoveProp(attr)) == -1)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised attribute to be deleted was not deleted successfully.");
        return false;
    }

    //verify attribute no longer exists inside docnode
    if ((attr = xmlHasProp(docnode, name)) != NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised attribute to be deleted (%s) was not deleted from edit node in %s", pp->promiser,
             pp->this_server);
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
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Attribute name to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    if ((value = CharToXmlChar(rawvalue)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Attribute value to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //verify attribute does not already exist inside docnode
    if ((attr = XmlVerifyAttributeInNode(name, value, docnode, a, pp)) != NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised attribute (%s) with value (%s) already exists in %s", pp->promiser, a.xml.attribute_value,
             pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to insert the promised attribute \"%s\" to %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }

    //insert attribute into docnode
    CfOut(cf_inform, "", " -> Inserting attribute \"%s\" in %s", pp->promiser,
          pp->this_server);
    if ((attr = xmlNewProp(docnode, name, value)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Attribute was not successfully inserted into xml document");
        return false;
    }

    //verify attribute was inserted
    if ((attr = XmlVerifyAttributeInNode(name, value, docnode, a, pp)) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised attribute (%s) with value (%s) was not inserted in %s", pp->promiser, a.xml.attribute_value,
             pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool DeleteTextInNode(char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Attribute name to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //verify text exists inside docnode
    if (XmlVerifyTextInNodeSubstring(text, docnode, a, pp) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised text to be deleted (%s) does not exist in %s", pp->promiser, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to insert the promised text \"%s\" to %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }

    //delete text from docnode
    CfOut(cf_inform, "", " -> Deleting text \"%s\" in %s", pp->promiser,
          pp->this_server);

    xmlNodeSetContent(docnode, "");

    //verify text no longer exists inside docnode
    if (XmlVerifyTextInNodeSubstring(text, docnode, a, pp) != NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised text (%s) was not deleted successfully from %s", pp->promiser, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SetTextInNode(char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Attribute name to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //verify text does not exist inside docnode
    if (XmlVerifyTextInNodeExact(text, docnode, a, pp) != NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised text (%s) already exists in %s", pp->promiser, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to set the promised text \"%s\" to %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }

    //set text in docnode
    CfOut(cf_inform, "", " -> Setting text \"%s\" in %s", pp->promiser,
          pp->this_server);

    xmlNodeSetContent(docnode, text);

    //verify text was inserted
    if (XmlVerifyTextInNodeExact(text, docnode, a, pp) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised text (%s) was not inserted successfully in %s", pp->promiser, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static bool InsertTextInNode(char *rawtext, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlChar *text = NULL;

    if ((text = CharToXmlChar(rawtext)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Attribute name to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //verify text does not exist inside docnode
    if (XmlVerifyTextInNodeSubstring(text, docnode, a, pp) != NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised text (%s) already exists in %s", pp->promiser, pp->this_server);
        return false;
    }

    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to insert the promised text \"%s\" to %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }

    //insert text into docnode
    CfOut(cf_inform, "", " -> Inserting text \"%s\" in %s", pp->promiser,
          pp->this_server);

    xmlNodeAddContent(docnode, text);

    //verify text was inserted
    if (XmlVerifyTextInNodeSubstring(text, docnode, a, pp) == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised text (%s) was not inserted successfully in %s", pp->promiser, pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static bool SanityCheckTreeDeletions(Attributes a, Promise *pp)
{
    if(!a.xml.haveselectxpathregion)
    {
        CfOut(cf_error, "",
              " !! Tree deletion requires select_xpath_region to be specified");
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckTreeInsertions(Attributes a, Promise *pp)
{
    if((a.xml.haveselectxpathregion && !xmlDocGetRootElement(pp->edcontext->xmldoc)))
    {
        CfOut(cf_error, "",
              " !! Tree insertion into an empty file, using select_xpath_region, does not make sense");
        return false;
    }

    else if((!a.xml.haveselectxpathregion &&  xmlDocGetRootElement(pp->edcontext->xmldoc)))
    {
        CfOut(cf_error, "Tree insertion requires select_xpath_region to be specified, unless inserting into an empty file",
              " !! ");
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckAttributeDeletions(Attributes a, Promise *pp)
{
    if(!(a.xml.haveselectxpathregion))
    {
        CfOut(cf_error, "",
              " !! Attribute deletion requires select_xpath_region to be specified");
        return false;
    }

    return true;
}

/***************************************************************************/

static bool SanityCheckAttributeSet(Attributes a)
{
    if(!(a.xml.haveselectxpathregion))
    {
        CfOut(cf_error, "",
              " !! Attribute insertion requires select_xpath_region to be specified");
        return false;
    }
    return true;
}

/***************************************************************************/

static bool SanityCheckTextDeletions(Attributes a, Promise *pp)
{
    if(!(a.xml.haveselectxpathregion))
    {
        CfOut(cf_error, "",
              " !! Tree insertion requires select_xpath_region to be specified");
        return false;
    }
    return true;
}

/***************************************************************************/

static bool SanityCheckTextSet(Attributes a)
{
    if(!(a.xml.haveselectxpathregion))
    {
        CfOut(cf_error, "",
              " !! Tree insertion requires select_xpath_region to be specified");
        return false;
    }
    return true;
}

/***************************************************************************/

static bool SanityCheckTextInsertions(Attributes a)
{
    if(!(a.xml.haveselectxpathregion))
    {
        CfOut(cf_error, "",
              " !! Tree insertion requires select_xpath_region to be specified");
        return false;
    }
    return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int XmlCompareToFile(xmlDocPtr doc, char *file, Attributes a, Promise *pp)
/* returns true if xml on disk is identical to xml in memory */
{
    struct stat statbuf;
    xmlDocPtr cmpdoc = NULL;

    CfDebug("XmlCompareToFile(%s)\n", file);

    if(cfstat(file, &statbuf) == -1)
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

static bool XmlDocsEqualContent(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp)
{
    xmlNodePtr root1 = NULL;
    xmlNodePtr root2 = NULL;
    xmlNodePtr copynode1 = NULL;
    xmlNodePtr copynode2 = NULL;
    int equal = true;

    if (!doc1 && !doc2)
    {
        return true;
    }

    if (!doc1 || !doc2)
    {
        return false;
    }

    root1 = xmlDocGetRootElement(doc1);
    root2 = xmlDocGetRootElement(doc2);

    copynode1 = xmlCopyNode(root1, 1);
    copynode2 = xmlCopyNode(root2, 1);

    if (!XmlNodesCompare(copynode1, copynode2, a, pp))
    {
        equal = false;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return equal;
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

    if(!node1 && !node2)
    {
        return true;
    }

    if(!node1 || !node2)
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

    if(!node1 && !node2)
    {
        return true;
    }

    if(!node1 || !node2)
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

    if(!node1 && !node2)
    {
        return true;
    }

    if(!node1 || !node2)
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

    if(!node1 && !node2)
    {
        return true;
    }

    if(!node1 || !node2)
    {
        return false;
    }

    //get text from nodes
    text1 = xmlNodeGetContent(node1->children);
    text2 = xmlNodeGetContent(node2->children);

    if(!text1 && !text2)
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

    if(!node1 && !node2)
    {
        return true;
    }

    if(!node1 || !node2)
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

    if(!node1 && !node2)
    {
        return true;
    }

    if(!node1 || !node2)
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

    if(!node1 && !node2)
    {
        return true;
    }

    if(!node1 || !node2)
    {
        return false;
    }

    text1 = xmlNodeGetContent(node1->children);
    text2 = xmlNodeGetContent(node2->children);

    if(!text1)
    {
        return true;
    }

    if(!text2)
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

    while(comparenode)
    {
        if(XmlNodesCompare(node1, comparenode, a, pp))
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

    while(comparenode)
    {
        if(XmlNodesSubset(node1, comparenode, a, pp))
        {

            return comparenode;
        }
        comparenode = xmlNextElementSibling(comparenode);
    }

    return NULL;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

xmlChar *CharToXmlChar(char* c)
{
    return BAD_CAST c;
}

/*********************************************************************/

static int XmlAttributeCount(xmlNodePtr node, Attributes a, Promise *pp)
{
    xmlNodePtr copynode;
    xmlAttrPtr attr;
    int count = 0;

    if(!node)
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

static bool XmlXPathConvergent(const char* xpath, Attributes a, Promise *pp)
/*verify that xpath does not contain position specific (such as):[#] [last()] [position()] following-sibling:: preceding-sibling:: */
{
    const char *regexp = "\\[\\s*([^\\[\\]]*\\s*(\\||(or)|(and)))?\\s*"     // [ (stuff) (|/or/and)
        // | position() (=/!=/</<=/>/>=)
        "((position)\\s*\\(\\s*\\)\\s*((=)|(!=)|(<)|(<=)|(>)|(>=))\\s*)?\\s*"
        // (number) | (number) (+/-/*/div/mod) (number) | last() | last() (+/-/*/div/mod) (number)
        "(((\\d+)\\s*|((last)\\s*\\(\\s*\\)\\s*))(((\\+)|(-)|(\\*)|(div)|(mod))\\s*(\\d+)\\s*)*)\\s*"
        // (|/or/and) (stuff) ]
        "((\\||(or)|(and))[^\\[\\]]*)?\\]"
        // following:: preceding:: following-sibling:: preceding-sibling::
        "|((following)|(preceding))(-sibling)?\\s*(::)";

    int ovector[OVECCOUNT], rc;
    const char *errorstr;
    int erroffset;

    pcre *rx = pcre_compile(regexp, 0, &errorstr, &erroffset, NULL);

    if ((rc = pcre_exec(rx, NULL, xpath, strlen(xpath), 0, 0, ovector, OVECCOUNT)) >= 0)
    {
        free(rx);
        return false;
    }

    free(rx);
    return true;
}
