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
    "insert_attribute",
    //"replace_patterns",
    "reports",
    NULL
};

static void EditXmlClassBanner(enum editxmltypesequence type);
static void KeepEditXmlPromise(Promise *pp);
static void VerifyTreeDeletions(Promise *pp);
static void VerifyTreeInsertions(Promise *pp);
static void VerifyAttributeDeletions(Promise *pp);
static void VerifyAttributeInsertions(Promise *pp);
static int XmlSelectAttribute(xmlNodePtr docnode, xmlAttrPtr *attr, Attributes a, Promise *pp);
static int XmlSelectNode(xmlDocPtr doc, xmlNodePtr *node, Attributes a, Promise *pp);
static int DeleteTreeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static int InsertTreeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static int DeleteAttributeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static int InsertAttributeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static int SanityCheckTreeDeletions(Attributes a, Promise *pp);
static int SanityCheckTreeInsertions(Attributes a);
static int SanityCheckAttributeDeletions(Attributes a, Promise *pp);
static int SanityCheckAttributeInsertions(Attributes a);

static int XmlDocsEqualContent(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp);
static int XmlDocsEqualMem(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp);
static int XmlNodesCompare(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static int XmlNodesCompareAttributes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static int XmlNodesCompareNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static int XmlNodesCompareTags(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static int XmlNodesCompareText(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static int XmlNodesSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static int XmlNodesSubsetOfAttributes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
static int XmlNodesSubsetOfNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
xmlAttrPtr XmlVerifyAttributeInNode(const xmlChar *name, xmlChar *value, xmlNodePtr node, Attributes a, Promise *pp);
xmlNodePtr XmlVerifyNodeInNodeExact(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);
xmlNodePtr XmlVerifyNodeInNodeSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp);

xmlChar *CharToXmlChar(char* c);
static int XmlAttributeCount(xmlNodePtr node, Attributes a, Promise *pp);

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

    if (strcmp("insert_attribute", pp->agentsubtype) == 0)
    {
        xmlInitParser();
        VerifyAttributeInsertions(pp);
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

    if (DeleteTreeAtNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyTreeInsertions(Promise *pp)
{
    xmlDocPtr doc = pp->edcontext->xmldoc;
    xmlNodePtr docnode;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetInsertionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckTreeInsertions(a))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised tree insertion (%s) breaks its own promises",
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

    snprintf(lockname, CF_BUFSIZE - 1, "inserttree-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (InsertTreeAtNode(pp->promiser, doc, docnode, a, pp))
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

    if (DeleteAttributeAtNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/

static void VerifyAttributeInsertions(Promise *pp)
{
    xmlDocPtr doc = pp->edcontext->xmldoc;
    xmlNodePtr docnode;

    Attributes a = { {0} };
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    a = GetInsertionAttributes(pp);
    a.transaction.ifelapsed = CF_EDIT_IFELAPSED;

    if (!SanityCheckAttributeInsertions(a))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a, " !! The promised attribute insertion (%s) breaks its own promises",
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

    snprintf(lockname, CF_BUFSIZE - 1, "insertattribute-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (InsertAttributeAtNode(pp->promiser, doc, docnode, a, pp))
    {
        (pp->edcontext->num_edits)++;
    }

    YieldCurrentLock(thislock);
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static int XmlSelectAttribute(xmlNodePtr docnode, xmlAttrPtr *attr, Attributes a, Promise *pp)
/*

This should provide a pointer to the edit attribute within the xml document node.
It returns true if a match was identified, else false.

If no such node matches, attr should point to NULL

*/
{
    xmlAttrPtr cur = NULL;
    const xmlChar* attrname;

    if ((attrname = CharToXmlChar(pp->promiser)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Error: unable to create new XPath expression from select_xpath_region");
        *attr = cur;
        return false;
    }

    if ((cur = xmlHasProp(docnode, attrname)) == NULL)
    {
        *attr = NULL;
        return false;
    }

    *attr = cur;

    return true;
}

/***************************************************************************/

static int XmlSelectNode(xmlDocPtr doc, xmlNodePtr *docnode, Attributes a, Promise *pp)
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
    int i, size;

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

    if ((nodes = xpathObj->nodesetval) == NULL)
    {
        xmlXPathFreeContext(xpathCtx);
        xmlXPathFreeObject(xpathObj);
        return false;
    }

    if ((size = (nodes) ? nodes->nodeNr : 0) == 0)
    {
        xmlXPathFreeContext(xpathCtx);
        xmlXPathFreeObject(xpathObj);
        return false;
    }

    if (size > 1)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! Current select_xpath_region expression \"%s\" returns (%d) edit nodes, please modify to select a unique edit node", xpathExpr, size);
        xmlXPathFreeContext(xpathCtx);
        xmlXPathFreeObject(xpathObj);
        return false;
    }

    //select first matching node
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
        xmlXPathFreeContext(xpathCtx);
        xmlXPathFreeObject(xpathObj);
        return false;
    }

    *docnode = cur;

    xmlXPathFreeContext(xpathCtx);
    xmlXPathFreeObject(xpathObj);

    return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static int DeleteTreeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr treenode = NULL;
    xmlNodePtr deletetree = NULL;
    xmlChar *buf = NULL;

    //for parsing subtree from memory
    if ((buf = CharToXmlChar(chunk)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Tree to be deleted was not successfully loaded into an xml buffer");
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode))
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Tree to be deleted was not parsed successfully");
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

    //remove the subtree from xml document
    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to delete the promised tree \"%s\" from %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }
    else
    {
        CfOut(cf_inform, "", " -> Deleting tree \"%s\" in %s", pp->promiser,
              pp->this_server);
        xmlUnlinkNode(deletetree);
        xmlFreeNode(deletetree);
    }

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

static int InsertTreeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlNodePtr treenode = NULL;
    xmlChar *buf = NULL;

    //for parsing subtree from memory
    if ((buf = CharToXmlChar(chunk)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //parse the subtree
    if (xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, buf, &treenode))
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Tree to be inserted was not parsed successfully");
        return false;
    }

    //verify treenode does not already exist inside docnode
    if (XmlVerifyNodeInNodeExact(treenode, docnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree (%s) already exists in %s", pp->promiser,
             pp->this_server);
        return false;
    }

    //insert the subtree into xml document
    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to insert the promised tree \"%s\" to %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }
    else
    {
        CfOut(cf_inform, "", " -> Inserting tree \"%s\" in %s", pp->promiser,
              pp->this_server);
        if (!xmlAddChild(docnode, treenode))
        {
            cfPS(cf_error, CF_INTERPT, "", pp, a,
                 " !! The promised tree (%s) was not inserted successfully in %s", pp->promiser,
                 pp->this_server);
            return false;
        }
    }

    //verify node was inserted
    if (!XmlVerifyNodeInNodeExact(treenode, docnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree (%s) was not inserted successfully in %s", pp->promiser,
             pp->this_server);
        return false;
    }

    return true;
}

/***************************************************************************/

static int DeleteAttributeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlAttrPtr attr = NULL;
    xmlChar *name = NULL;

    if ((name = CharToXmlChar(pp->promiser)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Attribute name to be inserted was not successfully loaded into an xml buffer");
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

    //delete attribute from docnode
    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to delete the promised attribute \"%s\" from %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }
    else
    {
        CfOut(cf_inform, "", " -> Deleting attribute \"%s\" in %s", pp->promiser,
              pp->this_server);
        if ((xmlRemoveProp(attr)) == -1)
        {
            cfPS(cf_error, CF_INTERPT, "", pp, a,
                 " !! The promised attribute to be deleted was not deleted successfully.");
            return false;
        }
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

static int InsertAttributeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlAttrPtr attr = NULL;
    xmlChar *name = NULL;
    xmlChar *value = NULL;

    if ((name = CharToXmlChar(pp->promiser)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Attribute name to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    if ((value = CharToXmlChar(a.xml.attribute_value)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Attribute value to be inserted was not successfully loaded into an xml buffer");
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

    //insert a new attribute into docnode
    if (a.transaction.action == cfa_warn)
    {
        cfPS(cf_error, CF_WARN, "", pp, a,
             " -> Need to insert the promised attribute \"%s\" to %s - but only a warning was promised",
             pp->promiser, pp->this_server);
        return true;
    }
    else
    {
        CfOut(cf_inform, "", " -> Inserting attribute \"%s\" in %s", pp->promiser,
              pp->this_server);
        if ((attr = xmlNewProp(docnode, name, value)) == NULL)
        {
            cfPS(cf_verbose, CF_INTERPT, "", pp, a,
                 " !! WARNING: Attribute was not successfully inserted into xml document");
            return false;
        }
    }

    //verify attribute now exists inside docnode
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
/* Level                                                                   */
/***************************************************************************/

static int SanityCheckTreeDeletions(Attributes a, Promise *pp)
{
    long ok = true;

    if(!a.xml.haveselectxpathregion)
    {
        CfOut(cf_error, "",
              " !! Tree deletion requires select_xpath_region to be specified");
        ok = false;
    }

    return ok;
}

/***************************************************************************/

static int SanityCheckTreeInsertions(Attributes a)
{
    long ok = true;

    if(!(a.xml.haveselectxpathregion))
    {
        CfOut(cf_error, "",
              " !! Tree insertion requires select_xpath_region to be specified");
        ok = false;
    }

    return ok;
}

/***************************************************************************/

static int SanityCheckAttributeDeletions(Attributes a, Promise *pp)
{
    long ok = true;

    if(!(a.xml.haveselectxpathregion))
    {
        CfOut(cf_error, "",
              " !! Attribute deletion requires select_xpath_region to be specified");
        ok = false;
    }

    return ok;
}

/***************************************************************************/

static int SanityCheckAttributeInsertions(Attributes a)
{
    long ok = true;

    if(!(a.xml.haveselectxpathregion))
    {
        CfOut(cf_error, "",
              " !! Attribute insertion requires select_xpath_region to be specified");
        ok = false;
    }

    return ok;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

int LoadFileAsXmlDoc(xmlDocPtr *doc, const char *file, Attributes a, Promise *pp)
{
    struct stat statbuf;

    if (cfstat(file, &statbuf) == -1)
    {
        CfOut(cf_verbose, "stat", " ** Information: the proposed file \"%s\" could not be loaded", file);
        return false;
    }

    if (a.edits.maxfilesize != 0 && statbuf.st_size > a.edits.maxfilesize)
    {
        CfOut(cf_inform, "", " !! File %s is bigger than the limit edit.max_file_size = %jd > %d bytes\n", file,
              (intmax_t) statbuf.st_size, a.edits.maxfilesize);
        return (false);
    }

    if (!S_ISREG(statbuf.st_mode))
    {
        cfPS(cf_inform, CF_INTERPT, "", pp, a, "%s is not a plain file\n", file);
        return false;
    }

    if ((*doc = xmlParseFile(file)) == NULL)
    {
        cfPS(cf_inform, CF_INTERPT, "xmlParseFile", pp, a, "Document %s not parsed successfully\n", file);
        return false;
    }

    return (true);
}

/*********************************************************************/

int SaveXmlDocAsFile(xmlDocPtr doc, const char *file, Attributes a, Promise *pp,
                       const ReportContext *report_context)
{
    struct stat statbuf;
    char new[CF_BUFSIZE], backup[CF_BUFSIZE];
    mode_t mask;
    char stamp[CF_BUFSIZE];
    time_t stamp_now;

#ifdef WITH_SELINUX
    int selinux_enabled = 0;
    security_context_t scontext = NULL;

    selinux_enabled = (is_selinux_enabled() > 0);

    if (selinux_enabled)
    {
        /* get current security context */
        getfilecon(file, &scontext);
    }
#endif

    stamp_now = time((time_t *) NULL);

    if (cfstat(file, &statbuf) == -1)
    {
        cfPS(cf_error, CF_FAIL, "stat", pp, a, " !! Can no longer access file %s, which needed editing!\n", file);
        return false;
    }

    strcpy(backup, file);

    if (a.edits.backup == cfa_timestamp)
    {
        snprintf(stamp, CF_BUFSIZE, "_%jd_%s", (intmax_t) CFSTARTTIME, CanonifyName(cf_ctime(&stamp_now)));
        strcat(backup, stamp);
    }

    strcat(backup, ".cf-before-edit");

    strcpy(new, file);
    strcat(new, ".cf-after-edit");
    unlink(new);                /* Just in case of races */

    // Save xml document to file and then free document
    if (xmlSaveFile(new, doc) == -1)
    {
        cfPS(cf_error, CF_FAIL, "xmlSaveFile", pp, a, "Failed to write xml document to file %s after editing\n", new);
        xmlFreeDoc(doc);
        return false;
    }
    xmlFreeDoc(doc);

    cfPS(cf_inform, CF_CHG, "", pp, a, " -> Edited file %s \n", file);

    if (cf_rename(file, backup) == -1)
    {
        cfPS(cf_error, CF_FAIL, "cf_rename", pp, a,
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", file, backup);
        return false;
    }

    if (a.edits.backup == cfa_rotate)
    {
        RotateFiles(backup, a.edits.rotate);
        unlink(backup);
    }

    if (a.edits.backup != cfa_nobackup)
    {
        if (ArchiveToRepository(backup, a, pp, report_context))
        {
            unlink(backup);
        }
    }
    else
    {
        unlink(backup);
    }

    if (cf_rename(new, file) == -1)
    {
        cfPS(cf_error, CF_FAIL, "cf_rename", pp, a,
             " !! Can't rename %s to %s - so promised edits could not be moved into place\n", new, file);
        return false;
    }

    mask = umask(0);
    cf_chmod(file, statbuf.st_mode);    /* Restore file permissions etc */
    chown(file, statbuf.st_uid, statbuf.st_gid);
    umask(mask);

#ifdef WITH_SELINUX
    if (selinux_enabled)
    {
        /* restore file context */
        setfilecon(file, scontext);
    }
#endif

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

    if (!XmlDocsEqualContent(cmpdoc, doc, (a.transaction.action == cfa_warn), a, pp))
    {
        xmlFreeDoc(cmpdoc);
        return false;
    }

    xmlFreeDoc(cmpdoc);
    xmlFreeDoc(doc);

    return (true);
}

/*********************************************************************/

static int XmlDocsEqualContent(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp)
{
    xmlNodePtr root1 = NULL;
    xmlNodePtr root2 = NULL;
    xmlNodePtr copynode1 = NULL;
    xmlNodePtr copynode2 = NULL;

    root1 = xmlDocGetRootElement(doc1);
    root2 = xmlDocGetRootElement(doc2);

    copynode1 = xmlCopyNode(root1, 1);
    copynode2 = xmlCopyNode(root2, 1);

    if (!XmlNodesCompare(copynode1, copynode2, a, pp))
    {
        xmlFree(copynode1);
        xmlFree(copynode2);
        return false;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return true;
}
/*********************************************************************/

static int XmlDocsEqualMem(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp)
{
    xmlChar *mem1;
    xmlChar *mem2;

    int memsize1;
    int memsize2;

    xmlDocDumpMemory(doc1, &mem1, &memsize1);
    xmlDocDumpMemory(doc2, &mem2, &memsize2);

    if (!xmlStrEqual(mem1, mem2))
    {
        xmlFree(mem1);
        xmlFree(mem2);
        return false;
    }

    xmlFree(mem1);
    xmlFree(mem2);

    return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static int XmlNodesCompare(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
{
    xmlNodePtr copynode1, copynode2;

    if ((node1 == NULL) && (node2 == NULL))
    {
        return true;
    }

    if ((node1 == NULL) || (node2 == NULL))
    {
        return false;
    }

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    if (!XmlNodesCompareTags(node1, node2, a, pp))
    {
        xmlFree(copynode1);
        xmlFree(copynode2);
        return false;
    }

    if (!XmlNodesCompareAttributes(node1, node2, a, pp))
    {
        xmlFree(copynode1);
        xmlFree(copynode2);
        return false;
    }

    //XmlNodesCompareText(node1, node2, a, pp);

    if (!XmlNodesCompareNodes(node1, node2, a, pp))
    {
        xmlFree(copynode1);
        xmlFree(copynode2);
        return false;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return true;
}

/*********************************************************************/

static int XmlNodesCompareAttributes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
{
    xmlNodePtr copynode1, copynode2;
    xmlAttrPtr attr1 = NULL;
    xmlAttrPtr attr2 = NULL;
    xmlChar *value = NULL;
    int count1, count2;

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
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! node1->properties or node2->properties are NULL");
        return false;
    }

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    count1 = XmlAttributeCount(copynode1, a, pp);
    count2 = XmlAttributeCount(copynode2, a, pp);

    if (count1 != count2)
    {
        return false;
    }

    //get attribute list from node1 and node2
    attr1 = copynode1->properties;
    attr2 = copynode2->properties;

    //check that each attribute in node1 is in node2
    while (attr1)
    {
        value = xmlNodeGetContent(attr1->children);

        if ((XmlVerifyAttributeInNode(attr1->name, value, copynode2, a, pp)) == NULL)
        {
            xmlFree(copynode1);
            xmlFree(copynode2);
            return false;
        }

        attr1 = attr1->next;
        attr2 = attr2->next;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return true;
}

/*********************************************************************/

static int XmlNodesCompareNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
{
    xmlNodePtr copynode1, copynode2;
    xmlNodePtr child1 = NULL;
    int count1, count2;

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

    count1 = xmlChildElementCount(copynode1);
    count2 = xmlChildElementCount(copynode2);

    if (count1 != count2)
    {
        return false;
    }

    //get node list from node1 and node2
    child1 = xmlFirstElementChild(copynode1);

    while (child1)
    {
        if (XmlVerifyNodeInNodeExact(child1, copynode2, a, pp) == NULL)
        {
            return false;
        }
        child1 = xmlNextElementSibling(copynode1);
    }

    return true;
}

/*********************************************************************/

static int XmlNodesCompareTags(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
{
    xmlNodePtr copynode1, copynode2;

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
        xmlFree(copynode1);
        xmlFree(copynode2);
        return false;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);
    return true;
}

/*********************************************************************/

static int XmlNodesCompareText(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
{
    xmlNodePtr copynode1, copynode2;

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

    //get text from nodes

    //check that text from node1 is in node2

    xmlFree(copynode1);
    xmlFree(copynode2);

    return true;
}

/*********************************************************************/

static int XmlNodesSubset(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
{
    xmlNodePtr copynode1, copynode2;

    if ((node1 == NULL) && (node2 == NULL))
    {
        return true;
    }

    if ((node2 == NULL))
    {
        return false;
    }

    copynode1 = xmlCopyNode(node1, 1);
    copynode2 = xmlCopyNode(node2, 1);

    if (!XmlNodesCompareTags(node1, node2, a, pp))
    {
        xmlFree(copynode1);
        xmlFree(copynode2);
        return false;
    }

    if (!XmlNodesSubsetOfAttributes(node1, node2, a, pp))
    {
        xmlFree(copynode1);
        xmlFree(copynode2);
        return false;
    }

    //XmlNodesCompareText(node1, node2, a, pp);

    if (!XmlNodesSubsetOfNodes(node1, node2, a, pp))
    {
        xmlFree(copynode1);
        xmlFree(copynode2);
        return false;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return true;
}

/*********************************************************************/

static int XmlNodesSubsetOfAttributes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
// Does node1 contain a subset of attributes found in node2?
{
    xmlNodePtr copynode1, copynode2;
    xmlAttrPtr attr1 = NULL;
    xmlChar *value = NULL;

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
            xmlFree(copynode1);
            xmlFree(copynode2);
            return false;
        }

        attr1 = attr1->next;
    }

    xmlFree(copynode1);
    xmlFree(copynode2);

    return true;
}

/*********************************************************************/

static int XmlNodesSubsetOfNodes(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
// Does node1 contain a subset of nodes found in node2?
{
    xmlNodePtr copynode1, copynode2;
    xmlNodePtr child1 = NULL;

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
            return false;
        }
        child1 = xmlNextElementSibling(copynode1);
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

    if ((name == NULL) || (node->properties == NULL))
    {
        return attr2;
    }

    //get attribute with matching name from node, if it exists
    if ((attr2 = xmlHasProp(node, name)) == NULL)
    {
        return attr2;
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

xmlNodePtr XmlVerifyNodeInNodeExact(xmlNodePtr node1, xmlNodePtr node2, Attributes a, Promise *pp)
/* Does node2 contain a node with content matching all content in node1?
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

    if ((attr = copynode->properties) == NULL)
    {
        return count;
    }

    while (attr)
    {
        count++;
        attr = attr->next;
    }

    xmlFree(copynode);

    return count;
}
