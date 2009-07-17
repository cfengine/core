
%{

/*******************************************************************/
/*                                                                 */
/*  PARSER for cfengine 3                                          */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern char *yytext;

%}

%token ID QSTRING CLASS CATEGORY BUNDLE BODY ASSIGN ARROW NAKEDVAR

%%

specification:       { yyerror("Something defined outside of a block or missing punctuation in input"); }
                     | blocks;

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
                          };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

body:                  BODY
                          {
                          DebugBanner("Body");
                          P.block = "body";
                          };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typeid:                ID {
                          P.blocktype = P.currentid;;
                          Debug("Found block type %s for %s\n",P.blocktype,P.block);
                          P.useargs = NULL;
                          };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

blockid:               ID {
                          P.blockid = P.currentid;
                          Debug("Found identifier %s for %s\n",P.currentid,P.block);
                          };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

usearglist:           '('
                      aitems
                      ')';

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitems:                 aitem           
                      | aitem ',' aitems
                      |;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

aitem:                 ID  /* recipient of argument is never a literal */
                          {
                          AppendRlist(&(P.useargs),P.currentid,CF_SCALAR);
                          free(P.currentid);
                          };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bundlebody:         '{'
                       {
                       if (RelevantBundle(THIS_AGENT,P.blocktype))
                          {
                          Debug("We a compiling everything here\n");
                          INSTALL_SKIP = false;                          
                          }
                       else if (strcmp(THIS_AGENT,P.blocktype) != 0)
                          {
                          Debug("This is for a different agent\n");
                          INSTALL_SKIP = true;
                          }
                       
                       P.currentbundle = AppendBundle(&BUNDLES,P.blockid,P.blocktype,P.useargs); 
                       P.useargs = NULL;
                       P.currenttype = NULL;
                       /* The append functions gobble these strdups, so no memory leak */
                       }
                     statements
                    '}'
                       {
                       INSTALL_SKIP = false;
                       Debug("End promise bundle\n\n");
                       };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

statements:            statement
                     | statements statement;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

statement:             category
                     | classpromises;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bodybody:            '{'
                        {
                        P.currentbody = AppendBody(&BODIES,P.blockid,P.blocktype,P.useargs);
                          
                        P.useargs = NULL;
                        P.currenttype = NULL;
                        P.currentid = NULL;
                        /* The append functions gobble these strdups, so no memory leak */
                        Debug("Starting block\n");
                        }

                      bodyattribs 

                      '}'
                        {
                        Debug("End promise body\n");
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

selection:            id                         /* BODY ONLY */
                      ASSIGN 
                      rval
                        { char *contextid = NULL;
 
                        CheckSelection(P.blocktype,P.blockid,P.lval,P.rval,P.rtype);

                        if (P.currentclasses == NULL)
                           {
                           AppendConstraint(&((P.currentbody)->conlist),P.lval,P.rval,P.rtype,"any");
                           }
                        else
                           {
                           AppendConstraint(&((P.currentbody)->conlist),P.lval,P.rval,P.rtype,P.currentclasses);
                           }

                        if (strcmp(P.blockid,"control") == 0 && strcmp(P.blocktype,"common") == 0)
                           {
                           if (strcmp(P.lval,"inputs") == 0)
                              {
                              if (IsDefinedClass(P.currentclasses))
                                 {
                                 if (VINPUTLIST == NULL)
                                    {
                                    VINPUTLIST = P.rval;
                                    }
                                 else
                                    {
                                    yyerror("Redefinition of input list (broken promise)");
                                    }
                                 }
                              }
                           }
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

category:             CATEGORY                  /* BUNDLE ONLY */
                         {
                         Debug("\n* Begin new promise type category %s in function \n\n",P.currenttype);
                                                 
                         if (strcmp(P.block,"bundle") == 0)
                            {
                            struct SubTypeSyntax ss = CheckSubType(P.blocktype,P.currenttype);
                            P.currentstype = AppendSubType(P.currentbundle,P.currenttype);
                            }
                         };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promise:              promiser                    /* BUNDLE ONLY */

                      ARROW
 
                      rval            /* This is full form with rlist promisees */
                        {
                        if (P.currentclasses == NULL)
                           {
                           P.currentpromise = AppendPromise(P.currentstype,P.promiser,P.rval,P.rtype,"any",P.blockid,P.blocktype);
                           }
                        else
                           {
                           P.currentpromise = AppendPromise(P.currentstype,P.promiser,P.rval,P.rtype,P.currentclasses,P.blockid,P.blocktype);
                           }
                        }

                      constraints ';'
                        {
                        Debug("End implicit promise %s\n\n",P.promiser);
                        P.currentid = NULL;
                        P.currentRlist = NULL;
                        P.currentstring = NULL;
                        P.promiser = NULL;
                        P.promisee = NULL;
                        /* reset argptrs etc*/
                        }

                       |

                     promiser
                        {
                        if (P.currentclasses == NULL)
                           {
                           P.currentpromise = AppendPromise(P.currentstype,P.promiser,NULL,CF_NOPROMISEE,"any",P.blockid,P.blocktype);
                           }
                        else
                           {
                           P.currentpromise = AppendPromise(P.currentstype,P.promiser,NULL,CF_NOPROMISEE,P.currentclasses,P.blockid,P.blocktype);
                           }
                        }

                     constraints ';'
                        {
                        Debug("End full promise with promisee %s\n\n",P.promiser);

                        /* Don't free these */
                        P.currentid = NULL;
                        P.currentRlist = NULL;
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

constraint:           id                        /* BUNDLE ONLY */
                      ASSIGN 
                      rval
                        { struct SubTypeSyntax ss;
                          char *contextid = NULL;

                        if (!INSTALL_SKIP)
                           {
                           ss = CheckSubType(P.blocktype,P.currenttype);                           
                           CheckConstraint(P.currenttype,P.blockid,P.lval,P.rval,P.rtype,ss);                           
                           AppendConstraint(&(P.currentpromise->conlist),P.lval,P.rval,P.rtype,"any");
                           P.rval = NULL;
                           P.lval = NULL;
                           P.currentRlist = NULL;
                           }  
                        };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class:                CLASS
                         {
                         Debug("  New class context \'%s\' :: \n\n",P.currentclasses);
                         };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

id:                    ID
                         {
                         P.lval = P.currentid;
                         P.currentRlist = NULL;
                         Debug("Recorded LVAL %s\n",P.lval);
                         };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

rval:                  ID
                         {
                         P.rval = P.currentid;
                         P.rtype = CF_SCALAR;
                         Debug("Recorded IDRVAL %s\n",P.rval);
                         }
                     | QSTRING
                         {
                         P.rval = P.currentstring;
                         P.rtype = CF_SCALAR;
                         Debug("Recorded scalarRVAL %s\n",P.rval);
                         }
                     | NAKEDVAR
                         {
                         P.rval = P.currentstring;
                         P.rtype = CF_SCALAR;
                         Debug("Recorded saclarvariableRVAL %s\n",P.rval);
                         }
                     | list
                         {
                         P.rval = P.currentRlist;
                         P.rtype = CF_LIST;
                         }
                     | usefunction
                         {
                         P.rval = P.currentfncall[P.arg_nesting+1];
                         P.rtype = CF_FNCALL;
                         };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

list:                '{'
                      litems                     
                     '}' ;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

litems:                litem            
                     | litems ',' litem
                     |;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

litem:                 ID
                          {
                          AppendRlist((struct Rlist **)&P.currentRlist,P.currentid,CF_SCALAR);
                          free(P.currentid);
                          }

                     | QSTRING
                          {
                          AppendRlist((struct Rlist **)&P.currentRlist,(void *)P.currentstring,CF_SCALAR);
                          free(P.currentstring);
                          }

                     | NAKEDVAR
                          {
                          AppendRlist((struct Rlist **)&P.currentRlist,(void *)P.currentstring,CF_SCALAR);
                          free(P.currentstring);
                          }

                     | usefunction
                          {
                          Debug("Install function call as list item from level %d\n",P.arg_nesting+1);
                          AppendRlist((struct Rlist **)&P.currentRlist,(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                          DeleteRvalItem(P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                          };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

functionid:            ID
                          {
                          Debug("Found function identifier %s\n",P.currentid);
                          };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

promiser:                QSTRING
                          {
                          P.promiser = P.currentstring; //strdup(P.currentstring);
                          Debug("Promising object name \'%s\'\n",P.promiser);
                          };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

usefunction:          functionid givearglist
                         {
                         Debug("Finished with function call, now at level %d\n\n",P.arg_nesting);
                         };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

givearglist:            '('
                           {
                           if (++P.arg_nesting > CF_MAX_NESTING)
                              {
                              yyerror("Nesting of functions is deeper than recommended");
                              }
                           P.currentfnid[P.arg_nesting] = P.currentid; //strdup(P.currentid);
                           Debug("Start FnCall %s args level %d\n",P.currentfnid[P.arg_nesting],P.arg_nesting);
                           }

                        gaitems
                        ')'
                           {
                           Debug("End args level %d\n",P.arg_nesting);
                           P.currentfncall[P.arg_nesting] = NewFnCall(P.currentfnid[P.arg_nesting],P.giveargs[P.arg_nesting]);
                           P.giveargs[P.arg_nesting] = NULL;
                           
                           P.currentid = NULL;
                           free(P.currentfnid[P.arg_nesting]);
                           P.currentfnid[P.arg_nesting] = NULL;
                           P.arg_nesting--;
                           };


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

gaitems:               gaitem            
                     | gaitems ',' gaitem 
                     |;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

gaitem:               ID
                          {
                          /* currently inside a use function */
                          AppendRlist(&P.giveargs[P.arg_nesting],P.currentid,CF_SCALAR);
                          free(P.currentid);
                          }

                     | QSTRING
                          {
                          /* currently inside a use function */
                          AppendRlist(&P.giveargs[P.arg_nesting],P.currentstring,CF_SCALAR);
                          free(P.currentstring);
                          }

                     | NAKEDVAR
                          {
                          /* currently inside a use function */
                          AppendRlist(&P.giveargs[P.arg_nesting],P.currentstring,CF_SCALAR);
                          free(P.currentstring);
                          }

                     | usefunction
                          {
                          /* Careful about recursion */
                          AppendRlist(&P.giveargs[P.arg_nesting],(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                          DeleteRvalItem(P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                          };

%%

/*****************************************************************/

void yyerror(s)

char *s;

{ char *sp = yytext;

if (*sp == '\"')
   {
   sp++;
   }

fprintf (stderr, "%s:%s:%d,%d: %s, near token \'%s\'\n",VPREFIX,P.filename,P.line_no,P.line_pos,s,sp);

ERRORCOUNT++;

if (ERRORCOUNT > 10)
   {
   FatalError("Too many errors");
   }
}

/*****************************************************************/

/* EOF */
