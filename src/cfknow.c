/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: cfknow.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

int main (int argc,char *argv[]);
void CheckOpts(int argc,char **argv);
void ThisAgentInit(void);
void KeepKnowControlPromises(void);
void KeepKnowledgePromise(struct Promise *pp);
void VerifyTopicPromise(struct Promise *pp);
void VerifyOccurrencePromises(struct Promise *pp);
void VerifyOntology(void);
void ShowOntology(void);
void ShowTopicLTM(FILE *fout,char *id,char *type,char *value);
void ShowAssociationsLTM(FILE *fout,char *id,char *type,struct TopicAssociation *ta);
void ShowOccurrencesLTM(FILE *fout,char *id,struct Occurrence *op);
char *Name2Id(char *s);

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

extern struct BodySyntax CFK_CONTROLBODY[];

enum typesequence
   {
   kp_classes,
   kp_topics,
   kp_occur,
   kp_reports,
   kp_none
   };

char *TYPESEQUENCE[] =
   {
   "classes",
   "topics",
   "occurrences",
   "reports",
   NULL
   };

struct Topic *TOPIC_MAP = NULL;

char TM_PREFIX[CF_MAXVARSIZE];
char BUILD_DIR[CF_BUFSIZE];


/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

  /* GNU STUFF FOR LATER #include "getopt.h" */
 
 struct option OPTIONS[10] =
      {
      { "help",no_argument,0,'h' },
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { "define",required_argument,0,'D' },
      { "negate",required_argument,0,'N' },
      { "file",required_argument,0,'f' },
      { "syntax",no_argument,0,'S'},
      { "parse-only",no_argument,0,'p'},
      { NULL,0,0,'\0' }
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
GenericInitialize(argc,argv,"knowledge");
ThisAgentInit();
KeepKnowControlPromises();
KeepPromiseBundles();
VerifyOntology();
ShowOntology(); // all types and assocs
return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  struct Item *actionList;
  int optindex = 0;
  int c;
  char ld_library_path[CF_BUFSIZE];

while ((c=getopt_long(argc,argv,"hd:vVf:pD:N:S",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
          break;

      case 'd': 
          AddClassToHeap("opt_debug");
          switch ((optarg==NULL) ? '3' : *optarg)
             {
             case '1':
                 D1 = true;
                 DEBUG = true;
                 break;
             case '2':
                 D2 = true;
                 DEBUG = true;
                 break;
             case '3':
                 D3 = true;
                 DEBUG = true;
                 VERBOSE = true;
                 break;
             case '4':
                 D4 = true;
                 DEBUG = true;
                 break;
             default:
                 DEBUG = true;
                 break;
             }
          break;
          
      case 'D': AddMultipleClasses(optarg);
          break;
          
      case 'N': NegateCompoundClass(optarg,&VNEGHEAP);
          break;
          
      case 'v': VERBOSE = true;
          break;
          
      case 'p': PARSEONLY = true;
          IGNORELOCK = true;
          break;          

      case 'V': Version("Knowledge agent");
          exit(0);
          
      case 'h': Syntax("Knowledge agent");
          exit(0);

      default: Syntax("Knowledge agent");
          exit(1);
          
      }
  }
}

/*****************************************************************************/

void ThisAgentInit()

{ char vbuff[CF_BUFSIZE];
 
}

/*****************************************************************************/

void KeepKnowControlPromises()

{ struct Constraint *cp;
  char rettype;
  void *retval;

for (cp = ControlBodyConstraints(cf_know); cp != NULL; cp=cp->next)
   {
   if (IsExcluded(cp->classes))
      {
      continue;
      }
   
   if (GetVariable("control_knowledge",cp->lval,&retval,&rettype) == cf_notype)
      {
      CfOut(cf_error,"","Unknown lval %s in knowledge control body",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_tm_prefix].lval) == 0)
      {
      strncpy(TM_PREFIX,retval,CF_MAXVARSIZE);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_builddir].lval) == 0)
      {
      strncpy(BUILD_DIR,retval,CF_BUFSIZE);
      continue;
      }
   }
}

/*****************************************************************************/

void KeepPromiseBundles()
    
{ struct Bundle *bp;
  struct SubType *sp;
  struct Promise *pp;
  struct Rlist *rp,*params;
  struct FnCall *fp;
  char rettype,*name;
  void *retval;
  int ok = true;
  enum typesequence type;

if (GetVariable("control_common","bundlesequence",&retval,&rettype) == cf_notype)
   {
   CfOut(cf_error,"","No bundlesequence in the common control body");
   exit(1);
   }

for (rp = (struct Rlist *)retval; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_SCALAR:
          name = (char *)rp->item;
          params = NULL;
          break;
      case CF_FNCALL:
          fp = (struct FnCall *)rp->item;
          name = (char *)fp->name;
          params = (struct Rlist *)fp->args;
          break;
          
      default:
          name = NULL;
          params = NULL;
          CfOut(cf_error,"","Illegal item found in bundlesequence: ");
          ShowRval(stdout,rp->item,rp->type);
          printf(" = %c\n",rp->type);
          ok = false;
          break;
      }
   
   if (!(GetBundle(name,"knowledge")||(GetBundle(name,"common"))))
      {
      CfOut(cf_error,"","Bundle \"%s\" listed in the bundlesequence was not found\n",name);
      ok = false;
      }
   }

if (!ok)
   {
   FatalError("Errors in knowledge bundles");
   }

/* If all is okay, go ahead and evaluate */

for (rp = (struct Rlist *)retval; rp != NULL; rp=rp->next)
   {
   switch (rp->type)
      {
      case CF_FNCALL:
          fp = (struct FnCall *)rp->item;
          name = (char *)fp->name;
          params = (struct Rlist *)fp->args;
          break;
      default:
          name = (char *)rp->item;
          params = NULL;
          break;
      }
   
   if ((bp = GetBundle(name,"knowledge")) || (bp = GetBundle(name,"common")))
      {
      BannerBundle(bp,params);
      AugmentScope(bp->name,bp->args,params);
      DeletePrivateClassContext(); // Each time we change bundle      
      }
             
   for (type = 0; TYPESEQUENCE[type] != NULL; type++)
      {
      if ((sp = GetSubTypeForBundle(TYPESEQUENCE[type],bp)) == NULL)
         {
         continue;      
         }

      BannerSubType(bp->name,sp->name);

      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         ExpandPromise(cf_agent,bp->name,pp,KeepKnowledgePromise);
         }
      }
   }
}

/*********************************************************************/

void VerifyOntology()

{ struct TopicAssociation *ta;
  struct Topic *tp;
  struct Rlist *rp;
  
for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {  
   for (ta = tp->associations; ta != NULL; ta=ta->next)
      { 
      for (rp = ta->associates; rp != NULL; rp=rp->next)
         {
         char *reverse_type = GetTopicType(TOPIC_MAP,rp->item);
         
         /* First populator sets the reverse type others must conform to */
         
         if (ta->associate_topic_type == NULL)
            {
            if (reverse_type == NULL)
               {
               CfOut(cf_error,"","Topic \"%s\" is not declared with a type",rp->item);
               continue;
               }

            if ((ta->associate_topic_type = strdup(CanonifyName(reverse_type))) == NULL)
               {
               CfOut(cferror,"malloc","Memory failure in AddTopicAssociation");
               FatalError("");
               }
            }
         
         if (ta->associate_topic_type && strcmp(ta->associate_topic_type,reverse_type) != 0)
            {
            CfOut(cf_error,"","Associates in the relationship \"%s\" do not all have the same topic type",ta->fwd_name);
            CfOut(cf_error,"","Found \"%s\" but expected \"%s\"",ta->associate_topic_type,reverse_type);
            }
         }
      }
   }
}

/*********************************************************************/

void ShowOntology()

{ struct Topic *tp;
  struct TopicAssociation *ta;
  struct Occurrence *op;
  FILE *fout = stdout;
  char id[CF_MAXVARSIZE],filename[CF_BUFSIZE];
  struct Item *generic_associations = NULL;

AddSlash(BUILD_DIR);
snprintf(filename,CF_BUFSIZE-1,"%sontology.ltm",BUILD_DIR);
  
if ((fout = fopen(filename,"w")) == NULL)
   {
   CfOut(cf_error,"fopen","Cannot write to %s\n",filename);
   return;
   }

fprintf(fout,"/*********************/\n");
fprintf(fout,"/* Class types       */\n");
fprintf(fout,"/*********************/\n\n");
   
for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   strncpy(id,CanonifyName(tp->topic_name),CF_MAXVARSIZE-1);

   if (tp->comment)
      {
      ShowTopicLTM(fout,id,tp->topic_type,tp->comment);
      }
   else
      {
      ShowTopicLTM(fout,id,tp->topic_type,tp->topic_name);
      }

   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      if (!IsItemIn(generic_associations,ta->fwd_name))
         {
         ShowAssociationsLTM(fout,NULL,NULL,ta);
         PrependItemList(&generic_associations,ta->fwd_name);            
         }
      }
   }

/* Second pass for details */

fprintf(fout,"\n/*********************/\n");
fprintf(fout,"/* Association types */\n");
fprintf(fout,"/*********************/\n\n");

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   for (ta = tp->associations; ta != NULL; ta=ta->next)
      {
      ShowAssociationsLTM(fout,tp->topic_name,tp->topic_type,ta);
      }
   }

/* Occurrences */

fprintf(fout,"\n/*********************/\n");
fprintf(fout,"/* Occurrences       */\n");
fprintf(fout,"/*********************/\n\n");

for (tp = TOPIC_MAP; tp != NULL; tp=tp->next)
   {
   strncpy(id,CanonifyName(tp->topic_name),CF_MAXVARSIZE-1);
      
   for (op = tp->occurrences; op != NULL; op=op->next)
      {
      ShowOccurrencesLTM(fout,id,op);
      }
   }

fclose(fout);
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void KeepKnowledgePromise(struct Promise *pp)

{
if (pp->done)
   {
   return;
   }

if (strcmp("classes",pp->agentsubtype) == 0)
   {
   if (!IsDefinedClass(pp->classes))
      {
      Verbose("\n");
      Verbose(". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
      Verbose("Skipping whole next promise, as context %s is not valid\n",pp->classes);
      Verbose(". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
      return;
      }

   KeepClassContextPromise(pp);
   return;
   }

if (strcmp("topics",pp->agentsubtype) == 0)
   {
   VerifyTopicPromise(pp);
   return;
   }

if (strcmp("occurrences",pp->agentsubtype) == 0)
   {
   VerifyOccurrencePromises(pp);
   return;
   }

if (strcmp("reports",pp->agentsubtype) == 0)
   {
   VerifyReportPromise(pp);
   return;
   }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void VerifyTopicPromise(struct Promise *pp)

{ char id[CF_MAXVARSIZE];
  char fwd[CF_MAXVARSIZE],bwd[CF_MAXVARSIZE];
  struct Attributes a;
  struct Topic *tp;

// Put all this in subfunc if LTM output specified

a = GetTopicsAttributes(pp);
 
strncpy(id,CanonifyName(pp->promiser),CF_MAXVARSIZE-1);

if (pp->ref != NULL)
   {
   AddCommentedTopic(&TOPIC_MAP,pp->promiser,pp->ref,pp->classes);
   }
else
   {
   AddTopic(&TOPIC_MAP,pp->promiser,pp->classes);
   }

tp = GetTopic(TOPIC_MAP,pp->promiser);

if (a.fwd_name)
   {
   AddTopicAssociation(&(tp->associations),a.fwd_name,a.bwd_name,tp->topic_type,a.associates);
   }
}

/*********************************************************************/

void VerifyOccurrencePromises(struct Promise *pp)

{ struct Attributes a;
  struct Topic *tp;
  enum representations rep_type;
  int s,e;

a = GetOccurrenceAttributes(pp);

if (a.represents == NULL)
   {
   CfOut(cf_error,"","Occurrence \"%s\" (type) promises no topics");
   return;
   }

if (a.rep_type)
   {
   rep_type = String2Representation(a.rep_type);
   }
else
   {
   rep_type = cfk_url;
   }

if (BlockTextMatch("[.&|]+",pp->classes,&s,&e))
   {
   CfOut(cf_error,"","Class should be a single topic for occurrences - %s does not make sense",pp->classes);
   return;
   }

if ((tp = GetTopic(TOPIC_MAP,pp->classes)) == NULL)
   {
   CfOut(cf_error,"","Class missing - canonical identifier \"%s\" was not previously defined so we can't map it to occurrences",pp->classes);
   return;
   }
 
AddOccurrence(&(tp->occurrences),pp->classes,pp->promiser,a.represents,rep_type);
}

/*********************************************************************/

void ShowTopicLTM(FILE *fout,char *id,char *type,char *value)

{
if (strcmp(type,"any") == 0)
   {
   fprintf(fout,"[%s = \"%s\"]\n",id,value);
   }
else
   {
   fprintf(fout,"[%s: %s = \"%s\"]\n",id,type,value);
   }
}

/*********************************************************************/

void ShowAssociationsLTM(FILE *fout,char *from_id,char *from_type,struct TopicAssociation *ta)

{ char assoc_id[CF_MAXVARSIZE];

strncpy(assoc_id,CanonifyName(ta->fwd_name),CF_MAXVARSIZE);

if (from_id == NULL)
   {
   fprintf(fout,"\n[%s = \"%s\" \n",assoc_id,ta->fwd_name);
   
   if (ta->associate_topic_type)
      {
      fprintf(fout,"          = \"%s\" / %s]\n\n",ta->bwd_name,ta->associate_topic_type);
      }
   else
      {
      fprintf(fout," = \"%s\" / unknown_association_counterpart]\n",ta->bwd_name);
      }
   }
else
   {
   struct Rlist *rp;
   char to_id[CF_MAXVARSIZE],*to_type;
   
   to_type = ta->associate_topic_type;

   for (rp = ta->associates; rp != NULL; rp=rp->next)
      {
      strncpy(to_id,CanonifyName(rp->item),CF_MAXVARSIZE);

      fprintf(fout,"%s( %s : %s, %s : %s)\n",assoc_id,CanonifyName(from_id),from_type,to_id,to_type);
      }
   }
}

/*********************************************************************/

void ShowOccurrencesLTM(FILE *fout,char *topic_id,struct Occurrence *op)

{ struct Rlist *rp;
  char subtype[CF_MAXVARSIZE];

fprintf(fout,"\n /* occurrences of %s */\n\n",topic_id);
 
for (rp = op->represents; rp != NULL; rp=rp->next)
   {
   strncpy(subtype,CanonifyName(rp->item),CF_MAXVARSIZE-1);
   fprintf(fout,"{%s,%s, \"%s\"}\n",topic_id,subtype,op->locator);
   }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

char *Name2Id(char *s)

{ static char ret[CF_BUFSIZE];

if (false)
   {
   snprintf(ret,CF_BUFSIZE,"%s_%s",TM_PREFIX,CanonifyName(s));
   }
else
   {
   snprintf(ret,CF_BUFSIZE,"%s",TM_PREFIX,CanonifyName(s));
   }
 
return ret;
}


/* EOF */




