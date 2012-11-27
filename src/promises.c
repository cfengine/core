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

#include "promises.h"

#include "constraints.h"
#include "policy.h"
#include "syntax.h"
#include "expand.h"
#include "files_names.h"
#include "files_hashes.h"
#include "scope.h"
#include "unix.h"
#include "cfstream.h"
#include "args.h"

#define PACK_UPIFELAPSED_SALT "packageuplist"

static void DereferenceComment(Promise *pp);

/*****************************************************************************/

char *BodyName(const Promise *pp)
{
    char *name, *sp;
    int i, size = 0;
    Constraint *cp;

/* Return a type template for the promise body for lock-type identification */

    name = xmalloc(CF_MAXVARSIZE);

    sp = pp->agentsubtype;

    if (size + strlen(sp) < CF_MAXVARSIZE - CF_BUFFERMARGIN)
    {
        strcpy(name, sp);
        strcat(name, ".");
        size += strlen(sp);
    }

    for (i = 0, cp = pp->conlist; (i < 5) && cp != NULL; i++, cp = cp->next)
    {
        if (strcmp(cp->lval, "args") == 0)      /* Exception for args, by symmetry, for locking */
        {
            continue;
        }

        if (size + strlen(cp->lval) < CF_MAXVARSIZE - CF_BUFFERMARGIN)
        {
            strcat(name, cp->lval);
            strcat(name, ".");
            size += strlen(cp->lval);
        }
    }

    return name;
}

/*****************************************************************************/

Promise *DeRefCopyPromise(const char *scopeid, const Promise *pp)
{
    Promise *pcopy;
    Constraint *cp, *scp;
    Rval returnval;

    if (pp->promisee.item)
    {
        CfDebug("CopyPromise(%s->", pp->promiser);
        if (DEBUG)
        {
            ShowRval(stdout, pp->promisee);
        }
        CfDebug("\n");
    }
    else
    {
        CfDebug("CopyPromise(%s->)\n", pp->promiser);
    }

    pcopy = xcalloc(1, sizeof(Promise));

    if (pp->promiser)
    {
        pcopy->promiser = xstrdup(pp->promiser);
    }

    if (pp->promisee.item)
    {
        pcopy->promisee = CopyRvalItem(pp->promisee);
    }

    if (pp->classes)
    {
        pcopy->classes = xstrdup(pp->classes);
    }

/* FIXME: may it happen? */
    if ((pp->promisee.item != NULL && pcopy->promisee.item == NULL))
    {
        FatalError("Unable to copy promise");
    }

    pcopy->parent_subtype = pp->parent_subtype;
    pcopy->bundletype = xstrdup(pp->bundletype);
    pcopy->audit = pp->audit;
    pcopy->offset.line = pp->offset.line;
    pcopy->bundle = xstrdup(pp->bundle);
    pcopy->namespace = xstrdup(pp->namespace);
    pcopy->ref = pp->ref;
    pcopy->ref_alloc = pp->ref_alloc;
    pcopy->agentsubtype = pp->agentsubtype;
    pcopy->done = pp->done;
    pcopy->inode_cache = pp->inode_cache;
    pcopy->this_server = pp->this_server;
    pcopy->donep = pp->donep;
    pcopy->conn = pp->conn;
    pcopy->edcontext = pp->edcontext;
    pcopy->has_subbundles = pp->has_subbundles;
    pcopy->org_pp = pp;

    CfDebug("Copying promise constraints\n\n");

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        Body *bp = NULL;
        FnCall *fp = NULL;
        char *bodyname = NULL;

        /* A body template reference could look like a scalar or fn to the parser w/w () */
        Policy *policy = PolicyFromPromise(pp);
        Body *bodies = policy ? policy->bodies : NULL;

        switch (cp->rval.rtype)
        {
        case CF_SCALAR:
            bodyname = (char *) cp->rval.item;
            if (cp->references_body)
            {
                bp = IsBody(bodies, pp->namespace, bodyname);
            }
            fp = NULL;
            break;
        case CF_FNCALL:
            fp = (FnCall *) cp->rval.item;
            bodyname = fp->name;
            bp = IsBody(bodies, pp->namespace, bodyname);
            break;
        default:
            bp = NULL;
            fp = NULL;
            bodyname = NULL;
            break;
        }

        /* First case is: we have a body template to expand lval = body(args), .. */

        if (bp != NULL)
        {
            if (strcmp(bp->type, cp->lval) != 0)
            {
                CfOut(cf_error, "",
                      "Body type mismatch for body reference \"%s\" in promise at line %zu of %s (%s != %s)\n",
                      bodyname, pp->offset.line, (pp->audit)->filename, bp->type, cp->lval);
                ERRORCOUNT++;
            }

            /* Keep the referent body type as a boolean for convenience when checking later */

            ConstraintAppendToPromise(pcopy, cp->lval, (Rval) {xstrdup("true"), CF_SCALAR}, cp->classes, false);

            CfDebug("Handling body-lval \"%s\"\n", cp->lval);

            if (bp->args != NULL)
            {
                /* There are arguments to insert */

                if (fp == NULL || fp->args == NULL)
                {
                    CfOut(cf_error, "", "Argument mismatch for body reference \"%s\" in promise at line %zu of %s\n",
                          bodyname, pp->offset.line, (pp->audit)->filename);
                }

                NewScope("body");

                if (fp && bp && fp->args && bp->args && !MapBodyArgs("body", fp->args, bp->args))
                {
                    ERRORCOUNT++;
                    CfOut(cf_error, "",
                          "Number of arguments does not match for body reference \"%s\" in promise at line %zu of %s\n",
                          bodyname, pp->offset.line, (pp->audit)->filename);
                }

                for (scp = bp->conlist; scp != NULL; scp = scp->next)
                {
                    CfDebug("Doing arg-mapped sublval = %s (promises.c)\n", scp->lval);
                    returnval = ExpandPrivateRval("body", scp->rval);
                    ConstraintAppendToPromise(pcopy, scp->lval, returnval, scp->classes, false);
                }

                DeleteScope("body");
            }
            else
            {
                /* No arguments to deal with or body undeclared */

                if (fp != NULL)
                {
                    CfOut(cf_error, "",
                          "An apparent body \"%s()\" was undeclared or could have incorrect args, but used in a promise near line %zu of %s (possible unquoted literal value)",
                          bodyname, pp->offset.line, (pp->audit)->filename);
                }
                else
                {
                    for (scp = bp->conlist; scp != NULL; scp = scp->next)
                    {
                        CfDebug("Doing sublval = %s (promises.c)\n", scp->lval);
                        Rval newrv = CopyRvalItem(scp->rval);

                        ConstraintAppendToPromise(pcopy, scp->lval, newrv, scp->classes, false);
                    }
                }
            }
        }
        else
        {
            Policy *policy = PolicyFromPromise(pp);

            if (cp->references_body && !IsBundle(policy->bundles, bodyname))
            {
                CfOut(cf_error, "",
                      "Apparent body \"%s()\" was undeclared, but used in a promise near line %zu of %s (possible unquoted literal value)",
                      bodyname, pp->offset.line, (pp->audit)->filename);
            }

            Rval newrv = CopyRvalItem(cp->rval);

            scp = ConstraintAppendToPromise(pcopy, cp->lval, newrv, cp->classes, false);
        }
    }

    return pcopy;
}

/*****************************************************************************/

Promise *ExpandDeRefPromise(const char *scopeid, Promise *pp)
{
    Promise *pcopy;
    Constraint *cp;
    Rval returnval, final;

    CfDebug("ExpandDerefPromise()\n");

    pcopy = xcalloc(1, sizeof(Promise));

    returnval = ExpandPrivateRval("this", (Rval) {pp->promiser, CF_SCALAR});
    pcopy->promiser = (char *) returnval.item;

    if (pp->promisee.item)
    {
        pcopy->promisee = EvaluateFinalRval(scopeid, pp->promisee, true, pp);
    }
    else
    {
        pcopy->promisee = (Rval) {NULL, CF_NOPROMISEE};
    }

    if (pp->classes)
    {
        pcopy->classes = xstrdup(pp->classes);
    }
    else
    {
        pcopy->classes = xstrdup("any");
    }

    if (pcopy->promiser == NULL)
    {
        FatalError("ExpandPromise returned NULL");
    }

    pcopy->parent_subtype = pp->parent_subtype;
    pcopy->bundletype = xstrdup(pp->bundletype);
    pcopy->done = pp->done;
    pcopy->donep = pp->donep;
    pcopy->audit = pp->audit;
    pcopy->offset.line = pp->offset.line;
    pcopy->bundle = xstrdup(pp->bundle);
    pcopy->namespace = xstrdup(pp->namespace);
    pcopy->ref = pp->ref;
    pcopy->ref_alloc = pp->ref_alloc;
    pcopy->agentsubtype = pp->agentsubtype;
    pcopy->cache = pp->cache;
    pcopy->inode_cache = pp->inode_cache;
    pcopy->this_server = pp->this_server;
    pcopy->conn = pp->conn;
    pcopy->edcontext = pp->edcontext;
    pcopy->org_pp = pp;

/* No further type checking should be necessary here, already done by CheckConstraintTypeMatch */

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        Rval returnval;

        if (ExpectedDataType(cp->lval) == cf_bundle)
        {
            final = ExpandBundleReference(scopeid, cp->rval);
        }
        else
        {
            returnval = EvaluateFinalRval(scopeid, cp->rval, false, pp);
            final = ExpandDanglers(scopeid, returnval, pp);
            DeleteRvalItem(returnval);
        }

        ConstraintAppendToPromise(pcopy, cp->lval, final, cp->classes, false);

        if (strcmp(cp->lval, "comment") == 0)
        {
            if (final.rtype != CF_SCALAR)
            {
                char err[CF_BUFSIZE];

                snprintf(err, CF_BUFSIZE, "Comments can only be scalar objects (not %c in \"%s\")", final.rtype,
                         pp->promiser);
                yyerror(err);
            }
            else
            {
                pcopy->ref = final.item;        /* No alloc reference to comment item */

                if (pcopy->ref && (strstr(pcopy->ref, "$(this.promiser)") || strstr(pcopy->ref, "${this.promiser}")))
                {
                    DereferenceComment(pcopy);
                }
            }
        }
    }

    return pcopy;
}

/*******************************************************************/

Body *IsBody(Body *list, const char *namespace, const char *key)
{
    char fqname[CF_BUFSIZE];

    for (Body *bp = list; bp != NULL; bp = bp->next)
    {

    // bp->namespace is where the body belongs, namespace is where we are now

        if (strchr(key, CF_NS) || strcmp(namespace,"default") == 0)
        {
            if (strncmp(key,"default:",strlen("default:")) == 0) // CF_NS == ':'
            {
                strcpy(fqname,strchr(key,CF_NS)+1);
            }
            else
            {
                strcpy(fqname,key);
            }        
        }
        else
        {
            snprintf(fqname,CF_BUFSIZE-1, "%s%c%s", namespace, CF_NS, key);
        }

        if (strcmp(bp->name, fqname) == 0)
        {
            return bp;
        }
    }

    return NULL;
}

/*******************************************************************/

Bundle *IsBundle(Bundle *list, const char *key)
{
    Bundle *bp;
    char fqname[CF_BUFSIZE];

    for (bp = list; bp != NULL; bp = bp->next)
    {
        if (strcmp(bp->namespace,"default") == 0)
        {
            if (strncmp(key,"default:",strlen("default:")) == 0)  // CF_NS == ':'
            {
                strcpy(fqname,strchr(key, CF_NS)+1);
            }
            else
            {
                strcpy(fqname,key);
            }        
        }
        else if (strncmp(bp->namespace,key,strlen(bp->namespace)) == 0)
        {
            strcpy(fqname,key);
        }
        else
        {
            snprintf(fqname,CF_BUFSIZE-1, "%s%c%s", bp->namespace, CF_NS, key);
        }

        if (strcmp(bp->name, fqname) == 0)
        {
            return bp;
        }
    }

    return NULL;
}

/*****************************************************************************/
/* Cleanup                                                                   */
/*****************************************************************************/

void DeletePromises(Promise *pp)
{
    if (pp == NULL)
    {
        return;
    }

    if (pp->this_server != NULL)
    {
        ThreadLock(cft_policy);
        free(pp->this_server);
        ThreadUnlock(cft_policy);
    }

    if (pp->next != NULL)
    {
        DeletePromises(pp->next);
    }

    if (pp->ref_alloc == 'y')
    {
        ThreadLock(cft_policy);
        free(pp->ref);
        ThreadUnlock(cft_policy);
    }

    DeletePromise(pp);
}

/*****************************************************************************/

Promise *NewPromise(char *typename, char *promiser)
{
    Promise *pp;

    ThreadLock(cft_policy);

    pp = xcalloc(1, sizeof(Promise));

    pp->audit = AUDITPTR;
    pp->bundle = xstrdup("cfe_internal_bundle_hardcoded");
    pp->namespace = xstrdup("default");
    pp->promiser = xstrdup(promiser);

    ThreadUnlock(cft_policy);

    pp->promisee = (Rval) {NULL, CF_NOPROMISEE};
    pp->donep = &(pp->done);

    pp->agentsubtype = typename;        /* cache this, do not copy string */
    pp->ref_alloc = 'n';
    pp->has_subbundles = false;

    ConstraintAppendToPromise(pp, "handle",
                              (Rval) {xstrdup("cfe_internal_promise_hardcoded"),
                              CF_SCALAR}, NULL, false);

    return pp;
}

/*****************************************************************************/

void DeletePromise(Promise *pp)
{
    if (pp == NULL)
    {
        return;
    }

    CfDebug("DeletePromise(%s->[%c])\n", pp->promiser, pp->promisee.rtype);

    ThreadLock(cft_policy);

    if (pp->promiser != NULL)
    {
        free(pp->promiser);
    }

    if (pp->promisee.item != NULL)
    {
        DeleteRvalItem(pp->promisee);
    }

    free(pp->bundle);
    free(pp->bundletype);
    free(pp->classes);
    free(pp->namespace);

// ref and agentsubtype are only references, do not free

    DeleteConstraintList(pp->conlist);

    free((char *) pp);
    ThreadUnlock(cft_policy);
}

/*****************************************************************************/

void PromiseRef(enum cfreport level, const Promise *pp)
{
    char *v;
    Rval retval;
    char buffer[CF_BUFSIZE];

    if (pp == NULL)
    {
        return;
    }

    if (GetVariable("control_common", "version", &retval) != cf_notype)
    {
        v = (char *) retval.item;
    }
    else
    {
        v = "not specified";
    }

    if (pp->audit)
    {
        CfOut(level, "", "Promise (version %s) belongs to bundle \'%s\' in file \'%s\' near line %zu\n", v, pp->bundle,
              pp->audit->filename, pp->offset.line);
    }
    else
    {
        CfOut(level, "", "Promise (version %s) belongs to bundle \'%s\' near line %zu\n", v, pp->bundle,
              pp->offset.line);
    }

    if (pp->ref)
    {
        CfOut(level, "", "Comment: %s\n", pp->ref);
    }

    switch (pp->promisee.rtype)
    {
       case CF_SCALAR:
           CfOut(level, "", "This was a promise to: %s\n", (char *)(pp->promisee.item));
           break;
       case CF_LIST:
           PrintRlist(buffer, CF_BUFSIZE, (Rlist *)pp->promisee.item);
           CfOut(level, "", "This was a promise to: %s",buffer);
           break;
       default:
           break;
    }
}

/*******************************************************************/

void HashPromise(char *salt, Promise *pp, unsigned char digest[EVP_MAX_MD_SIZE + 1], enum cfhashes type)
{
    EVP_MD_CTX context;
    int md_len;
    const EVP_MD *md = NULL;
    Constraint *cp;
    Rlist *rp;
    FnCall *fp;

    char *noRvalHash[] = { "mtime", "atime", "ctime", NULL };
    int doHash;
    int i;

    md = EVP_get_digestbyname(FileHashName(type));

    EVP_DigestInit(&context, md);

// multiple packages (promisers) may share same package_list_update_ifelapsed lock
    if (!(salt && (strncmp(salt, PACK_UPIFELAPSED_SALT, sizeof(PACK_UPIFELAPSED_SALT) - 1) == 0)))
    {
        EVP_DigestUpdate(&context, pp->promiser, strlen(pp->promiser));
    }

    if (pp->ref)
    {
        EVP_DigestUpdate(&context, pp->ref, strlen(pp->ref));
    }

    if (pp->this_server)
    {
        EVP_DigestUpdate(&context, pp->this_server, strlen(pp->this_server));
    }

    if (salt)
    {
        EVP_DigestUpdate(&context, salt, strlen(salt));
    }

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        EVP_DigestUpdate(&context, cp->lval, strlen(cp->lval));

        // don't hash rvals that change (e.g. times)
        doHash = true;

        for (i = 0; noRvalHash[i] != NULL; i++)
        {
            if (strcmp(cp->lval, noRvalHash[i]) == 0)
            {
                doHash = false;
                break;
            }
        }

        if (!doHash)
        {
            continue;
        }

        switch (cp->rval.rtype)
        {
        case CF_SCALAR:
            EVP_DigestUpdate(&context, cp->rval.item, strlen(cp->rval.item));
            break;

        case CF_LIST:
            for (rp = cp->rval.item; rp != NULL; rp = rp->next)
            {
                EVP_DigestUpdate(&context, rp->item, strlen(rp->item));
            }
            break;

        case CF_FNCALL:

            /* Body or bundle */

            fp = (FnCall *) cp->rval.item;

            EVP_DigestUpdate(&context, fp->name, strlen(fp->name));

            for (rp = fp->args; rp != NULL; rp = rp->next)
            {
                EVP_DigestUpdate(&context, rp->item, strlen(rp->item));
            }
            break;
        }
    }

    EVP_DigestFinal(&context, digest, &md_len);

/* Digest length stored in md_len */
}

/*******************************************************************/

static void DereferenceComment(Promise *pp)
{
    char pre_buffer[CF_BUFSIZE], post_buffer[CF_BUFSIZE], buffer[CF_BUFSIZE], *sp;
    int offset = 0;

    strlcpy(pre_buffer, pp->ref, CF_BUFSIZE);

    if ((sp = strstr(pre_buffer, "$(this.promiser)")) || (sp = strstr(pre_buffer, "${this.promiser}")))
    {
        *sp = '\0';
        offset = sp - pre_buffer + strlen("$(this.promiser)");
        strncpy(post_buffer, pp->ref + offset, CF_BUFSIZE);
        snprintf(buffer, CF_BUFSIZE, "%s%s%s", pre_buffer, pp->promiser, post_buffer);

        if (pp->ref_alloc == 'y')
        {
            free(pp->ref);
        }

        pp->ref = xstrdup(buffer);
        pp->ref_alloc = 'y';
    }
}
