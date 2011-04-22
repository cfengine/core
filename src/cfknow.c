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
void VerifyThingsPromise(struct Promise *pp);
void VerifyOccurrencePromises(struct Promise *pp);
void VerifyInferencePromise(struct Promise *pp);
void WriteKMDB(void);
void GenerateManual(void);
void CfGenerateStories(char *query);
void VerifyOccurrenceGroup(char *file,struct Promise *pp);
void CfQueryCFDB(char *query);

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

extern struct BodySyntax CFK_CONTROLBODY[];

enum typesequence
   {
   kp_classes,
   kp_things,
   kp_topics,
   kp_occur,
   kp_reports,
   kp_none
   };

char *TYPESEQUENCE[] =
   {
   "classes",
   "things",
   "topics",
   "occurrences",
   "reports",
   NULL
   };

char BUILD_DIR[CF_BUFSIZE];
char TOPIC_CMD[CF_MAXVARSIZE];

int HTML = false;
int WRITE_KMDB = false;
int GENERATE_MANUAL = false;
char MANDIR[CF_BUFSIZE];
int PASS;

struct Occurrence *OCCURRENCES = NULL;
struct Inference *INFERENCES = NULL;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

 char *ID = "The knowledge management agent is capable of building\n"
            "and analysing a semantic knowledge network. It can\n"
            "configure a relational database to contain an ISO\n"
            "standard topic map and permit regular-expression based\n"
            "searching of the map. Analysis of the semantic network\n"
            "can be performed providing graphical output of the data,\n"
            "and cf-know can assemble and converge the reference manual\n"
            "for the current version of the Cfengine software.";
 
 struct option OPTIONS[12] =
      {
      { "help",no_argument,0,'h' },
      { "build",no_argument,0,'b'},
      { "debug",optional_argument,0,'d' },
      { "verbose",no_argument,0,'v' },
      { "version",no_argument,0,'V' },
      { "file",required_argument,0,'f' },
      { "manual",no_argument,0,'m'},
      { "manpage",no_argument,0,'M'},
      { "query_cfdb",required_argument,0,'Q'},
      { "stories",required_argument,0,'s'},
      { "syntax",required_argument,0,'S'},
      { NULL,0,0,'\0' }
      };

 char *HINTS[12] =
      {
      "Print the help message",
      "Build and store topic map in the CFDB",
      "Set debugging level 0,1,2,3",
      "Output verbose information about the behaviour of the agent",
      "Output the version of the software",
      "Specify an alternative input file than the default",
      "Generate reference manual from internal data",
      "Generate reference manpage from internal data",
      "Query the CFDB for testing, etc",
      "Look up stories for a given topic on the command line",
      "Print a syntax summary of the optional keyword or this cfengine version",
      NULL
      };

/*****************************************************************************/

int main(int argc,char *argv[])

{
CheckOpts(argc,argv); 
GenericInitialize(argc,argv,"knowledge");
ThisAgentInit();
KeepKnowControlPromises();

if (strlen(TOPIC_CMD) == 0)
   {
   int complete;
   double percent;
   KeepPromiseBundles();
   WriteKMDB();
   GenerateManual();
   
   complete = (double)CF_NODES*(CF_NODES-1);
   percent = 100.0 * (double)CF_EDGES/(double)complete;
   CfOut(cf_inform,""," -> Complexity of knowledge model yields %d/%d = %.4lf%%\n",CF_EDGES,complete,percent);
   }

return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

void CheckOpts(int argc,char **argv)

{ extern char *optarg;
  char arg[CF_BUFSIZE];
  int optindex = 0;
  int c;

LOOKUP = false;

while ((c=getopt_long(argc,argv,"hbd:vVf:mMQ:s:S",OPTIONS,&optindex)) != EOF)
  {
  switch ((char) c)
      {
      case 'f':

          if (optarg && strlen(optarg) < 5)
             {
             snprintf(arg,CF_MAXVARSIZE," -f used but argument \"%s\" incorrect",optarg);
             FatalError(arg);
             }

          strncpy(VINPUTFILE,optarg,CF_BUFSIZE-1);
          VINPUTFILE[CF_BUFSIZE-1] = '\0';
          MINUSF = true;
          break;

      case 'd': 

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
             default:
                 DEBUG = true;
                 break;
             }
          break;

      case 'Q':
          strcpy(TOPIC_CMD,optarg);
          CfQueryCFDB(TOPIC_CMD);
          exit(0);
          break;

      case 's':
#ifdef HAVE_CONSTELLATION
          strcpy(TOPIC_CMD,optarg);
          CfGenerateStories(TOPIC_CMD);
#endif
          exit(0);
          break;
          
      case 'b':
          WRITE_KMDB = true;
          break;
          
      case 'v':
          VERBOSE = true;
          break;
          
      case 'V':
          Version("cf-know");
          exit(0);
          
      case 'h':
          Syntax("cf-know - cfengine's knowledge agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'M':
          ManPage("cf-know - cfengine's knowledge agent",OPTIONS,HINTS,ID);
          exit(0);

      case 'H':
          HTML = 1;
          break;

      case 'S':
          if (optarg)
             {
             SyntaxCompletion(optarg);
             exit(0);
             }
          break;

      case 'm':
          GENERATE_MANUAL = true;
          break;
          
      default: Syntax("cf-know - knowledge agent",OPTIONS,HINTS,ID);
          exit(1);
          
      }
  }

if (argv[optind] != NULL)
   {
   CfOut(cf_error,"","Unexpected argument with no preceding option: %s\n",argv[optind]);
   }

}

/*****************************************************************************/

void ThisAgentInit()

{ 
strcpy(WEBDRIVER,"");
strcpy(LICENSE_COMPANY,"");
strcpy(MANDIR,".");
strcpy(SQL_DATABASE,"cf_kmdb");
strcpy(GRAPHDIR,"");
SHOWREPORTS = false;

PrependRScalar(&GOALS,"goal.*",CF_SCALAR);
PrependRScalar(&GOALCATEGORIES,"goals",CF_SCALAR);

if (InsertTopic("any","any"))
   {
   struct Rlist *list = NULL;
   PrependRScalar(&list,"Description",CF_SCALAR);
   AddOccurrence(&OCCURRENCES,"The generic knowledge context - any time, any place, anywhere",list,cfk_literal,"any");
   DeleteRlist(list);
   }

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
      CfOut(cf_error,""," !! Unknown lval %s in knowledge control body",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_tm_prefix].lval) == 0)
      {
      CfOut(cf_error,"","The topic map prefix has been deprecated");
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_builddir].lval) == 0)
      {
      strncpy(BUILD_DIR,retval,CF_BUFSIZE);

      if (strlen(MANDIR) < 2) /* MANDIR defaults to BUILDDIR */
         {
         strncpy(MANDIR,retval,CF_BUFSIZE);
         }
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_goalpatterns].lval) == 0)
      {
      GOALS = (struct Rlist *)retval;
      CfOut(cf_verbose,"","SET goal_patterns list\n");
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_goalcategories].lval) == 0)
      {
      GOALCATEGORIES = (struct Rlist *)retval;
      CfOut(cf_verbose,"","SET goal_categories list\n");
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_type].lval) == 0)
      {
      SQL_TYPE = Str2dbType(retval);
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_database].lval) == 0)
      {
      strncpy(SQL_DATABASE,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_owner].lval) == 0)
      {
      strncpy(SQL_OWNER,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }
      
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_passwd].lval) == 0)
      {
      strncpy(SQL_PASSWD,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_server].lval) == 0)
      {
      strncpy(SQL_SERVER,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_sql_connect_db].lval) == 0)
      {
      strncpy(SQL_CONNECT_NAME,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_query_engine].lval) == 0)
      {
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_htmlbanner].lval) == 0)
      {
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_htmlfooter].lval) == 0)
      {
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_stylesheet].lval) == 0)
      {
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_query_output].lval) == 0)
      {
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_graph_output].lval) == 0)
      {
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_views].lval) == 0)
      {
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_graph_dir].lval) == 0)
      {
      strncpy(GRAPHDIR,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,"","SET graph_directory = %s\n",GRAPHDIR);
      continue;
      }
   
   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_genman].lval) == 0)
      {
      GENERATE_MANUAL = GetBoolean(retval);
      CfOut(cf_verbose,"","SET generate_manual = %d\n",GENERATE_MANUAL);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_mandir].lval) == 0)
      {
      strncpy(MANDIR,retval,CF_MAXVARSIZE);
      CfOut(cf_verbose,"","SET manual_source_directory = %s\n",MANDIR);
      continue;
      }

   if (strcmp(cp->lval,CFK_CONTROLBODY[cfk_docroot].lval) == 0)
      {
      CfOut(cf_verbose,""," -> Option %s has been deprecated in this release",cp->lval);
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
   CfOut(cf_error,""," !! No bundlesequence in the common control body");
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
          CfOut(cf_error,""," !! Illegal item found in bundlesequence: ");
          ShowRval(stdout,rp->item,rp->type);
          printf(" = %c\n",rp->type);
          ok = false;
          break;
      }
   
   if (!(GetBundle(name,"knowledge")||(GetBundle(name,"common"))))
      {
      CfOut(cf_error,""," !! Bundle \"%s\" listed in the bundlesequence was not found\n",name);
      ok = false;
      }
   }

if (!ok)
   {
   FatalError("Errors in knowledge bundles");
   }

/* If all is okay, go ahead and evaluate */

PASS = 1;

for (type = 0; TYPESEQUENCE[type] != NULL; type++)
   {
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
      
      if ((sp = GetSubTypeForBundle(TYPESEQUENCE[type],bp)) == NULL)
         {
         continue;      
         }
      
      BannerSubType(bp->name,sp->name,1);
      
      for (pp = sp->promiselist; pp != NULL; pp=pp->next)
         {
         ExpandPromise(cf_know,bp->name,pp,KeepKnowledgePromise);
         }      
      }
   }
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
   CfOut(cf_verbose,""," ! Class promises do not have any effect here.\n");
   return;
   }

if (strcmp("inferences",pp->agentsubtype) == 0)
   {
   VerifyInferencePromise(pp);
   return;
   }

if (strcmp("things",pp->agentsubtype) == 0)
   {
   VerifyThingsPromise(pp);
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

void CfQueryCFDB(char *query)
{
#ifdef HAVE_NOVA
Nova_CfQueryCFDB(query);
#endif
}

/*********************************************************************/

void CfGenerateStories(char *query)
{
#ifdef HAVE_CONSTELLATION
Constellation_CfGenerateStories(query);
#endif
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void VerifyInferencePromise(struct Promise *pp)

{ struct Attributes a = {0};
 struct Rlist *rpp,*rpq;

if (!IsDefinedClass(pp->classes))
   {
   CfOut(cf_verbose,""," -> Skipping inference for \"%s\" as class \"%s\" is not defined",pp->promiser,pp->classes);
   return;
   }
 
a = GetInferencesAttributes(pp);

for (rpp = a.precedents; rpp != NULL; rpp=rpp->next)
   {
   for (rpq = a.qualifiers; rpq != NULL; rpq=rpq->next)
      {
      CfOut(cf_verbose,""," -> Add inference: (%s,%s,%s)\n",rpp->item,rpq->item,pp->promiser);
      AddInference(&INFERENCES,pp->promiser,rpp->item,rpq->item);
      }
   }
}

/*********************************************************************/

void VerifyThingsPromise(struct Promise *pp)

{ char id[CF_BUFSIZE];
  struct Attributes a = {0};
  struct Topic *tp = NULL, *otp;
  struct Rlist *rp,*rps,*contexts;
  char *handle = (char *)GetConstraint("handle",pp,CF_SCALAR);

a = GetThingsAttributes(pp);

CfOut(cf_verbose,""," -> Attempting to install thing-topic %s::%s \n",pp->classes,pp->promiser);

// Add a standard reserved word

contexts = SplitContextExpression(pp->classes,pp);

for (rp = contexts; rp != NULL; rp = rp->next)
   {
   if ((tp = InsertTopic(pp->promiser,rp->item)) == NULL)
      {
      return;
      }

   CfOut(cf_verbose,""," -> New thing \"%s\" about context \"%s\"",pp->promiser,rp->item);

   if (a.fwd_name && a.bwd_name)
      {
      CfOut(cf_verbose,""," -> New thing \"%s\" has a relation \"%s/%s\"",pp->promiser,a.fwd_name,a.bwd_name);
      AddTopicAssociation(tp,&(tp->associations),a.fwd_name,a.bwd_name,a.associates,true);
      }

   // Handle all synonyms as associations
   
   if (handle)
      {
      char synonym[CF_BUFSIZE];
      snprintf(synonym,CF_BUFSIZE-1,"handles::%s",handle);
      otp = IdempInsertTopic(synonym);
      PrependRScalar(&(a.synonyms),otp->topic_name,CF_SCALAR);
      }
   
   // Handle all synonyms as associations
   
   if (a.synonyms)
      {
      for (rps = a.general; rps != NULL; rps=rps->next)
         {
         otp = IdempInsertTopic(rps->item);
         }
      
      AddTopicAssociation(tp,&(tp->associations),KM_SYNONYM,KM_SYNONYM,a.synonyms,true);
      }

   // Handle all generalizations as associations

   if (a.general)
      {
      for (rps = a.general; rps != NULL; rps=rps->next)
         {
         otp = IdempInsertTopic(rps->item);
         }
      
      AddTopicAssociation(tp,&(tp->associations),KM_GENERALIZES_F,KM_GENERALIZES_B,a.general,true);
      }

   // Treat comments as occurrences of information.

   if (pp->ref)
      {
      struct Rlist *list = NULL;
      snprintf(id,CF_MAXVARSIZE,"%s.%s",pp->classes,CanonifyName(pp->promiser));
      PrependRScalar(&list,"description",CF_SCALAR);
      AddOccurrence(&OCCURRENCES,pp->ref,list,cfk_literal,id);
      DeleteRlist(list);
      }
   
   if (handle)
      {
      struct Rlist *list = NULL;
      PrependRScalar(&list,handle,CF_SCALAR);
      AddTopicAssociation(tp,&(tp->associations),"is the promise of","stands for",list,true);
      DeleteRlist(list);
      list = NULL;
      snprintf(id,CF_MAXVARSIZE,"%s.%s",pp->classes,handle);
      PrependRScalar(&list,"description",CF_SCALAR);
      AddOccurrence(&OCCURRENCES,pp->ref,list,cfk_literal,id);
      DeleteRlist(list);
      }
   }

DeleteRlist(contexts);
}

/*********************************************************************/

void VerifyTopicPromise(struct Promise *pp)

{ char id[CF_BUFSIZE];
  struct Attributes a = {0};
  struct Topic *tp = NULL, *otp;
  struct Rlist *rp,*rps,*contexts;
  char *handle = (char *)GetConstraint("handle",pp,CF_SCALAR);

a = GetTopicsAttributes(pp);

CfOut(cf_verbose,""," -> Attempting to install topic %s::%s \n",pp->classes,pp->promiser);

// Add a standard reserved word

contexts = SplitContextExpression(pp->classes,pp);

for (rp = contexts; rp != NULL; rp = rp->next)
   {
   if ((tp = InsertTopic(pp->promiser,rp->item)) == NULL)
      {
      return;
      }

   CfOut(cf_verbose,""," -> New topic promise for \"%s\" about context \"%s\"",pp->promiser,rp->item);
   
   if (a.fwd_name && a.bwd_name)
      {
      AddTopicAssociation(tp,&(tp->associations),a.fwd_name,a.bwd_name,a.associates,true);
      }

   // Handle all synonyms as associations

   if (a.synonyms)
      {
      for (rps = a.general; rps != NULL; rps=rps->next)
         {
         otp = IdempInsertTopic(rps->item);
         }
      
      AddTopicAssociation(tp,&(tp->associations),KM_SYNONYM,KM_SYNONYM,a.synonyms,true);
      }

   // Handle all generalizations as associations

   if (a.general)
      {
      for (rps = a.general; rps != NULL; rps=rps->next)
         {
         otp = IdempInsertTopic(rps->item);
         }
      
      AddTopicAssociation(tp,&(tp->associations),KM_GENERALIZES_F,KM_GENERALIZES_B,a.general,true);
      }
   
   if (handle)
      {
      char synonym[CF_BUFSIZE];
      snprintf(synonym,CF_BUFSIZE-1,"handles::%s",handle);
      otp = IdempInsertTopic(synonym);
      PrependRScalar(&(a.synonyms),otp->topic_name,CF_SCALAR);
      }
   
   // Treat comments as occurrences of information.

   if (pp->ref)
      {
      struct Rlist *list = NULL;
      snprintf(id,CF_MAXVARSIZE,"%s.%s",pp->classes,CanonifyName(pp->promiser));
      PrependRScalar(&list,"description",CF_SCALAR);
      AddOccurrence(&OCCURRENCES,pp->ref,list,cfk_literal,id);
      DeleteRlist(list);
      }
   
   if (handle)
      {
      struct Rlist *list = NULL;
      PrependRScalar(&list,handle,CF_SCALAR);
      AddTopicAssociation(tp,&(tp->associations),"is the promise of","stands for",list,true);
      DeleteRlist(list);
      list = NULL;
      snprintf(id,CF_MAXVARSIZE,"%s.%s",pp->classes,handle);
      PrependRScalar(&list,"description",CF_SCALAR);
      AddOccurrence(&OCCURRENCES,pp->ref,list,cfk_literal,id);
      DeleteRlist(list);
      }
   }

DeleteRlist(contexts);
}

/*********************************************************************/

void VerifyOccurrencePromises(struct Promise *pp)

{ struct Attributes a = {0};
  char name[CF_BUFSIZE];
  enum representations rep_type;
  struct Rlist *contexts,*rp;

a = GetOccurrenceAttributes(pp);

if (a.rep_type)
   {
   rep_type = String2Representation(a.rep_type);
   }
else
   {
   rep_type = cfk_url;
   }

if (a.represents == NULL)
   {
   if (rep_type == cfk_literal)
      {
      CfOut(cf_error,""," ! Occurrence of text information \"%s\" does not promise any topics to represent",pp->promiser);
      }
   else
      {
      CfOut(cf_error,""," ! Occurrence or reference to information \"%s\" does not promise any topics to represent",pp->promiser);
      }
   return;
   }

contexts = SplitContextExpression(pp->classes,pp);

for (rp = contexts; rp != NULL; rp = rp->next)
   {
   CfOut(cf_verbose,""," -> New occurrence promise for \"%s\" about context \"%s\"",pp->promiser,rp->item);
   
   switch (rep_type)
      {
      case cfk_file:
          
          if (a.web_root == NULL || a.path_root == NULL)
             {
             CfOut(cf_error,""," !! File pattern but no complete url mapping path_root -> web_root");
             return;
             }
          
          strncpy(name,a.path_root,CF_BUFSIZE-1);
          
          if (!JoinPath(name,pp->promiser))
             {
             CfOut(cf_error,""," !! Unable to form pathname in search for local files");
             return;
             }

          // FIXME - this should pass rp->item instead of pp->classes if we want to keep this
          
          LocateFilePromiserGroup(name,pp,VerifyOccurrenceGroup);
          break;
          
      default:
          
          AddOccurrence(&OCCURRENCES,pp->promiser,a.represents,rep_type,rp->item);
          break;
      }
   }

DeleteRlist(contexts);
}

/*********************************************************************/

void WriteKMDB()

{
#ifdef HAVE_NOVA 
if (WRITE_KMDB)
   {
   Nova_StoreKMDB(TOPICHASH,OCCURRENCES,INFERENCES);
   }
#endif
}
/*********************************************************************/

void GenerateManual()

{
if (GENERATE_MANUAL)
   {
   TexinfoManual(MANDIR);
   }
}

/*********************************************************************/

void VerifyOccurrenceGroup(char *file,struct Promise *pp)
    
{ struct Attributes a = {0};
  enum representations rep_type;
  struct stat sb;
  char *sp,url[CF_BUFSIZE];
  struct Rval retval;

a = GetOccurrenceAttributes(pp);

if (a.rep_type)
   {
   rep_type = String2Representation(a.rep_type);
   }
else
   {
   rep_type = cfk_url;
   }

if (cfstat(file,&sb) == -1)
   {
   CfOut(cf_verbose,""," !! File %s matched but could not be read",file);
   return;
   }

if (a.path_root == NULL || a.web_root == NULL)
   {
   CfOut(cf_error,""," !! No pathroot/webroot defined in representation");
   PromiseRef(cf_error,pp);
   return;
   }

Chop(a.path_root);
DeleteSlash(a.path_root);
sp = file + strlen(a.path_root) + 1;

FullTextMatch(pp->promiser,sp);
retval = ExpandPrivateRval("this",a.represents,CF_LIST);
DeleteScope("match");

if (strlen(a.web_root) > 0)
   {
   snprintf(url,CF_BUFSIZE-1,"%s/%s",a.web_root,sp);
   }
else
   {
   snprintf(url,CF_BUFSIZE-1,"%s",sp);
   }

AddOccurrence(&OCCURRENCES,url,retval.item,cfk_url,pp->classes);
CfOut(cf_verbose,""," -> File %s matched and being logged at %s",file,url);

DeleteRlist((struct Rlist *)retval.item);
}

 
