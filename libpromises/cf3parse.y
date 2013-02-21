
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

#include "env_context.h"
#include "fncall.h"
#include "logging.h"
#include "rlist.h"
#include "item_lib.h"
#include "policy.h"
#include "mod_files.h"

int yylex(void);

// FIX: remove
#include "syntax.h"

extern char *yytext;

static int RelevantBundle(const char *agent, const char *blocktype);
static void DebugBanner(const char *s);
static bool LvalWantsBody(char *stype, char *lval);
static SyntaxTypeMatch CheckSelection(const char *type, const char *name, const char *lval, Rval rval);
static SyntaxTypeMatch CheckConstraint(const char *type, const char *lval, Rval rval, SubTypeSyntax ss);
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
                           RlistDestroy(P.currentRlist);
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
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                           P.currentstring = NULL;
                           strcpy(P.blocktype,"");
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typeid:                IDSYNTAX
                       {
                           strncpy(P.blocktype,P.currentid,CF_MAXVARSIZE);
                           CfDebug("Found block type %s for %s\n",P.blocktype,P.block);

                           RlistDestroy(P.useargs);
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
                           RlistAppend(&(P.useargs),P.currentid, RVAL_TYPE_SCALAR);
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
                               P.currentbundle = PolicyAppendBundle(P.policy, P.current_namespace, P.blockid, P.blocktype, P.useargs, P.filename);
                               P.currentbundle->offset.line = P.line_no;
                               P.currentbundle->offset.start = P.offsets.last_block_id;
                           }
                           else
                           {
                               P.currentbundle = NULL;
                           }

                           RlistDestroy(P.useargs);
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
                           P.currentbody = PolicyAppendBody(P.policy, P.current_namespace, P.blockid, P.blocktype, P.useargs, P.filename);
                           if (P.currentbody)
                           {
                               P.currentbody->offset.line = P.line_no;
                               P.currentbody->offset.start = P.offsets.last_block_id;
                           }

                           RlistDestroy(P.useargs);
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
                           {
                               SyntaxTypeMatch err = CheckSelection(P.blocktype, P.blockid, P.lval, P.rval);
                               if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
                               {
                                   yyerror(SyntaxTypeMatchToString(err));
                               }
                           }

                           if (!INSTALL_SKIP)
                           {
                               Constraint *cp = NULL;

                               if (P.rval.type == RVAL_TYPE_SCALAR && strcmp(P.lval, "ifvarclass") == 0)
                               {
                                   ValidateClassSyntax(P.rval.item);
                               }

                               if (P.currentclasses == NULL)
                               {
                                   cp = BodyAppendConstraint(P.currentbody, P.lval, P.rval, "any", P.references_body);
                               }
                               else
                               {
                                   cp = BodyAppendConstraint(P.currentbody, P.lval, P.rval, P.currentclasses, P.references_body);
                               }
                               cp->offset.line = P.line_no;
                               cp->offset.start = P.offsets.last_id;
                               cp->offset.end = P.offsets.current;
                               cp->offset.context = P.offsets.last_class_id;
                           }
                           else
                           {
                               RvalDestroy(P.rval);
                           }

                           if (strcmp(P.blockid,"control") == 0 && strcmp(P.blocktype,"file") == 0)
                           {
                               if (strcmp(P.lval,"namespace") == 0)
                               {
                                   if (P.rval.type != RVAL_TYPE_SCALAR)
                                   {
                                       yyerror("namespace must be a constant scalar string");
                                   }
                                   else
                                   {
                                       free(P.current_namespace);
                                       P.current_namespace = xstrdup(P.rval.item);
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
                                   P.currentstype = BundleAppendSubType(P.currentbundle,P.currenttype);
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
                               P.currentpromise = SubTypeAppendPromise(P.currentstype, P.promiser,
                                                                       P.rval,
                                                                       P.currentclasses ? P.currentclasses : "any");
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
                           RlistDestroy(P.currentRlist);
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
                               P.currentpromise = SubTypeAppendPromise(P.currentstype, P.promiser,
                                                                (Rval) { NULL, RVAL_TYPE_NOPROMISEE },
                                                                P.currentclasses ? P.currentclasses : "any");
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
                           RlistDestroy(P.currentRlist);
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
                               {
                                   SyntaxTypeMatch err = CheckConstraint(P.currenttype, P.lval, P.rval, ss);
                                   if (err != SYNTAX_TYPE_MATCH_OK && err != SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED)
                                   {
                                       yyerror(SyntaxTypeMatchToString(err));
                                   }
                               }
                               if (P.rval.type == RVAL_TYPE_SCALAR && strcmp(P.lval, "ifvarclass") == 0)
                               {
                                   ValidateClassSyntax(P.rval.item);
                               }

                               cp = PromiseAppendConstraint(P.currentpromise, P.lval, P.rval, "any", P.references_body);
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
                               RlistDestroy(P.currentRlist);
                               P.currentRlist = NULL;
                           }
                           else
                           {
                               RvalDestroy(P.rval);
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
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                           CfDebug("Recorded LVAL %s\n",P.lval);
                       };


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

rval:                  IDSYNTAX
                       {
                           P.rval = (Rval) { xstrdup(P.currentid), RVAL_TYPE_SCALAR };
                           P.references_body = true;
                           CfDebug("Recorded IDRVAL %s\n", P.currentid);
                       }
                     | BLOCKID
                       {
                           P.rval = (Rval) { xstrdup(P.currentid), RVAL_TYPE_SCALAR };
                           P.references_body = true;
                           CfDebug("Recorded IDRVAL %s\n", P.currentid);
                       }
                     | QSTRING
                       {
                           P.rval = (Rval) { P.currentstring, RVAL_TYPE_SCALAR };
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
                           P.rval = (Rval) { P.currentstring, RVAL_TYPE_SCALAR };
                           CfDebug("Recorded saclarvariableRVAL %s\n", P.currentstring);

                           P.currentstring = NULL;
                           P.references_body = false;
                       }
                     | list
                       {
                           P.rval = (Rval) { RlistCopy(P.currentRlist), RVAL_TYPE_LIST };
                           RlistDestroy(P.currentRlist);
                           P.currentRlist = NULL;
                           P.references_body = false;
                       }
                     | usefunction
                       {
                           P.rval = (Rval) { P.currentfncall[P.arg_nesting+1], RVAL_TYPE_FNCALL };
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
                           RlistAppend((Rlist **)&P.currentRlist,P.currentid, RVAL_TYPE_SCALAR);
                       }

                     | QSTRING
                       {
                           RlistAppend((Rlist **)&P.currentRlist,(void *)P.currentstring, RVAL_TYPE_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           RlistAppend((Rlist **)&P.currentRlist,(void *)P.currentstring, RVAL_TYPE_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           CfDebug("Install function call as list item from level %d\n",P.arg_nesting+1);
                           RlistAppend((Rlist **)&P.currentRlist,(void *)P.currentfncall[P.arg_nesting+1], RVAL_TYPE_FNCALL);
                           FnCallDestroy(P.currentfncall[P.arg_nesting+1]);
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
                           P.currentfncall[P.arg_nesting] = FnCallNew(P.currentfnid[P.arg_nesting],P.giveargs[P.arg_nesting]);
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
                           RlistAppend(&P.giveargs[P.arg_nesting],P.currentid, RVAL_TYPE_SCALAR);
                       }

                     | QSTRING
                       {
                           /* currently inside a use function */
                           RlistAppend(&P.giveargs[P.arg_nesting],P.currentstring, RVAL_TYPE_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | NAKEDVAR
                       {
                           /* currently inside a use function */
                           RlistAppend(&P.giveargs[P.arg_nesting],P.currentstring, RVAL_TYPE_SCALAR);
                           free(P.currentstring);
                           P.currentstring = NULL;
                       }

                     | usefunction
                       {
                           /* Careful about recursion */
                           RlistAppend(&P.giveargs[P.arg_nesting],(void *)P.currentfncall[P.arg_nesting+1], RVAL_TYPE_FNCALL);
                           RvalDestroy((Rval) { P.currentfncall[P.arg_nesting+1], RVAL_TYPE_FNCALL });
                       };

%%

/*****************************************************************/

void yyerror(const char *s)
{
    char *sp = yytext;

    if (sp == NULL)
    {
        fprintf(stderr, "%s:%d:%d: error: %s\n", P.filename, P.line_no, P.line_pos, s);
    }
    else if (*sp == '\"' && strlen(sp) > 1)
    {
        sp++;
    }

    fprintf(stderr, "%s:%d:%d: error: %s, near token \'%.20s\'\n", P.filename, P.line_no, P.line_pos, s, sp);

    ERRORCOUNT++;

    if (ERRORCOUNT > 10)
    {
        fprintf(stderr, "Too many errors");
        exit(1);
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

    fprintf(stderr, "%s: %d,%d: Fatal error during parsing: %s, near token \'%.20s\'\n", P.filename, P.line_no, P.line_pos, s, sp ? sp : "NULL");
    exit(1);
}

static void DebugBanner(const char *s)
{
    CfDebug("----------------------------------------------------------------\n");
    CfDebug("  %s                                                            \n", s);
    CfDebug("----------------------------------------------------------------\n");
}

static int RelevantBundle(const char *agent, const char *blocktype)
{
    Item *ip;

    if ((strcmp(agent, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0) || (strcmp(CF_COMMONC, blocktype) == 0))
    {
        return true;
    }

/* Here are some additional bundle types handled by cfAgent */

    ip = SplitString("edit_line,edit_xml", ',');

    if (strcmp(agent, CF_AGENTTYPES[AGENT_TYPE_AGENT]) == 0)
    {
        if (IsItemIn(ip, blocktype))
        {
            DeleteItemList(ip);
            return true;
        }
    }

    DeleteItemList(ip);
    return false;
}

static bool LvalWantsBody(char *stype, char *lval)
{
    int i, j, l;
    const SubTypeSyntax *ss;
    const BodySyntax *bs;

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ss[j].subtype != NULL; j++)
        {
            if ((bs = ss[j].bs) == NULL)
            {
                continue;
            }

            if (strcmp(ss[j].subtype, stype) != 0)
            {
                continue;
            }

            for (l = 0; bs[l].range != NULL; l++)
            {
                if (strcmp(bs[l].lval, lval) == 0)
                {
                    if (bs[l].dtype == DATA_TYPE_BODY)
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
            }
        }
    }

    return false;
}

static SyntaxTypeMatch CheckSelection(const char *type, const char *name, const char *lval, Rval rval)
{
    int lmatch = false;
    int i, j, k, l;
    const SubTypeSyntax *ss;
    const BodySyntax *bs, *bs2;
    char output[CF_BUFSIZE];

    CfDebug("CheckSelection(%s,%s,", type, lval);

    if (DEBUG)
    {
        RvalShow(stdout, rval);
    }

    CfDebug(")\n");

/* Check internal control bodies etc */

    for (i = 0; CF_ALL_BODIES[i].subtype != NULL; i++)
    {
        if (strcmp(CF_ALL_BODIES[i].subtype, name) == 0 && strcmp(type, CF_ALL_BODIES[i].bundle_type) == 0)
        {
            CfDebug("Found matching a body matching (%s,%s)\n", type, name);

            bs = CF_ALL_BODIES[i].bs;

            for (l = 0; bs[l].lval != NULL; l++)
            {
                if (strcmp(lval, bs[l].lval) == 0)
                {
                    CfDebug("Matched syntatically correct body (lval) item = (%s)\n", lval);

                    if (bs[l].dtype == DATA_TYPE_BODY)
                    {
                        CfDebug("Constraint syntax ok, but definition of body is elsewhere\n");
                        return SYNTAX_TYPE_MATCH_OK;
                    }
                    else if (bs[l].dtype == DATA_TYPE_BUNDLE)
                    {
                        CfDebug("Constraint syntax ok, but definition of bundle is elsewhere\n");
                        return SYNTAX_TYPE_MATCH_OK;
                    }
                    else
                    {
                        return CheckConstraintTypeMatch(lval, rval, bs[l].dtype, (char *) (bs[l].range), 0);
                    }
                }
            }

        }
    }

/* Now check the functional modules - extra level of indirection */

    for (i = 0; i < CF3_MODULES; i++)
    {
        CfDebug("Trying function module %d for matching lval %s\n", i, lval);

        if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ss[j].subtype != NULL; j++)
        {
            if ((bs = ss[j].bs) == NULL)
            {
                continue;
            }

            CfDebug("\nExamining subtype %s\n", ss[j].subtype);

            for (l = 0; bs[l].range != NULL; l++)
            {
                if (bs[l].dtype == DATA_TYPE_BODY)
                {
                    bs2 = (const BodySyntax *) (bs[l].range);

                    if (bs2 == NULL || bs2 == (void *) CF_BUNDLE)
                    {
                        continue;
                    }

                    for (k = 0; bs2[k].dtype != DATA_TYPE_NONE; k++)
                    {
                        /* Either module defined or common */

                        if (strcmp(ss[j].subtype, type) == 0 && strcmp(ss[j].subtype, "*") != 0)
                        {
                            snprintf(output, CF_BUFSIZE, "lval %s belongs to promise type \'%s:\' but this is '\%s\'\n",
                                     lval, ss[j].subtype, type);
                            yyerror(output);
                            return SYNTAX_TYPE_MATCH_OK;
                        }

                        if (strcmp(lval, bs2[k].lval) == 0)
                        {
                            CfDebug("Matched\n");
                            return CheckConstraintTypeMatch(lval, rval, bs2[k].dtype, (char *) (bs2[k].range), 0);
                        }
                    }
                }
            }
        }
    }

    if (!lmatch)
    {
        snprintf(output, CF_BUFSIZE, "Constraint lvalue \"%s\" is not allowed in \'%s\' constraint body", lval, type);
        yyerror(output);
    }

    return SYNTAX_TYPE_MATCH_OK;
}

static SyntaxTypeMatch CheckConstraint(const char *type, const char *lval, Rval rval, SubTypeSyntax ss)
{
    int lmatch = false;
    int i, l, allowed = false;
    const BodySyntax *bs;
    char output[CF_BUFSIZE];

    CfDebug("CheckConstraint(%s,%s,", type, lval);

    if (DEBUG)
    {
        RvalShow(stdout, rval);
    }

    CfDebug(")\n");

    if (ss.subtype != NULL)     /* In a bundle */
    {
        if (strcmp(ss.subtype, type) == 0)
        {
            CfDebug("Found type %s's body syntax\n", type);

            bs = ss.bs;

            for (l = 0; bs[l].lval != NULL; l++)
            {
                CfDebug("CMP-bundle # (%s,%s)\n", lval, bs[l].lval);

                if (strcmp(lval, bs[l].lval) == 0)
                {
                    /* If we get here we have found the lval and it is valid
                       for this subtype */

                    lmatch = true;
                    CfDebug("Matched syntatically correct bundle (lval,rval) item = (%s) to its rval\n", lval);

                    if (bs[l].dtype == DATA_TYPE_BODY)
                    {
                        CfDebug("Constraint syntax ok, but definition of body is elsewhere %s=%c\n", lval, rval.type);
                        return SYNTAX_TYPE_MATCH_OK;
                    }
                    else if (bs[l].dtype == DATA_TYPE_BUNDLE)
                    {
                        CfDebug("Constraint syntax ok, but definition of relevant bundle is elsewhere %s=%c\n", lval,
                                rval.type);
                        return SYNTAX_TYPE_MATCH_OK;
                    }
                    else
                    {
                        return CheckConstraintTypeMatch(lval, rval, bs[l].dtype, (char *) (bs[l].range), 0);
                    }
                }
            }
        }
    }

/* Now check the functional modules - extra level of indirection
   Note that we only check body attributes relative to promise type.
   We can enter any promise types in any bundle, but only recognized
   types will be dealt with. */

    for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        CfDebug("CMP-common # %s,%s\n", lval, CF_COMMON_BODIES[i].lval);

        if (strcmp(lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common constraint attributes\n", lval);
            return SYNTAX_TYPE_MATCH_OK;
        }
    }

    for (i = 0; CF_COMMON_EDITBODIES[i].lval != NULL; i++)
    {
        CfDebug("CMP-common # %s,%s\n", lval, CF_COMMON_EDITBODIES[i].lval);

        if (strcmp(lval, CF_COMMON_EDITBODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common edit_line constraint attributes\n", lval);
            return SYNTAX_TYPE_MATCH_OK;
        }
    }

    for (i = 0; CF_COMMON_XMLBODIES[i].lval != NULL; i++)
    {
        CfDebug("CMP-common # %s,%s\n", lval, CF_COMMON_XMLBODIES[i].lval);

        if (strcmp(lval, CF_COMMON_XMLBODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common edit_xml constraint attributes\n", lval);
            return SYNTAX_TYPE_MATCH_OK;
        }
    }


// Now check if it is in the common list...

    if (!lmatch || !allowed)
    {
        snprintf(output, CF_BUFSIZE, "Constraint lvalue \'%s\' is not allowed in bundle category \'%s\'", lval, type);
        yyerror(output);
    }

    return SYNTAX_TYPE_MATCH_OK;
}
