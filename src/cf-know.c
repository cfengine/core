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

#include "generic_agent.h"
#include "cf3.extern.h"

#include "files_names.h"
#include "ontology.h"
#include "export_xml.h"

static void ThisAgentInit(void);
static GenericAgentConfig CheckOpts(int argc, char **argv);

static void KeepKnowControlPromises(void);
static void KeepKnowledgePromise(Promise *pp);
static void VerifyTopicPromise(Promise *pp);
static void VerifyThingsPromise(Promise *pp);
static void VerifyOccurrencePromises(Promise *pp);
static void VerifyInferencePromise(Promise *pp);
static void WriteKMDB(void);
static void GenerateManual(void);

#ifdef HAVE_CONSTELLATION
static void CfGenerateStories(char *query, enum storytype type);
#endif
static void CfGenerateTestData(int count);
static void CfRemoveTestData(void);
static void CfUpdateTestData(void);
static void ShowSingletons(void);
static void ShowWords(void);
static void GenerateXml(void);
static char *NormalizeTopic(char *s);
static void AddInference(Inference **list, char *result, char *pre, char *qual);
static Topic *IdempInsertTopic(char *classified_name);
static Topic *InsertTopic(char *name, char *context);
static Topic *AddTopic(Topic **list, char *name, char *type);
static void AddTopicAssociation(Topic *tp, TopicAssociation **list, char *fwd_name, char *bwd_name, Rlist *li, int ok,
                                char *from_context, char *from_topic);
static void AddOccurrence(Occurrence **list, char *reference, Rlist *represents, enum representations rtype,
                          Rlist *add_topics, char *context);
static Topic *TopicExists(char *topic_name, char *topic_type);
static TopicAssociation *AssociationExists(TopicAssociation *list, char *fwd, char *bwd);
static Occurrence *OccurrenceExists(Occurrence *list, char *locator, enum representations repy_type, char *s);
static void KeepPromiseBundles(void);
int GetTopicPid(char *classified_topic);

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

static Topic *TOPICHASH[CF_HASHTABLESIZE];

int GLOBAL_ID = 1;              // Used as a primary key for convenience, 0 reserved
static int BGOALS = false;

extern BodySyntax CFK_CONTROLBODY[];

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

int WORDS = false;
int HTML = false;
int WRITE_KMDB = false;
int GENERATE_MANUAL = false;
int GENERATE_XML = false;
static char *OUTPUT_FILE;
char MANDIR[CF_BUFSIZE];
char STORY[CF_BUFSIZE];
char FINDTOPIC[CF_BUFSIZE];

Occurrence *OCCURRENCES = NULL;
Inference *INFERENCES = NULL;

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The knowledge management agent is capable of building\n"
    "and analysing a semantic knowledge network. It can\n"
    "configure a relational database to contain an ISO\n"
    "standard topic map and permit regular-expression based\n"
    "searching of the map. Analysis of the semantic network\n"
    "can be performed providing graphical output of the data,\n"
    "and cf-know can assemble and converge the reference manual\n" "for the current version of the Cfengine software.";

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"build", no_argument, 0, 'b'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"file", required_argument, 0, 'f'},
    {"goals", no_argument, 0, 'g'},
    {"inform", no_argument, 0, 'I'},
    {"lookup", required_argument, 0, 'l'},
    {"manual", no_argument, 0, 'm'},
    {"manpage", no_argument, 0, 'M'},
    {"tell-me-about", required_argument, 0, 'z'},
    {"syntax", required_argument, 0, 'S'},
    {"topics", no_argument, 0, 'T'},
    {"test", required_argument, 0, 't'},
    {"removetest", no_argument, 0, 'r'},
    {"updatetest", no_argument, 0, 'u'},
    {"xml", no_argument, 0, 'x'},
    {"output-file", required_argument, 0, 'o'},
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[] =
{
    "Print the help message",
    "Build and store topic map in the CFDB",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    "Specify an alternative input file than the default",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Print JSON output about a possibly qualified context::topic",
    "lookup",
    "Generate reference manual from internal data",
    "Generate reference manpage from internal data",
    "Look up stories for a given topic on the command line (use \"any\" to list possible)",
    "Print a syntax summary of the optional keyword or this cfengine version",
    "Show all topic names in CFEngine",
    "Generate test data",
    "Remove test data",
    "Update test data",
    "Generate documentation in XML format",
    "Output file for XML documentation",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig config = CheckOpts(argc, argv);

    ARGV0 = xstrdup(basename(argv[0]));

    GenericInitialize("knowledge", config);
    ThisAgentInit();

    KeepKnowControlPromises();

    if (BGOALS)
    {
#ifdef HAVE_NOVA
# ifdef HAVE_LIBMONGOC
    char buffer[CF_BUFSIZE], *sp, name[CF_BUFSIZE],desc[CF_BUFSIZE], *end;
    
    Nova_GetUniqueBusinessGoals(buffer, CF_BUFSIZE);

    end = buffer + strlen(buffer);
    
    for (sp = strstr(buffer,"desc"); sp < end && sp != NULL; sp = strstr(sp,"desc"))
       {
       desc[0] = '\0';
       name[0] = '\0';
       sscanf(sp+strlen("desc")+2,"%*[ \"]%[^\"]",desc);
       sp = strstr(sp+strlen(desc),"name");
       sscanf(sp+strlen("name")+2,"%*[ \"]%[^\"]",name);
       printf("%s => %s\n",name,desc);
       }
    return 0;
# endif
#endif    
    }
    
    if (strlen(STORY) > 0)
    {
#ifdef HAVE_CONSTELLATION
       if (strncmp(STORY, "SHA=", 4) == 0)
       {
          char buffer[CF_BUFSIZE];
          
                Constellation_HostStory(STORY, buffer, CF_BUFSIZE);
                printf("%s\n", buffer);
       }
       else
       {
          strcpy(TOPIC_CMD, STORY);
          
          printf("Let's start with stories about cause-effect:\n\n");
          CfGenerateStories(TOPIC_CMD, cfi_cause);
          printf("Now looking for stories about connections between things:\n\n");
          CfGenerateStories(TOPIC_CMD, cfi_connect);
          printf("Anything about structure:\n\n");
          CfGenerateStories(TOPIC_CMD, cfi_part);
       }
#endif
       exit(0);
            
    }
    else if (strlen(FINDTOPIC) > 0)
    {
#ifdef HAVE_NOVA
# ifdef HAVE_LIBMONGOC
         Nova_ShowTopic(FINDTOPIC);
# endif
         exit(0);
#endif    
    }
    
    if (GENERATE_XML)
    {
        GenerateXml();
    }
    else if (strlen(TOPIC_CMD) == 0)
    {
        int complete;
        double percent;

        KeepPromiseBundles();
        WriteKMDB();
        GenerateManual();
        ShowWords();
        ShowSingletons();

        complete = (double) CF_TOPICS *(CF_TOPICS - 1);

        percent = 100.0 * (double) CF_EDGES / (double) complete;
        CfOut(cf_inform, "", " -> Association density yields %d/%d = %.4lf%%\n", CF_EDGES, complete, percent);
        percent = 100.0 * (double) CF_OCCUR / (double) CF_TOPICS;
        CfOut(cf_inform, "", " -> Hit probability (efficiency) yields %d/%d = %.4lf%%\n", CF_OCCUR, CF_TOPICS, percent);
    }

    return 0;
}

/*****************************************************************************/
/* Level 1                                                                   */
/*****************************************************************************/

static GenericAgentConfig CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    GenericAgentConfig config = GenericAgentDefaultConfig(cf_know);

    LOOKUP = false;

    while ((c = getopt_long(argc, argv, "gIhbdvVf:mxMz:St:ruTl:", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'f':

            if (optarg && strlen(optarg) < 5)
            {
                FatalError(" -f used but argument \"%s\" incorrect", optarg);
            }

            SetInputFile(optarg);
            MINUSF = true;
            break;

        case 'd':
            DEBUG = true;
            break;

        case 'I':
            INFORM = true;
            break;

        case 'z':
            if (optarg)
               {
               strncpy(STORY,optarg,CF_BUFSIZE-1);
               }
            break;

        case 'g':
            BGOALS = true;
            break;

        case 'l':
            if (optarg)
               {
               strncpy(FINDTOPIC,optarg,CF_BUFSIZE-1);
               }
            break;
            
        case 'b':
            WRITE_KMDB = true;
            break;

        case 'v':
            VERBOSE = true;
            break;

        case 'V':
            PrintVersionBanner("cf-know");
            exit(0);

        case 'h':
            Syntax("cf-know - cfengine's knowledge agent", OPTIONS, HINTS, ID);
            exit(0);

        case 'M':
            ManPage("cf-know - cfengine's knowledge agent", OPTIONS, HINTS, ID);
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

        case 'x':
            GENERATE_XML = true;
            break;

        case 'o':
            OUTPUT_FILE = optarg;
            break;

        case 't':
            if (atoi(optarg))
            {
                CfGenerateTestData(atoi(optarg));
                exit(0);
            }
            break;

        case 'r':
            CfRemoveTestData();
            exit(0);

        case 'u':
            CfUpdateTestData();
            exit(0);

        case 'T':
            WORDS = true;
            break;

        default:
            Syntax("cf-know - knowledge agent", OPTIONS, HINTS, ID);
            exit(1);
        }
    }

    if (argv[optind] != NULL)
    {
        CfOut(cf_error, "", "Unexpected argument with no preceding option: %s\n", argv[optind]);
    }

    return config;
}

/*****************************************************************************/

static void ThisAgentInit(void)
{
    strcpy(WEBDRIVER, "");
    strcpy(LICENSE_COMPANY, "");
    strcpy(MANDIR, ".");
    SHOWREPORTS = false;
}

/*****************************************************************************/

static void KeepKnowControlPromises()
{
    Constraint *cp;
    Rval retval;

    for (cp = ControlBodyConstraints(cf_know); cp != NULL; cp = cp->next)
    {
        if (IsExcluded(cp->classes))
        {
            continue;
        }

        if (GetVariable("control_knowledge", cp->lval, &retval) == cf_notype)
        {
            CfOut(cf_error, "", " !! Unknown lval %s in knowledge control body", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_tm_prefix].lval) == 0)
        {
            CfOut(cf_error, "", "The topic map prefix has been deprecated");
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_builddir].lval) == 0)
        {
            strncpy(BUILD_DIR, retval.item, CF_BUFSIZE);

            if (strlen(MANDIR) < 2)     /* MANDIR defaults to BUILDDIR */
            {
                strncpy(MANDIR, retval.item, CF_BUFSIZE);
            }
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_sql_type].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_sql_database].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_sql_owner].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_sql_passwd].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_sql_server].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_sql_connect_db].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_query_engine].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_htmlbanner].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_htmlfooter].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_stylesheet].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_query_output].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_graph_output].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_views].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_genman].lval) == 0)
        {
            GENERATE_MANUAL = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET generate_manual = %d\n", GENERATE_MANUAL);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_mandir].lval) == 0)
        {
            strncpy(MANDIR, retval.item, CF_MAXVARSIZE);
            CfOut(cf_verbose, "", "SET manual_source_directory = %s\n", MANDIR);
            continue;
        }

        if (strcmp(cp->lval, CFK_CONTROLBODY[cfk_docroot].lval) == 0)
        {
            CfOut(cf_verbose, "", " -> Option %s has been deprecated in this release", cp->lval);
            continue;
        }
    }
}

/*****************************************************************************/

static void KeepPromiseBundles()
{
    Bundle *bp;
    SubType *sp;
    Promise *pp;
    Rlist *rp, *params;
    FnCall *fp;
    char *name;
    Rval retval;
    int ok = true;
    enum typesequence type;

    if (GetVariable("control_common", "bundlesequence", &retval) == cf_notype)
    {
        CfOut(cf_error, "", " !! No bundlesequence in the common control body");
        exit(1);
    }

    for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:
            name = (char *) rp->item;
            params = NULL;
            break;
        case CF_FNCALL:
            fp = (FnCall *) rp->item;
            name = (char *) fp->name;
            params = (Rlist *) fp->args;
            break;

        default:
            name = NULL;
            params = NULL;
            CfOut(cf_error, "", " !! Illegal item found in bundlesequence: ");
            ShowRval(stdout, (Rval) {rp->item, rp->type});
            printf(" = %c\n", rp->type);
            ok = false;
            break;
        }

        if (!(GetBundle(name, "knowledge") || (GetBundle(name, "common"))))
        {
            CfOut(cf_error, "", " !! Bundle \"%s\" listed in the bundlesequence was not found\n", name);
            ok = false;
        }
    }

    if (!ok)
    {
        FatalError("Errors in knowledge bundles");
    }

/* If all is okay, go ahead and evaluate */

    for (type = 0; TYPESEQUENCE[type] != NULL; type++)
    {
        for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
        {
            switch (rp->type)
            {
            case CF_FNCALL:
                fp = (FnCall *) rp->item;
                name = (char *) fp->name;
                params = (Rlist *) fp->args;
                break;
            default:
                name = (char *) rp->item;
                params = NULL;
                break;
            }

            if ((bp = GetBundle(name, "knowledge")) || (bp = GetBundle(name, "common")))
            {
                BannerBundle(bp, params);
                AugmentScope(bp->name, bp->args, params);
                DeletePrivateClassContext();    // Each time we change bundle      
            }

            if ((sp = GetSubTypeForBundle(TYPESEQUENCE[type], bp)) == NULL)
            {
                continue;
            }

            BannerSubType(bp->name, sp->name, 1);

            for (pp = sp->promiselist; pp != NULL; pp = pp->next)
            {
                ExpandPromise(cf_know, bp->name, pp, KeepKnowledgePromise);
            }
        }
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static void KeepKnowledgePromise(Promise *pp)
{
    if (pp->done)
    {
        return;
    }

    if (strcmp("classes", pp->agentsubtype) == 0)
    {
        CfOut(cf_verbose, "", " ! Class promises do not have any effect here.\n");
        return;
    }

    if (strcmp("inferences", pp->agentsubtype) == 0)
    {
        VerifyInferencePromise(pp);
        return;
    }

    if (strcmp("things", pp->agentsubtype) == 0)
    {
        VerifyThingsPromise(pp);
        return;
    }

    if (strcmp("topics", pp->agentsubtype) == 0)
    {
        VerifyTopicPromise(pp);
        return;
    }

    if (strcmp("occurrences", pp->agentsubtype) == 0)
    {
        VerifyOccurrencePromises(pp);
        return;
    }

    if (strcmp("reports", pp->agentsubtype) == 0)
    {
        VerifyReportPromise(pp);
        return;
    }
}

/*********************************************************************/

#ifdef HAVE_CONSTELLATION
void CfGenerateStories(char *query, enum storytype type)
{
    Constellation_GenerateStoriesCmdLine(query, type);
}
#endif

/*********************************************************************/

void CfGenerateTestData(int count)
{
#ifdef HAVE_NOVA
    Nova_GenerateTestData(count);
#endif
}

/*********************************************************************/

void CfUpdateTestData(void)
{
#ifdef HAVE_NOVA
    Nova_UpdateTestData();
#endif
}

/*********************************************************************/

void CfRemoveTestData(void)
{
#ifdef HAVE_NOVA
    Nova_RemoveTestData();
#endif
}

/*********************************************************************/

void ShowWords()
{
    Topic *tp;
    Item *ip, *list = NULL;
    int slot;

    if (!WORDS)
    {
        return;
    }

    for (slot = 0; slot < CF_HASHTABLESIZE; slot++)
    {
        for (tp = TOPICHASH[slot]; tp != NULL; tp = tp->next)
        {
            IdempPrependItem(&list, tp->topic_name, tp->topic_context);
        }
    }

    list = SortItemListNames(list);

    for (ip = list; ip != NULL; ip = ip->next)
    {
        printf("%s::%s\n", ip->classes, ip->name);
    }
}

/*********************************************************************/

void ShowSingletons()
{
    Topic *tp;
    Item *ip, *list = NULL;
    int slot;

    if (VERBOSE || DEBUG)
    {
        for (slot = 0; slot < CF_HASHTABLESIZE; slot++)
        {
            for (tp = TOPICHASH[slot]; tp != NULL; tp = tp->next)
            {
                if (tp->associations == NULL)
                {
                    PrependItem(&list, tp->topic_name, tp->topic_context);
                }
            }
        }

        list = SortItemListNames(list);

        for (ip = list; ip != NULL; ip = ip->next)
        {
            printf(" ! Warning, topic \"%s::%s\" is a singleton with no connection to the map\n", ip->classes,
                   ip->name);
        }
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static void VerifyInferencePromise(Promise *pp)
{
    Attributes a = { {0} };
    Rlist *rpp, *rpq;

    if (!IsDefinedClass(pp->classes))
    {
        CfOut(cf_verbose, "", " -> Skipping inference for \"%s\" as class \"%s\" is not defined", pp->promiser,
              pp->classes);
        return;
    }

    a = GetInferencesAttributes(pp);

    for (rpp = a.precedents; rpp != NULL; rpp = rpp->next)
    {
        for (rpq = a.qualifiers; rpq != NULL; rpq = rpq->next)
        {
            CfOut(cf_verbose, "", " -> Add inference: (%s,%s,%s)\n", ScalarValue(rpp), ScalarValue(rpq), pp->promiser);
            AddInference(&INFERENCES, pp->promiser, rpp->item, rpq->item);
        }
    }
}

/*********************************************************************/

static void VerifyThingsPromise(Promise *pp)
{
    char id[CF_BUFSIZE];
    Attributes a = { {0} };
    Topic *tp = NULL, *otp;
    Rlist *rp, *rps, *contexts;
    char *handle = (char *) GetConstraintValue("handle", pp, CF_SCALAR);

    a = GetThingsAttributes(pp);

    CfOut(cf_verbose, "", " -> Attempting to install thing-topic %s::%s \n", pp->classes, pp->promiser);

// Add a standard reserved word

    contexts = SplitContextExpression(pp->classes, pp);

    for (rp = contexts; rp != NULL; rp = rp->next)
    {
        if ((tp = InsertTopic(pp->promiser, rp->item)) == NULL)
        {
            return;
        }

        CfOut(cf_verbose, "", " -> New thing \"%s\" about context \"%s\"", pp->promiser, ScalarValue(rp));

        if (a.fwd_name && a.bwd_name)
        {
            CfOut(cf_verbose, "", " -> New thing \"%s\" has a relation \"%s/%s\"", pp->promiser, a.fwd_name,
                  a.bwd_name);

            AddTopicAssociation(tp, &(tp->associations), a.fwd_name, a.bwd_name, a.associates, true, rp->item,
                                pp->promiser);
        }

        // Handle all synonyms as associations

        if (handle)
        {
            char synonym[CF_BUFSIZE];

            snprintf(synonym, CF_BUFSIZE - 1, "handles::%s", handle);
            otp = IdempInsertTopic(synonym);
            PrependRScalar(&(a.synonyms), otp->topic_name, CF_SCALAR);
        }

        // Handle all synonyms as associations

        if (a.synonyms)
        {
            for (rps = a.synonyms; rps != NULL; rps = rps->next)
            {
                otp = IdempInsertTopic(rps->item);
                CfOut(cf_verbose, "", " ---> %s is a synonym for %s", ScalarValue(rps), tp->topic_name);
            }

            AddTopicAssociation(tp, &(tp->associations), KM_SYNONYM, KM_SYNONYM, a.synonyms, true, rp->item,
                                pp->promiser);
        }

        // Handle all generalizations as associations

        if (a.general)
        {
            for (rps = a.general; rps != NULL; rps = rps->next)
            {
                otp = IdempInsertTopic(rps->item);
                CfOut(cf_verbose, "", " ---> %s is a generalization for %s", ScalarValue(rps), tp->topic_name);
            }

            AddTopicAssociation(tp, &(tp->associations), KM_GENERALIZES_B, KM_GENERALIZES_F, a.general, true, rp->item,
                                pp->promiser);
        }

        // Treat comments as occurrences of information.

        if (pp->ref)
        {
        Rlist *list = NULL, *topics = NULL;

            snprintf(id, CF_MAXVARSIZE-1, "promisers::%s", pp->promiser);
            PrependRScalar(&list, "Comment", CF_SCALAR);
            PrependRScalar(&topics, id, CF_SCALAR);
            AddOccurrence(&OCCURRENCES, pp->ref, list, cfk_literal, topics, pp->classes);
            DeleteRlist(list);
            DeleteRlist(topics);
        }

        if (handle)
        {
        Rlist *list = NULL, *topics = NULL;

            PrependRScalar(&list, handle, CF_SCALAR);
            AddTopicAssociation(tp, &(tp->associations), "is the promise of", "stands for", list, true, rp->item,
                                pp->promiser);
            DeleteRlist(list);
            list = NULL;
            snprintf(id, CF_MAXVARSIZE, "handles::%s", handle);
            PrependRScalar(&list, "promise handle", CF_SCALAR);
            PrependRScalar(&topics, id, CF_SCALAR);
            AddOccurrence(&OCCURRENCES, pp->ref, list, cfk_literal,  topics, pp->classes);
            DeleteRlist(list);
            DeleteRlist(topics);
        }
    }

    DeleteRlist(contexts);
}

/*********************************************************************/

static void VerifyTopicPromise(Promise *pp)
{
    char id[CF_BUFSIZE];
    Attributes a = { {0} };
    Topic *tp = NULL, *otp;
    Rlist *rp, *rps, *contexts;
    char *handle = (char *) GetConstraintValue("handle", pp, CF_SCALAR);

    a = GetTopicsAttributes(pp);

    CfOut(cf_verbose, "", " -> Attempting to install topic %s::%s \n", pp->classes, pp->promiser);

// Add a standard reserved word

    contexts = SplitContextExpression(pp->classes, pp);

    for (rp = contexts; rp != NULL; rp = rp->next)
    {
        if ((tp = InsertTopic(pp->promiser, rp->item)) == NULL)
        {
            return;
        }

        CfOut(cf_verbose, "", " -> New topic promise for \"%s\" about context \"%s\"", pp->promiser, ScalarValue(rp));

        if (a.fwd_name && a.bwd_name)
        {
            AddTopicAssociation(tp, &(tp->associations), a.fwd_name, a.bwd_name, a.associates, true, rp->item,
                                pp->promiser);
        }

        // Handle all synonyms as associations

        if (a.synonyms)
        {
            for (rps = a.synonyms; rps != NULL; rps = rps->next)
            {
                otp = IdempInsertTopic(rps->item);
                CfOut(cf_verbose, "", " ---> %s is a synonym for %s", ScalarValue(rps), tp->topic_name);
            }

            AddTopicAssociation(tp, &(tp->associations), KM_SYNONYM, KM_SYNONYM, a.synonyms, true, rp->item,
                                pp->promiser);
        }

        // Handle all generalizations as associations

        if (a.general)
        {
            for (rps = a.general; rps != NULL; rps = rps->next)
            {
                otp = IdempInsertTopic(rps->item);
                CfOut(cf_verbose, "", " ---> %s is a generalization for %s", ScalarValue(rps), tp->topic_name);
            }

            AddTopicAssociation(tp, &(tp->associations), KM_GENERALIZES_B, KM_GENERALIZES_F, a.general, true, rp->item,
                                pp->promiser);
        }

        if (handle)
        {
            char synonym[CF_BUFSIZE];

            snprintf(synonym, CF_BUFSIZE - 1, "handles::%s", handle);
            otp = IdempInsertTopic(synonym);
            PrependRScalar(&(a.synonyms), otp->topic_name, CF_SCALAR);
        }

        // Treat comments as occurrences of information.

        if (pp->ref)
        {
        Rlist *list = NULL, *topics = NULL;

            snprintf(id, CF_MAXVARSIZE-1, "promisers::%s", pp->promiser);
            PrependRScalar(&list, "Comment", CF_SCALAR);
            PrependRScalar(&topics, id, CF_SCALAR);
            AddOccurrence(&OCCURRENCES, pp->ref, list, cfk_literal, topics, pp->classes);
            DeleteRlist(list);
            DeleteRlist(topics);
        }

        if (handle)
        {
        Rlist *list = NULL, *topics = NULL;

            PrependRScalar(&list, handle, CF_SCALAR);
            AddTopicAssociation(tp, &(tp->associations), "is the promise of", "stands for", list, true, rp->item,
                                pp->promiser);
            DeleteRlist(list);
            list = NULL;
            snprintf(id, CF_MAXVARSIZE, "handles::%s", handle);
            PrependRScalar(&list, "promise handle", CF_SCALAR);
            PrependRScalar(&topics, id, CF_SCALAR);
            AddOccurrence(&OCCURRENCES, pp->ref, list, cfk_literal, topics, pp->classes);
            DeleteRlist(list);
            DeleteRlist(topics);
        }
   }

    DeleteRlist(contexts);
}

/*********************************************************************/

static void VerifyOccurrencePromises(Promise *pp)
{
    Attributes a = { {0} };
    enum representations rep_type;
    Rlist *contexts, *rp;

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
            CfOut(cf_error, "", " ! Occurrence of text information \"%s\" does not promise any topics to represent",
                  pp->promiser);
        }
        else
        {
            CfOut(cf_error, "",
                  " ! Occurrence or reference to information \"%s\" does not promise any topics to represent",
                  pp->promiser);
        }
        return;
    }

    contexts = SplitContextExpression(pp->classes, pp);

    for (rp = contexts; rp != NULL; rp = rp->next)
    {
        CfOut(cf_verbose, "", " -> New occurrence promise for \"%s\" about context \"%s\"", pp->promiser,
              ScalarValue(rp));

        switch (rep_type)
        {
        default:

            AddOccurrence(&OCCURRENCES, pp->promiser, a.represents, rep_type, a.about_topics, rp->item);
            break;
        }
    }

    DeleteRlist(contexts);
}

/*********************************************************************/

static void WriteKMDB()
{
#ifdef HAVE_NOVA
    if (WRITE_KMDB)
    {
        Nova_StoreKMDB(TOPICHASH, OCCURRENCES, INFERENCES);
    }
#endif
}

/*********************************************************************/

static void GenerateManual()
{
    if (GENERATE_MANUAL)
    {
        TexinfoManual(MANDIR);
    }
}

/*********************************************************************/

static void GenerateXml(void)
{
    if (OUTPUT_FILE == NULL)
    {
        /* Reconsider this once agents do not output any error messages to stdout */
        FatalError("Please specify output file");
    }
    else
    {
        FILE *out = fopen(OUTPUT_FILE, "w");

        if (out == NULL)
        {
            FatalError("Unable to open %s for writing\n", OUTPUT_FILE);
        }
        XmlManual(MANDIR, out);
    }
}

/*****************************************************************************/

static Topic *IdempInsertTopic(char *classified_name)
{
    char context[CF_MAXVARSIZE], topic[CF_MAXVARSIZE];

    context[0] = '\0';
    topic[0] = '\0';

    DeClassifyTopic(classified_name, topic, context);

    return InsertTopic(topic, context);
}

/*****************************************************************************/

static Topic *InsertTopic(char *name, char *context)
{
    int slot = GetHash(ToLowerStr(name));

    return AddTopic(&(TOPICHASH[slot]), name, context);
}

/*****************************************************************************/

static Topic *AddTopic(Topic **list, char *name, char *context)
{
    Topic *tp;

    if ((tp = TopicExists(name, context)))
    {
        CfOut(cf_verbose, "", " -> Topic %s already defined, ok\n", name);
    }
    else
    {
        tp = xmalloc(sizeof(Topic));

        tp->topic_name = xstrdup(NormalizeTopic(name));

        if (context && strlen(context) > 0)
        {
            tp->topic_context = xstrdup(NormalizeTopic(context));
        }
        else
        {
            tp->topic_context = xstrdup("any");
        }

        tp->id = GLOBAL_ID++;
        tp->associations = NULL;
        tp->next = *list;
        *list = tp;
        CF_TOPICS++;
    }

    return tp;
}

/*****************************************************************************/

static void AddTopicAssociation(Topic *this_tp, TopicAssociation **list, char *fwd_name, char *bwd_name,
                                Rlist *passociates, int ok_to_add_inverse, char *from_context, char *from_topic)
{
    TopicAssociation *ta = NULL, *texist;
    char fwd_context[CF_MAXVARSIZE];
    Rlist *rp;
    Topic *new_tp;
    char contexttopic[CF_BUFSIZE], ntopic[CF_BUFSIZE], ncontext[CF_BUFSIZE];

    strncpy(ntopic, NormalizeTopic(from_topic), CF_BUFSIZE - 1);
    strncpy(ncontext, NormalizeTopic(from_context), CF_BUFSIZE - 1);
    snprintf(contexttopic, CF_MAXVARSIZE, "%s::%s", ncontext, ntopic);
    strncpy(fwd_context, CanonifyName(fwd_name), CF_MAXVARSIZE - 1);

    if (passociates == NULL || passociates->item == NULL)
    {
        CfOut(cf_error, "", " !! A topic must have at least one associate in association %s", fwd_name);
        return;
    }

    if ((texist = AssociationExists(*list, fwd_name, bwd_name)) == NULL)
    {
        ta = xcalloc(1, sizeof(TopicAssociation));

        ta->fwd_name = xstrdup(fwd_name);

        if (bwd_name)
        {
            ta->bwd_name = xstrdup(bwd_name);
        }

        ta->fwd_context = xstrdup(fwd_context);

        ta->next = *list;
        *list = ta;
    }
    else
    {
        ta = texist;
    }

/* Association now exists, so add new members */

    if (ok_to_add_inverse)
    {
        CfOut(cf_verbose, "", " -> BEGIN add fwd associates for %s::%s", ncontext, ntopic);
    }
    else
    {
        CfOut(cf_verbose, "", "  ---> BEGIN reverse associations %s::%s", ncontext, ntopic);
    }

// First make sure topics pointed to exist so that they can point to us also

    for (rp = passociates; rp != NULL; rp = rp->next)
    {
        char normalform[CF_BUFSIZE] = { 0 };

        strncpy(normalform, NormalizeTopic(rp->item), CF_BUFSIZE - 1);
        new_tp = IdempInsertTopic(normalform);

        if (strcmp(contexttopic, normalform) == 0)
        {
            CfOut(cf_verbose, "", " ! Excluding self-reference to %s", ScalarValue(rp));
            continue;
        }

        if (ok_to_add_inverse)
        {
            CfOut(cf_verbose, "", " --> Adding '%s' with id %d as an associate of '%s::%s'", normalform, new_tp->id,
                  this_tp->topic_context, this_tp->topic_name);
        }
        else
        {
            CfOut(cf_verbose, "", " ---> Reverse '%s' with id %d as an associate of '%s::%s' (inverse)", normalform,
                  new_tp->id, this_tp->topic_context, this_tp->topic_name);
        }

        if (!IsItemIn(ta->associates, normalform))
        {
            PrependFullItem(&(ta->associates), normalform, NULL, new_tp->id, 0);

            if (ok_to_add_inverse)
            {
                // inverse is from normalform to ncontext::ntopic
                char rev[CF_BUFSIZE], ndt[CF_BUFSIZE], ndc[CF_BUFSIZE];
                Rlist *rlist = 0;

                snprintf(rev, CF_BUFSIZE - 1, "%s::%s", ncontext, ntopic);
                PrependRScalar(&rlist, rev, CF_SCALAR);

                // Stupid to have to declassify + reclassify, but ..
                DeClassifyTopic(normalform, ndt, ndc);
                AddTopicAssociation(new_tp, &(new_tp->associations), bwd_name, fwd_name, rlist, false, ndc, ndt);
                DeleteRlist(rlist);
            }
        }
        else
        {
            CfOut(cf_verbose, "", " -> Already in %s::%s's associate list", ncontext, ntopic);
        }

        CF_EDGES++;
    }

    if (ok_to_add_inverse)
    {
        CfOut(cf_verbose, "", " -> END add fwd associates for %s::%s", ncontext, ntopic);
    }
    else
    {
        CfOut(cf_verbose, "", "  ---> END reverse associations %s::%s", ncontext, ntopic);
    }
}

/*****************************************************************************/

static void AddOccurrence(Occurrence **list, char *reference, Rlist *represents, enum representations rtype,
                          Rlist *about_topics, char *context)
{
    Occurrence *op = NULL;
    Rlist *rp;

    if ((op = OccurrenceExists(*list, reference, rtype, context)) == NULL)
    {
        op = xcalloc(1, sizeof(Occurrence));

        op->occurrence_context = xstrdup(ToLowerStr(context));
        op->locator = xstrdup(reference);
        op->rep_type = rtype;
        op->next = *list;
        *list = op;
        CF_OCCUR++;
        CfOut(cf_verbose, "", " -> Noted occurrence for %s::%s", context, reference);
    }

/* Occurrence now exists, so add new subtype promises */

    if (represents == NULL)
    {
        IdempPrependRScalar(&(op->represents), "Unspecified document", CF_SCALAR);
    }

    for (rp = represents; rp != NULL; rp = rp->next)
    {
        IdempPrependRScalar(&(op->represents), rp->item, rp->type);
    }

    for (rp = about_topics; rp != NULL; rp = rp->next)
    {
        IdempPrependRScalar(&(op->about_topics), ToLowerStr(rp->item), rp->type);
        IdempInsertTopic(ToLowerStr(rp->item));
    }

}

/*********************************************************************/

void AddInference(Inference **list, char *result, char *pre, char *qual)
{
    Inference *ip;

    ip = xmalloc(sizeof(Occurrence));

    ip->inference = xstrdup(result);
    ip->precedent = xstrdup(pre);
    ip->qualifier = xstrdup(qual);
    ip->next = *list;
    *list = ip;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static Topic *TopicExists(char *topic_name, char *topic_context)
{
    Topic *tp;
    int slot;

    slot = GetHash(ToLowerStr(topic_name));

    for (tp = TOPICHASH[slot]; tp != NULL; tp = tp->next)
    {
        if (strcmp(tp->topic_name, NormalizeTopic(topic_name)) == 0)
        {
            if (topic_context)
            {
                if (strlen(topic_context) > 0 && strcmp(tp->topic_context, NormalizeTopic(topic_context)) == 0)
                {
                    return tp;
                }

                if (strlen(topic_context) == 0 && strcmp(tp->topic_context, "any") == 0)
                {
                    return tp;
                }
            }
        }
    }

    return NULL;
}

/*****************************************************************************/

int GetTopicPid(char *classified_topic)
{
    Topic *tp;
    int slot;
    char context[CF_MAXVARSIZE], name[CF_MAXVARSIZE];

    name[0] = '\0';

    DeClassifyTopic(classified_topic, name, context);
    slot = GetHash(ToLowerStr(name));

    if ((tp = GetTopic(TOPICHASH[slot], classified_topic)))
    {
        return tp->id;
    }

    return 0;
}

/*****************************************************************************/

static TopicAssociation *AssociationExists(TopicAssociation *list, char *fwd, char *bwd)
{
    TopicAssociation *ta;
    int yfwd = false, ybwd = false;
    char l[CF_BUFSIZE], r[CF_BUFSIZE];

    if (fwd == NULL || (fwd && strlen(fwd) == 0))
    {
        CfOut(cf_error, "", "NULL forward association name\n");
        return NULL;
    }

    if (bwd == NULL || (bwd && strlen(bwd) == 0))
    {
        CfOut(cf_verbose, "", "NULL backward association name\n");
    }

    for (ta = list; ta != NULL; ta = ta->next)
    {
        if (fwd && (strcmp(fwd, ta->fwd_name) == 0))
        {
            CfOut(cf_verbose, "", "Association '%s' exists already\n", fwd);
            yfwd = true;
        }
        else if (fwd && ta->fwd_name)
        {
            strncpy(l, ToLowerStr(fwd), CF_MAXVARSIZE);
            strncpy(r, ToLowerStr(ta->fwd_name), CF_MAXVARSIZE);

            if (strcmp(l, r) == 0)
            {
                CfOut(cf_error, "", " ! Association \"%s\" exists with different capitalization \"%s\"\n", fwd,
                      ta->fwd_name);
                yfwd = true;
            }
            else
            {
                yfwd = false;
            }
        }
        else
        {
            yfwd = false;
        }

        if (bwd && (strcmp(bwd, ta->bwd_name) == 0))
        {
            CfOut(cf_verbose, "", " ! Association '%s' exists already\n", bwd);
            ybwd = true;
        }
        else if (bwd && ta->bwd_name)
        {
            strncpy(l, ToLowerStr(bwd), CF_MAXVARSIZE);
            strncpy(r, ToLowerStr(ta->bwd_name), CF_MAXVARSIZE);

            if (strcmp(l, r) == 0)
            {
                CfOut(cf_inform, "", " ! Association \"%s\" exists with different capitalization \"%s\"\n", bwd,
                      ta->bwd_name);
            }

            ybwd = true;
        }
        else
        {
            ybwd = false;
        }

        if (yfwd && ybwd)
        {
            return ta;
        }
    }

    return NULL;
}

/*****************************************************************************/

static Occurrence *OccurrenceExists(Occurrence *list, char *locator, enum representations rep_type, char *context)
{
    Occurrence *op;

    for (op = list; op != NULL; op = op->next)
    {
        if (strcmp(locator, op->locator) == 0)
        {
        if (strstr(op->occurrence_context,context) == NULL)
           {
           // This context is unknown, but same reference
           char *replace = xmalloc(strlen(op->occurrence_context)+strlen(", .")+strlen(context));

           strcpy(replace,op->occurrence_context);
           strcat(replace,", ");
           strcat(replace,context);
           free(op->occurrence_context);
           op->occurrence_context = replace;
           }
        
            return op;
        }
    }

    return NULL;
}

/*****************************************************************************/

static char *NormalizeTopic(char *s)
{
    char *sp;
    int special = false;

    for (sp = s; *sp != '\0'; sp++)
    {
        if (strchr("/\\&|=$@", *sp))
        {
            special = true;
            break;
        }
    }

    if (special)
    {
        return s;
    }
    else
    {
        return ToLowerStr(s);
    }
}
