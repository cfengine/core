
/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
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
/* File: enterprise_stubs.c                                                  */
/*                                                                           */
/*****************************************************************************/

/*

This file is a stub for generating cfengine's commerical enterprise level
versions. We appreciate your respect of our commercial offerings, which go
to accelerate future developments of both free and commercial versions. If
you have a good reason why a particular feature of the commercial version
should be free, please let us know and we will consider this carefully.

*/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void BundleNode(FILE *fp,char *bundle)

{
#ifdef HAVE_LIBCFNOVA
 Nova_BundleNode(fp,bundle);
#else
#endif
}

/*****************************************************************************/

void BodyNode(FILE *fp,char *bundle,int calltype)

{
#ifdef HAVE_LIBCFNOVA
 Nova_BodyNode(fp,bundle,calltype);
#else
#endif
}

/*****************************************************************************/

void TypeNode(FILE *fp,char *type)

{
#ifdef HAVE_LIBCFNOVA
 Nova_TypeNode(fp,type);
#else
#endif
}

/*****************************************************************************/

void PromiseNode(FILE *fp,struct Promise *pp,int type)

{
#ifdef HAVE_LIBCFNOVA
 Nova_PromiseNode(fp,pp,type);
#else
#endif
}

/*****************************************************************************/

void MapPromiseToTopic(FILE *fp,struct Promise *pp,char *version)

{
#ifdef HAVE_LIBCFNOVA
 Nova_MapPromiseToTopic(fp,pp,version); 
#else
#endif
}

/*****************************************************************************/

void ShowTopicRepresentation(FILE *fp)

{ int i,j,k,l,m;
  struct SubTypeSyntax *ss;
  struct BodySyntax *bs,*bs2;

#ifdef HAVE_LIBCFNOVA
 Nova_ShowTopicRepresentation(fp);
#else
 CfOut(cf_verbose,"","# Knowledge map reporting feature is only available in version Nova and above\n");
#endif

 
for  (i = 0; i < CF3_MODULES; i++)
   {
   if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
      {
      continue;
      }

   for (j = 0; ss[j].btype != NULL; j++)
      {
      if (ss[j].bs != NULL) /* In a bundle */
         {
         bs = ss[j].bs;

         for (l = 0; bs[l].lval != NULL; l++)
            {
            fprintf(fp,"Promise_types::\n");
            fprintf(fp,"   \"%s\";\n",ss[j].subtype);
            
            fprintf(fp,"Body_lval_types::\n");
            fprintf(fp,"   \"%s\"\n",bs[l].lval);
            fprintf(fp,"   association => a(\"is a body-lval for\",\"Promise_types::%s\",\"has body-lvals\");\n",ss[j].subtype);
            
            if (bs[l].dtype == cf_body)
               {
               bs2 = (struct BodySyntax *)(bs[l].range);
               
               if (bs2 == NULL || bs2 == (void *)CF_BUNDLE)
                  {
                  continue;
                  }
               
               for (k = 0; bs2[k].dtype != cf_notype; k++)
                  {
                  fprintf(fp,"   \"%s\";\n",bs2[k].lval);
                  }
               }
            }
         }
      }
   }

for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
   {
   fprintf(fp,"   \"%s\";\n",CF_COMMON_BODIES[i].lval);
   }


for (i = 0; CF_COMMON_EDITBODIES[i].lval != NULL; i++)
   {
   fprintf(fp,"   \"%s\";\n",CF_COMMON_EDITBODIES[i].lval);
   }

}

/*****************************************************************************/
/* Monitord                                                                  */
/*****************************************************************************/

void HistoryUpdate(struct Averages newvals)

{ struct Promise *pp = NewPromise("history_db","the long term memory");
  struct Attributes dummyattr;
  struct CfLock thislock;
  time_t now = time(NULL);
  char timekey[CF_MAXVARSIZE];
  static struct Averages shift_value;

#ifdef HAVE_LIBCFNOVA  
/* We do this only once per hour - this should not be changed */

Banner("Update long-term history");
  
dummyattr.transaction.ifelapsed = 59;

thislock = AcquireLock(pp->promiser,VUQNAME,now,dummyattr,pp);

if (thislock.lock == NULL)
   {
   Nova_UpdateShiftAverage(&shift_value,&newvals);
   return;
   }

/* Refresh the class context of the agent */
DeletePrivateClassContext();
DeleteEntireHeap();
DeleteAllScope();

if (!NOHARDCLASSES)
   {
   NewScope("sys");
   NewScope("const");
   NewScope("match");
   NewScope("control_monitor");
   NewScope("control_common");
   GetNameInfo3();
   GetInterfaceInfo3();
   FindV6InterfaceInfo();
   Get3Environment();
   OSClasses();
   SetReferenceTime(true);
   }

LoadPersistentContext();
LoadSystemConstants();

YieldCurrentLock(thislock);

snprintf(timekey,CF_MAXVARSIZE-1,"%s_%s_%s_%s",VDAY,VMONTH,VLIFECYCLE,VSHIFT);
Nova_HistoryUpdate(timekey,newvals);
Nova_ResetShiftAverage(&shift_value);
#else
#endif
}

/*****************************************************************************/
/* Reporting                                                                 */
/*****************************************************************************/

void SummarizeCompliance(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeCompliance(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Compliance reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizeFileChanges(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeFileChanges(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# File change reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizeSetuid(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeSetuid(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Setuid reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/
/* Montoring                                                                 */
/*****************************************************************************/

void InitMeasurements()
{
#ifdef HAVE_LIBCFNOVA
 NovaInitMeasurements();
#endif
}

/*****************************************************************************/

void VerifyMeasurement(double *this,struct Attributes a,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
 Nova_VerifyMeasurement(this,a,pp);
#else
 CfOut(cf_verbose,"","# Custom monitoring feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void LongHaul()

{
#ifdef HAVE_LIBCFNOVA
 Nova_LongHaul(VDAY,VMONTH,VLIFECYCLE,VSHIFT);
#else
#endif
}

/*****************************************************************************/
/* ACLs                                                                      */
/*****************************************************************************/

void VerifyACL(char *file,struct Attributes a, struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
 Nova_VerifyACL(file,a,pp);
#else
#endif
}
   
