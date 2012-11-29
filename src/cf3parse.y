
%{

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
#include "parser_state.h"

#include "constraints.h"
#include "env_context.h"
#include "fncall.h"

// FIX: remove
#include "syntax.h"

extern char *yytext;

static void DebugBanner(const char *s);
static void fatal_yyerror(const char *s);

static bool INSTALL_SKIP = false;

#define YYMALLOC xmalloc

%}

%token IDSYNTAX BLOCKID QSTRING CLASS CATEGORY BUNDLE BODY ASSIGN ARROW NAKEDVAR

%%

specification:       /* empty */
                     | blocks

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

blocks:                block
                     | blocks block;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

block:                 bundle typeid blockid bundlebody
                     | bundle typeid blockid usearglist bundlebody
                     | body typeid blockid bodybody
                     | body typeid blockid usearglist bodybody;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundle:                BUNDLE
                       {
                           DebugBanner("Bundle");
                           P.block = "bundle";
                           P.rval = (Rval) { NULL, '\0' };
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           P.currentstring = NULL;
                           strcpy(P.blockid,"");
                           strcpy(P.blocktype,"");
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

body:                  BODY
                       {
                           DebugBanner("Body");
                           P.block = "body";
                           strcpy(P.blockid,"");
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           P.currentstring = NULL;
                           strcpy(P.blocktype,"");
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typeid:                IDSYNTAX
                       {
                           strncpy(P.blocktype,P.currentid,CF_MAXVARSIZE);
                           CfDebug("Found block type %s for %s\n",P.blocktype,P.block);

                           DeleteRlist(P.useargs);
                           P.useargs = NULL;
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

blockid:               IDSYNTAX
                       {
                           strncpy(P.blockid,P.currentid,CF_MAXVARSIZE);
                           P.offsets.last_block_id = P.offsets.last_id;
                           CfDebug("Found identifier %s for %s\n",P.currentid,P.block);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

usearglist:            '('
                       aitems
                       ')';

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitems:                aitem
                     | aitems ',' aitem
                     |;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitem:                 IDSYNTAX  /* recipient of argument is never a literal */
                       {
                           AppendRlist(&(P.useargs),P.currentid,CF_SCALAR);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundlebody:            '{'
                       {
                           if (RelevantBundle(CF_AGENTTYPES[THIS_AGENT_TYPE], P.blocktype))
                           {
                               CfDebug("We a compiling everything here\n");
                               INSTALL_SKIP = false;
                           }
                           else if (strcmp(CF_AGENTTYPES[THIS_AGENT_TYPE], P.blocktype) != 0)
                           {
                               CfDebug("This is for a different agent\n");
                               INSTALL_SKIP = true;
                           }

                           if (!INSTALL_SKIP)
                           {
                               P.currentbundle = AppendBundle(P.policy, P.blockid, P.blocktype, P.useargs, P.filename);
                               P.currentbundle->offset.line = P.line_no;
                               P.currentbundle->offset.start = P.offsets.last_block_id;
                           }
                           else
                           {
                               P.currentbundle = NULL;
                           }

                           DeleteRlist(P.useargs);
                           P.useargs = NULL;
                       }

                       statements
                       '}'
                       {
                           INSTALL_SKIP = false;
                           P.offsets.last_id = -1;
                           P.offsets.last_string = -1;
                           P.offsets.last_class_id = -1;

                           if (P.currentbundle)
                           {
                               P.currentbundle->offset.end = P.offsets.current;
                           }
                           CfDebug("End promise bundle\n\n");
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

statements:            statement
                     | statements statement;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

statement:             category
                     | classpromises;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bodybody:              '{'
                       {
                           P.currentbody = AppendBody(P.policy, P.blockid, P.blocktype, P.useargs, P.filename);
                           if (P.currentbody)
                           {
                               P.currentbody->offset.line = P.line_no;
                               P.currentbody->offset.start = P.offsets.last_block_id;
                           }

                           DeleteRlist(P.useargs);
                           P.useargs = NULL;

                           strcpy(P.currentid,"");
                           CfDebug("Starting block\n");
                       }

                       bodyattribs

                       '}'
                       {
                           P.offsets.last_id = -1;
                           P.offsets.last_string = -1;
                           P.offsets.last_class_id = -1;
                           if (P.currentbody)
                           {
                               P.currentbody->offset.end = P.offsets.current;
                           }
                           CfDebug("End promise body\n");
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bodyattribs:           bodyattrib                    /* BODY ONLY */
                     | bodyattribs bodyattrib;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bodyattrib:            class
                     | selections;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

selections:            selection                 /* BODY ONLY */
                     | selections selection;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

selection:             id                         /* BODY ONLY */
                       ASSIGN
                       rval
                       {
                           CheckSelection(P.blocktype,P.blockid,P.lval,P.rval);

                           if (!INSTALL_SKIP)
                           {
                               Constraint *cp = NULL;

                               if (P.currentclasses == NULL)
                               {
                                   cp = ConstraintAppendToBody(P.currentbody, P.lval, P.rval, "any", P.references_body);
                               }
                               else
                               {
                                   cp = ConstraintAppendToBody(P.currentbody,P.lval,P.rval,P.currentclasses,P.references_body);
                               }
                               cp->offset.line = P.line_no;
                               cp->offset.start = P.offsets.last_id;
                               cp->offset.end = P.offsets.current;
                               cp->offset.context = P.offsets.last_class_id;
                           }
                           else
                           {
                               DeleteRvalItem(P.rval);
                           }

                           if (strcmp(P.blockid,"control") == 0 && strcmp(P.blocktype,"file") == 0)
                           {
                               if (strcmp(P.lval,"namespace") == 0)
                               {
                                   if (P.rval.rtype != CF_SCALAR)
                                   {
                                       yyerror("namespace must be a constant scalar string");
                                   }
                                   else
                                   {
                                       PolicySetNameSpace(P.policy, P.rval.item);
                                   }
                               }
                           }
                           
                           if (strcmp(P.blockid,"control") == 0 && strcmp(P.blocktype,"common") == 0)
                           {
                               if (strcmp(P.lval,"inputs") == 0)
                               {
                                   if (IsDefinedClass(P.currentclasses, CurrentNameSpace(P.policy)))
                                   {
                                       if (VINPUTLIST == NULL)
                                       {
                                           if (P.rval.rtype == CF_LIST)
                                           {
                                               VINPUTLIST = P.rval.item;
                                           }
                                           else
                                           {
                                               yyerror("inputs promise must have a list as rvalue");
                                           }
                                       }
                                       else
                                       {
                                           yyerror("Redefinition of input list (broken promise)");
                                       }
                                   }
                               }
                           }

                           P.rval = (Rval) { NULL, '\0' };
                       }
                       ';' ;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

classpromises:         classpromise                     /* BUNDLE ONLY */
                     | classpromises classpromise;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

classpromise:          class
                     | promises;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promises:              promise                  /* BUNDLE ONLY */
                     | promises promise;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

category:              CATEGORY                  /* BUNDLE ONLY */
                       {
                           CfDebug("\n* Begin new promise type category %s in function \n\n",P.currenttype);

                           if (strcmp(P.block,"bundle") == 0)
                           {
                               if (!INSTALL_SKIP)
                               {
                                   P.currentstype = AppendSubType(P.currentbundle,P.currenttype);
                                   P.currentstype->offset.line = P.line_no;
                                   P.currentstype->offset.start = P.offsets.last_subtype_id;
                               }
                               else
                               {
                                   P.currentstype = NULL;
                               }
                           }
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promise:               promiser                    /* BUNDLE ONLY */

                       ARROW

                       rval            /* This is full form with rlist promisees */
                       {
                           if (!INSTALL_SKIP)
                           {
                               P.currentpromise = AppendPromise(P.currentstype, P.promiser,
                                                                P.rval,
                                                                P.currentclasses ? P.currentclasses : "any",
                                                                P.blockid, P.blocktype,CurrentNameSpace(P.policy));
                               P.currentpromise->offset.line = P.line_no;
                               P.currentpromise->offset.start = P.offsets.last_string;
                               P.currentpromise->offset.context = P.offsets.last_class_id;
                           }
                           else
                           {
                               P.currentpromise = NULL;
                           }
                       }

                       constraints ';'
                       {
                           CfDebug("End implicit promise %s\n\n",P.promiser);
                           strcpy(P.currentid,"");
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           free(P.promiser);
                           if (P.currentstring)
                           {
                               free(P.currentstring);
                           }
                           P.currentstring = NULL;
                           P.promiser = NULL;
                           P.promisee = NULL;
                           /* reset argptrs etc*/
                       }

                       |

                       promiser
                       {
                           if (!INSTALL_SKIP)
                           {
                               P.currentpromise = AppendPromise(P.currentstype, P.promiser,
                                                                (Rval) { NULL, CF_NOPROMISEE },
                                                                P.currentclasses ? P.currentclasses : "any",
                                                                P.blockid, P.blocktype,CurrentNameSpace(P.policy));
                               P.currentpromise->offset.line = P.line_no;
                               P.currentpromise->offset.start = P.offsets.last_string;
                               P.currentpromise->offset.context = P.offsets.last_class_id;
                           }
                           else
                           {
                               P.currentpromise = NULL;
                           }
                       }

                       constraints ';'
                       {
                           CfDebug("End full promise with promisee %s\n\n",P.promiser);

                           /* Don't free these */
                           strcpy(P.currentid,"");
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           free(P.promiser);
                           if (P.currentstring)
                           {
                               free(P.currentstring);
                           }
                           P.currentstring = NULL;
                           P.promiser = NULL;
                           P.promisee = NULL;
                           /* reset argptrs etc*/
                       }
                       ;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

constraints:           constraint               /* BUNDLE ONLY */
                     | constraints ',' constraint
                     |;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

constraint:            id                        /* BUNDLE ONLY */
                       ASSIGN
                       rval
                       {
                           if (!INSTALL_SKIP)
                           {
                               Constraint *cp = NULL;
                               SubTypeSyntax ss = SubTypeSyntaxLookup(P.blocktype,P.currenttype);
                               CheckConstraint(P.currenttype, CurrentNameSpace(P.policy), P.blockid, P.lval, P.rval, ss);
                               cp = ConstraintAppendToPromise(P.currentpromise, P.lval, P.rval, "any", P.references_body);
                               cp->offset.line = P.line_no;
                               cp->offset.start = P.offsets.last_id;
                               cp->offset.end = P.offsets.current;
                               cp->offset.context = P.offsets.last_class_id;
                               P.currentstype->offset.end = P.offsets.current;

                               // Cache whether there are subbundles for later $(this.promiser) logic

                               if (strcmp(P.lval,"usebundle") == 0 || strcmp(P.lval,"edit_line") == 0
                                   || strcmp(P.lval,"edit_xml") == 0)
                               {
                                   P.currentpromise->has_subbundles = true;
                               }

                               P.rval = (Rval) { NULL, '\0' };
                               strcpy(P.lval,"no lval");
                               DeleteRlist(P.currentRlist);
                               P.currentRlist = NULL;
                           }
                           else
                           {
                               DeleteRvalItem(P.rval);
                           }
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class:                 CLASS
                       {
                           P.offsets.last_class_id = P.offsets.current - strlen(P.currentclasses) - 2;
                           CfDebug("  New class context \'%s\' :: \n\n",P.currentclasses);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

id:                    IDSYNTAX
                       {
                           strncpy(P.lval,P.currentid,CF_MAXVARSIZE);
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           CfDebug("Recorded LVAL %s\n",P.lval);
                       };


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

rval:                  IDSYNTAX
                       {
                           P.rval = (Rval) { xstrdup(P.currentid), CF_SCALAR };
                           P.references_body = true;
                           CfDebug("Recorded IDRVAL %s\n", P.currentid);
                       }
                     | BLOCKID
                       {
                           P.rval = (Rval) { xstrdup(P.currentid), CF_SCALAR };
                           P.references_body = true;
                           CfDebug("Recorded IDRVAL %s\n", P.currentid);
                       }
                     | QSTRING
                       {
                           P.rval = (Rval) { P.currentstring, CF_SCALAR };
                           CfDebug("Recorded scalarRVAL %s\n", P.currentstring);

                           P.currentstring = NULL;
                           P.references_body = false;

                           if (P.currentpromise)
                           {
                               if (LvalWantsBody(P.currentpromise->agentsubtype,P.lval))
                               {
                                   yyerror("An rvalue is quoted, but we expect an unquoted body identifier");
                               }
                           }
                       }
                     | NAKEDVAR
                       {
                           P.rval = (Rval) { P.currentstring, CF_SCALAR };
                           CfDebug("Recorded saclarvariableRVAL %s\n", P.currentstring);

                           P.currentstring = NULL;
                           P.references_body = false;
                       }
                     | list
                       {
                           P.rval = (Rval) { CopyRlist(P.currentRlist), CF_LIST };
                           DeleteRlist(P.currentRlist);
                           P.currentRlist = NULL;
                           P.references_body = false;
                       }
                     | usefunction
                       {
                           P.rval = (Rval) { P.currentfncall[P.arg_nesting+1], CF_FNCALL };
                           P.references_body = false;
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

list:                  '{'
                       litems
                       '}';

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

litems:                litems_int
                     | litems_int ',';

litems_int:            litem
                     | litems_int ',' litem;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

litem:                 IDSYNTAX
                       {
                           AppendRlist((Rlist **)&P.currentRlist,P.currentid,CF_SCALAR);
                       }

                     | QSTRING
                       {
                           AppendRlist((Rlist **)&P.currentRlist,(void *)P.currentstring,CF_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           AppendRlist((Rlist **)&P.currentRlist,(void *)P.currentstring,CF_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           CfDebug("Install function call as list item from level %d\n",P.arg_nesting+1);
                           AppendRlist((Rlist **)&P.currentRlist,(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                           DeleteFnCall(P.currentfncall[P.arg_nesting+1]);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

functionid:            IDSYNTAX
                       {
                           CfDebug("Found function identifier %s\n",P.currentid);
                       }
                     | BLOCKID
                       {
                           CfDebug("Found qualified function identifier %s\n",P.currentid);
                       }
                     | NAKEDVAR
                       {
                           strncpy(P.currentid,P.currentstring,CF_MAXVARSIZE); // Make a var look like an ID
                           free(P.currentstring);
                           P.currentstring = NULL;
                           CfDebug("Found variable in place of a function identifier %s\n",P.currentid);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promiser:              QSTRING
                       {
                           P.promiser = P.currentstring;
                           P.currentstring = NULL;
                           CfDebug("Promising object name \'%s\'\n",P.promiser);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

usefunction:           functionid givearglist
                       {
                           CfDebug("Finished with function call, now at level %d\n\n",P.arg_nesting);
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

givearglist:           '('
                       {
                           if (++P.arg_nesting >= CF_MAX_NESTING)
                           {
                               fatal_yyerror("Nesting of functions is deeper than recommended");
                           }
                           P.currentfnid[P.arg_nesting] = xstrdup(P.currentid);
                           CfDebug("Start FnCall %s args level %d\n",P.currentfnid[P.arg_nesting],P.arg_nesting);
                       }

                       gaitems
                       ')'
                       {
                           CfDebug("End args level %d\n",P.arg_nesting);
                           P.currentfncall[P.arg_nesting] = NewFnCall(P.currentfnid[P.arg_nesting],P.giveargs[P.arg_nesting]);
                           P.giveargs[P.arg_nesting] = NULL;
                           strcpy(P.currentid,"");
                           free(P.currentfnid[P.arg_nesting]);
                           P.currentfnid[P.arg_nesting] = NULL;
                           P.arg_nesting--;
                       };


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

gaitems:               gaitem
                     | gaitems ',' gaitem
                     |;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

gaitem:                IDSYNTAX
                       {
                           /* currently inside a use function */
                           AppendRlist(&P.giveargs[P.arg_nesting],P.currentid,CF_SCALAR);
                       }

                     | QSTRING
                       {
                           /* currently inside a use function */
                           AppendRlist(&P.giveargs[P.arg_nesting],P.currentstring,CF_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           /* currently inside a use function */
                           AppendRlist(&P.giveargs[P.arg_nesting],P.currentstring,CF_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           /* Careful about recursion */
                           AppendRlist(&P.giveargs[P.arg_nesting],(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                           DeleteRvalItem((Rval) { P.currentfncall[P.arg_nesting+1], CF_FNCALL });
                       };

%%

/*****************************************************************/

void yyerror(const char *s)
{
    char *sp = yytext;

    if (sp == NULL)
    {
        if (USE_GCC_BRIEF_FORMAT)
        {
            fprintf(stderr, "%s:%d:%d: error: %s\n", P.filename, P.line_no, P.line_pos, s);
        }
        else
        {
            fprintf(stderr, "%s> %s:%d,%d: %s, near token \'NULL\'\n", VPREFIX, P.filename, P.line_no, P.line_pos, s);
        }
    }
    else if (*sp == '\"' && strlen(sp) > 1)
    {
        sp++;
    }

    if (USE_GCC_BRIEF_FORMAT)
    {
        fprintf(stderr, "%s:%d:%d: error: %s, near token \'%.20s\'\n", P.filename, P.line_no, P.line_pos, s, sp);
    }
    else
    {
        fprintf(stderr, "%s> %s:%d,%d: %s, near token \'%.20s\'\n", VPREFIX, P.filename, P.line_no, P.line_pos, s, sp);
    }

    ERRORCOUNT++;

    if (ERRORCOUNT > 10)
    {
        FatalError("Too many errors");
    }
}

static void fatal_yyerror(const char *s)
{
    char *sp = yytext;
    /* Skip quotation mark */
    if (sp && *sp == '\"' && sp[1])
    {
        sp++;
    }

    FatalError("%s: %d,%d: Fatal error during parsing: %s, near token \'%.20s\'\n", P.filename, P.line_no, P.line_pos, s, sp ? sp : "NULL");
}

static void DebugBanner(const char *s)
{
    CfDebug("----------------------------------------------------------------\n");
    CfDebug("  %s                                                            \n", s);
    CfDebug("----------------------------------------------------------------\n");
}
