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
    //"replace_patterns",
    "reports",
    NULL
};

static void EditXmlClassBanner(enum editxmltypesequence type);
static void KeepEditXmlPromise(Promise *pp);
static void VerifyTreeInsertions(Promise *pp);
static void VerifyTreeDeletions(Promise *pp);
static void VerifyAttributeInsertions(Promise *pp);
static void VerifyAttributeDeletions(Promise *pp);
static int SelectNode(xmlDocPtr doc, xmlNodePtr *node, Attributes a, Promise *pp);
static int InsertTreeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static int InsertAttributeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp);
static int DeleteTreeAtNode(xmlNodePtr tree, Attributes a, Promise *pp);
static int DeleteAttributeAtNode(xmlAttrPtr attr, Attributes a, Promise *pp);
static int SanityCheckTreeInsertions(Attributes a);
static int SanityCheckAttributeInsertions(Attributes a);
static int SanityCheckTreeDeletions(Attributes a, Promise *pp);
static int SanityCheckAttributeDeletions(Attributes a, Promise *pp);
static int VerifyXPath(xmlDocPtr doc, Attributes a, Promise *pp);

static int XmlDocsEqual(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp);
xmlChar *CharToXmlChar(char* c);

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
             " !! WARNING: VerifyTreeInsertions: doc == NULL");
    }

    if (VerifyXPath(doc, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The verify_tree_xpath (%s) promise is already valid. No need to insert the promised tree (%s) in %s", a.xml.verify_tree_xpath, pp->promiser,
             pp->this_server);
        return;
    }

    if (!SelectNode(doc, &docnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree insertion (%s) could not select an edit node in %s", pp->promiser,
             pp->this_server);
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

static void VerifyTreeDeletions(Promise *pp)
{
    xmlDocPtr doc = pp->edcontext->xmldoc;
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

    if(doc == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: VerifyTreeDeletions: doc == NULL");
    }

        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! select xpath: %s", pp->promiser);


    if (!SelectNode(doc, &docnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree deletion (%s) could not select an edit node in %s", pp->promiser,
             pp->this_server);
        return;
    }

        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! selected xpath: %s", pp->promiser);


    snprintf(lockname, CF_BUFSIZE - 1, "deletetree-%s-%s", pp->promiser, pp->this_server);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, true);

    if (thislock.lock == NULL)
    {
        return;
    }

    if (DeleteTreeAtNode(docnode, a, pp))
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
             " !! WARNING: VerifyAttributeInsertions: doc == NULL");
    }

    if (VerifyXPath(doc, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The verify_attribute_xpath (%s) promise is already valid. No need to insert the promised attribute (%s) in %s", a.xml.verify_attribute_xpath, pp->promiser,
             pp->this_server);
        return;
    }

    if (!SelectNode(doc, &docnode, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised attribute insertion (%s) could not select an edit node in %s", pp->promiser,
             pp->this_server);
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

static void VerifyAttributeDeletions(Promise *pp)
{
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static int SelectNode(xmlDocPtr doc, xmlNodePtr *docnode, Attributes a, Promise *pp)
/*

This should provide pointers to the edit node within the xml document.
It returns true if a match was identified, else false.

If no such node matches, docnode should point to NULL

*/
{
    xmlNodePtr cur = NULL;
    const xmlChar* xpathExpr;

    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;

    if ((xpathExpr = CharToXmlChar(a.xml.insert_tree_xpath)) == NULL && (xpathExpr = CharToXmlChar(a.xml.delete_tree_xpath)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Error: unable to create new XPath expression from select_xpath");
        return false;
    }

    if((xpathCtx = xmlXPathNewContext(doc)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Error: unable to create new XPath context");
        return false;
    }

    if((xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Error: unable to evaluate xpath expression \"%s\"", xpathExpr);
        xmlXPathFreeContext(xpathCtx); 
        return false;
    }

    xmlNodeSetPtr nodes = xpathObj->nodesetval;

    int i, size;

    if ((size = (nodes) ? nodes->nodeNr : 0) == 0)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! insert/delete xpath: %s", xpathExpr);
        cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! size: %d", size);
        xmlXPathFreeContext(xpathCtx);
        xmlXPathFreeObject(xpathObj);
        return false;
    }

    cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! insert/delete xpath: %s", xpathExpr);
    cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! size: %d", size);

    //select first matching node
    for(i = 0; i < size; ++i)
    {
        if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
        {
            cur = nodes->nodeTab[i];
            cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! cur->name: %s", cur->name);
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

static int VerifyXPath(xmlDocPtr doc, Attributes a, Promise *pp)
{
    xmlNodePtr cur = NULL;
    const xmlChar* xpathExpr;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodeSetPtr nodes;

    if ((xpathExpr = CharToXmlChar(a.xml.verify_tree_xpath)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Error: unable to create new XPath expression from verify_tree_xpath");
        return false;
    }

    if((xpathCtx = xmlXPathNewContext(doc)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Error: unable to create new XPath context");
        return false;
    }

    if((xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! Error: unable to evaluate xpath expression \"%s\"", xpathExpr);
        xmlXPathFreeContext(xpathCtx); 
        return false;
    }

    if ((nodes = xpathObj->nodesetval) == NULL)
    {
        return false;
    }

    int i, size;

    if ((size = (nodes) ? nodes->nodeNr : 0) == 0)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! verify_tree_xpath: %s", xpathExpr);
        cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! size: %d", size);
        xmlXPathFreeContext(xpathCtx);
        xmlXPathFreeObject(xpathObj);
        return false;
    }

    cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! verify_tree_xpath: %s", xpathExpr);
    cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! size: %d", size);

    //select first matching node
    for(i = 0; i < size; ++i)
    {
        if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
        {
            cur = nodes->nodeTab[i];
            cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! cur->type: %d", cur->type);
            cfPS(cf_verbose, CF_INTERPT, "", pp, a, " !! cur->name: %s", cur->name);
            break;
        }
    }

    if(cur == NULL)
    {
        xmlXPathFreeContext(xpathCtx);
        xmlXPathFreeObject(xpathObj);
        return false;
    }

    xmlXPathFreeContext(xpathCtx);
    xmlXPathFreeObject(xpathObj);

    return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/
static int InsertTreeAtNode(char *chunk, xmlDocPtr doc, xmlNodePtr docnode, Attributes a, Promise *pp)
{
    xmlDocPtr treedoc = NULL;
    xmlNodePtr treenode = NULL;
    xmlNodePtr treeroot = NULL;
    xmlChar *buf = NULL;

    //for reading subtree from memory
    if ((buf = CharToXmlChar(chunk)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Tree to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

    //parse the subtree
    if ((treedoc = xmlParseMemory(buf, CF_BUFSIZE)) == NULL ) {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Tree to be inserted was not parsed successfully");
        return false;
    }
    if ((treeroot = xmlDocGetRootElement(treedoc)) == NULL) {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Tree to be inserted is empty");
        xmlFreeDoc(treedoc);
        return false;
    }

    if ((treenode = xmlDocCopyNode(treeroot, treedoc, 1)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Tree to be inserted could not be copied");
        return false;
    }

    //insert the subtree into xml document
    xmlAddChild(docnode, treenode);
    xmlFreeDoc(treedoc);

    if (!VerifyXPath(doc, a, pp))
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The verify_tree_xpath (%s) promise is not valid. Tree was not inserted successfully", a.xml.verify_tree_xpath);
        DeleteTreeAtNode(treenode, a, pp);
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

    if ((name = CharToXmlChar(chunk)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Attribute name to be inserted was not successfully loaded into an xml buffer");
        return false;
    }
    if ((value = CharToXmlChar(chunk)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Attribute value to be inserted was not successfully loaded into an xml buffer");
        return false;
    }

/*    if ((attr = xmlHasProp(docnode, name)) == NULL)
    {
    }
    if ((xmlNodeSetContent(nodes->nodeTab[i], value);) == NULL)
    {
    }*/

    if ((attr = xmlNewProp(docnode, name, value)) == NULL)
    {
        cfPS(cf_verbose, CF_INTERPT, "", pp, a,
             " !! WARNING: Attribute was not successfully inserted into xml document");
        return false;
    }

    return true;
}

/***************************************************************************/

static int DeleteTreeAtNode(xmlNodePtr tree, Attributes a, Promise *pp)
{
    if(tree == NULL)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised tree to be deleted does not exist at specified delete_tree_xpath (%s) and cannot be deleted.", a.xml.delete_tree_xpath);
        return false;
    }

    //remove the subtree from xml document
    xmlUnlinkNode(tree);
    xmlFreeNode(tree);

    return true;
}

static int DeleteAttributeAtNode(xmlAttrPtr attr, Attributes a, Promise *pp)
{
    if ((xmlRemoveProp(attr)) == -1)
    {
        cfPS(cf_error, CF_INTERPT, "", pp, a,
             " !! The promised attribute to be deleted was not deleted successfully.");
        return false;
    }
    return true;
}

/***************************************************************************/

static int SanityCheckTreeInsertions(Attributes a)
{
    long ok = true;

    if(!(a.xml.haveinserttreexpath && a.xml.haveverifytreexpath))
    {
        CfOut(cf_error, "",
              " !! Tree insertion requires both insert_tree_xpath and verify_tree_xpath to be specified");
        ok = false;
    }

    return ok;
}

/***************************************************************************/

static int SanityCheckAttributeInsertions(Attributes a)
{
    long ok = true;
    return ok;
}

/***************************************************************************/

static int SanityCheckTreeDeletions(Attributes a, Promise *pp)
{
    long ok = true;

    if(!a.xml.havedeletetreexpath)
    {
        CfOut(cf_error, "",
              " !! Tree deletion requires delete_tree_xpath to be specified");
        ok = false;
    }

    return ok;
}

/***************************************************************************/

static int SanityCheckAttributeDeletions(Attributes a, Promise *pp)
{
    long ok = true;
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

int CompareToXml(xmlDocPtr doc, char *file, Attributes a, Promise *pp)
/* returns true if xml on disk is identical to xml in memory */
{
    struct stat statbuf;
    xmlDocPtr cmpdoc = NULL;

    CfDebug("CompareToXml(%s)\n", file);

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

    if (!XmlDocsEqual(cmpdoc, doc, (a.transaction.action == cfa_warn), a, pp))
    {
        xmlFreeDoc(cmpdoc);
        return false;
    }

    xmlFreeDoc(cmpdoc);
    xmlFreeDoc(doc);

    return (true);
}

/*********************************************************************/

static int XmlDocsEqual(xmlDocPtr doc1, xmlDocPtr doc2, int warnings, Attributes a, Promise *pp)
// Some complex logic here to enable warnings of diffs to be given
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

xmlChar *CharToXmlChar(char* c)
{
    return BAD_CAST c;
}
