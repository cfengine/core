
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

void EnterpriseVersion()

{
#ifdef HAVE_LIBCFNOVA
 Nova_Version();
#endif 
}

/*****************************************************************************/
/* Knowledge                                                                 */
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

char *PromiseID(struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
 Nova_PromiseID(pp);
#else
#endif
}

/*****************************************************************************/

void NotePromiseCompliance(struct Promise *pp,double val)

{
#ifdef HAVE_LIBCFNOVA
 Nova_NotePromiseCompliance(pp,val);
#else
#endif
}

/*****************************************************************************/

time_t GetPromiseCompliance(struct Promise *pp,double *value,double *average,double *var,time_t *lastseen)
    
{
#ifdef HAVE_LIBCFNOVA
return Nova_GetPromiseCompliance(pp,value,average,var,lastseen);
#else
return time(NULL);
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

void RegisterBundleDependence(char *name,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
 Nova_RegisterBundleDepedence(name,pp);
#else
#endif
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
   NewScope("mon");
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
Nova_DumpSlowlyVaryingObservations();

#else
#endif
}

/*****************************************************************************/

void GetClassName(int i,char *name)
{
#ifdef HAVE_LIBCFNOVA
 Nova_GetClassName(i,name);
#else
 return OBS[i][0];
#endif
}

/*****************************************************************************/

void LookUpClassName(int i,char *name)
{
#ifdef HAVE_LIBCFNOVA
 Nova_LookupClassName(i,name);
#else
 return OBS[i][0];
#endif
}

/*****************************************************************************/

void LoadSlowlyVaryingObservations()

{
#ifdef HAVE_LIBCFNOVA
 Nova_LoadSlowlyVaryingObservations();
#else
 CfOut(cf_verbose,"","# Extended system discovery is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/
/* Server                                                                    */
/*****************************************************************************/

void RegisterLiteralServerData(char *handle,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
 Nova_RegisterLiteralServerData(handle,pp);
#else
 CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

char *ReturnLiteralData(char *handle)

{
#ifdef HAVE_LIBCFNOVA
return Nova_ReturnLiteralData(handle);
#else
 CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
return "";
#endif 
}

/*****************************************************************************/

char *GetRemoteScalar(char *handle,char *server,int encrypted)

{
#ifdef HAVE_LIBCFNOVA
return Nova_GetRemoteScalar(handle,server,encrypted);
#else
CfOut(cf_verbose,"","# Access to server literals is only available in version Nova and above\n");
return "";
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

void ReportSoftware(struct CfPackageManager *list)
{
#ifdef HAVE_LIBCFNOVA
 Nova_ReportSoftware(list);
#else
 CfOut(cf_verbose,"","# Software reporting feature is only available in version Nova and above\n");
#endif
}

/*****************************************************************************/

void SummarizeSoftware(int xml,int html,int csv,int embed,char *stylesheet,char *head,char *foot,char *web)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SummarizeSoftware(xml,html,csv,embed,stylesheet,head,foot,web);
#else
 CfOut(cf_verbose,"","# Software summary reporting feature is only available in version Nova and above\n");
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

void SetMeasurementPromises(struct Item **classlist)

{
#ifdef HAVE_LIBCFNOVA
 Nova_SetMeasurementPromises(classlist);
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
   
/*****************************************************************************/
/* SQL                                                                       */
/*****************************************************************************/

int CheckDatabaseSanity(struct Attributes a, struct Promise *pp)
{
#ifdef HAVE_LIBCFNOVA
return Nova_CheckDatabaseSanity(a,pp);
#else
return false;
#endif
}

/*****************************************************************************/

int VerifyDatabasePromise(CfdbConn *cfdb,char *database,struct Attributes a,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
  char query[CF_BUFSIZE],name[CF_MAXVARSIZE];
  int found = false;
 
CfOut(cf_verbose,""," -> Verifying promised database");

if (!cfdb->connected)
   {
   CfOut(cf_inform,"","Database %s is not connected\n",database);
   return false;
   }

Nova_CreateDBQuery(cfdb->type,query);

CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns < 1)
   {
   CfOut(cf_error,""," !! The schema did not promise the expected number of fields - got %d expected >= %d\n",cfdb->maxcolumns,1);
   return false;
   }

while(CfFetchRow(cfdb))
   {
   strncpy(name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);

   if (strcmp(name,database) == 0)
      {
      found = true;
      }
   }

if (found)
   {
   CfOut(cf_verbose,""," -> Database \"%s\" exists on this connection",database);
   return true;
   }
else
   {
   CfOut(cf_verbose,""," !! Database \"%s\" does not seem to exist on this connection",database);
   }

if (a.database.operation && strcmp(a.database.operation,"drop") == 0)
   {
   if (a.transaction.action != cfa_warn && !DONTDO)
      {
      CfOut(cf_verbose,""," -> Attempting to delete the database %s",database);
      snprintf(query,CF_MAXVARSIZE-1,"drop database %s",database); 
      CfVoidQueryDB(cfdb,query);
      return cfdb->result;
      }
   else
      {
      CfOut(cf_error,""," !! Need to delete the database %s but only a warning was promised\n",database);
      return false;
      }   
   }

if (a.database.operation && strcmp(a.database.operation,"create") == 0)
   {
   if (a.transaction.action != cfa_warn && !DONTDO)
      {
      CfOut(cf_verbose,""," -> Attempting to create the database %s",database);
      snprintf(query,CF_MAXVARSIZE-1,"create database %s",database); 
      CfVoidQueryDB(cfdb,query);
      return cfdb->result;
      }
   else
      {
      CfOut(cf_error,""," !! Need to create the database %s but only a warning was promised\n",database);
      return false;
      }
   }

return false;
#else
CfOut(cf_verbose,"","Verifying SQL database promises is only available with Cfengine Nova or above");
return false;
#endif
}

/*****************************************************************************/

int CfVerifyTablePromise(CfdbConn *cfdb,char *name,struct Rlist *columns,struct Attributes a,struct Promise *pp)

{
#ifdef HAVE_LIBCFNOVA
return Nova_VerifyTablePromise(cfdb,name,columns,a,pp);
#else
CfOut(cf_verbose,"","Verifying SQL table promises is only available with Cfengine Nova or above");
return false;
#endif
}

/*****************************************************************************/
/* Linker troubles require this code to be here in the main body             */
/*****************************************************************************/

#ifdef HAVE_LIBCFNOVA

int Nova_VerifyTablePromise(CfdbConn *cfdb,char *table_path,struct Rlist *columns,struct Attributes a,struct Promise *pp)

{ char name[CF_MAXVARSIZE],type[CF_MAXVARSIZE],query[CF_MAXVARSIZE],table[CF_MAXVARSIZE];
  int i,count,size,no_of_cols,*size_table,*done,identified,retval = true;
  char **name_table,**type_table;
  struct Rlist *rp, *cols;

CfOut(cf_verbose,""," -> Verifying promised table structure");

if (!cfdb->connected)
   {
   CfOut(cf_inform,"","Database %s is not connected\n",table_path);
   return false;
   }

if (!Nova_ValidateSQLTableName(table_path,table))
   {
   CfOut(cf_error,"","The structure of the promiser did not match that for an SQL table, i.e. \"database.table\"\n",table_path);
   return false;
   }

/* Get a list of the columns in the table */

Nova_QueryTable(query,table);
CfNewQueryDB(cfdb,query);

if (cfdb->maxcolumns != 3)
   {
   CfDeleteQuery(cfdb);
   }

/* Assume that the Rlist has been validated and consists of a,b,c */

count = 0;
no_of_cols = RlistLen(columns);

if (!Nova_NewSQLColumns(table,columns,&name_table,&type_table,&size_table,&done))
   {
   return false;
   }

/* Obtain columns from the named table - if any */

while(CfFetchRow(cfdb))
   {
   strncpy(name,CfFetchColumn(cfdb,0),CF_MAXVARSIZE-1);
   strncpy(type,ToLowerStr(CfFetchColumn(cfdb,1)),CF_MAXVARSIZE-1);
   size = Str2Int(CfFetchColumn(cfdb,2));

   if (size == CF_NOINT)
      {
      cfPS(cf_error,CF_NOP,"",pp,a," !! Integer size of datatype could not be determined - invalid promise.");
      Nova_DeleteSQLColumns(name_table,type_table,size_table,done,no_of_cols);
      free(done);
      return false;
      }

   identified = false;
   
   for (i = 0; i < no_of_cols; i++)
      {
      if (done[i])
         {
         continue;
         }

      if (strcmp(name,name_table[i]) == 0)
         {
         NovaCheckSQLDataType(type,type_table[i],pp);

         if (size != size_table[i])
            {
            cfPS(cf_error,CF_FAIL,"",pp,a," !! Promised column %s in database %s has a non-matching array size (%d != %d)",name,pp->promiser,size,size_table[i]);
            }
         
         count++;
         done[i] = true;
         identified = true;
         break;
         }
      }

   if (!identified)
      {
      cfPS(cf_error,CF_FAIL,"",pp,a,"Column \"%s\" found in database table %s is not part of its promise.",name,pp->promiser);

      if (a.database.operation && strcmp(a.database.operation,"drop") == 0)
         {
         cfPS(cf_error,CF_FAIL,"",pp,a,"Cfengine will not promise to repair this, as the operation is potentially too destructive.");
         // Future allow deletion?
         }
      
      retval = false;
      }
   }

CfDeleteQuery(cfdb);

if (count == 0 && a.database.operation && strcmp(a.database.operation,"create") == 0)
   {
   CfOut(cf_error,"","Database.table %s doesn't seem to exist, creating\n",table_path);
   return Nova_CreateTable(cfdb,table,columns,a,pp);
   }

/* Now look for deviations - only if we have promised to create missing */

if (a.database.operation && strcmp(a.database.operation,"create") != 0)
   {
   return retval;
   }

if (count != no_of_cols)
   {
   for (i = 0; i < no_of_cols; i++)
      {
      if (!done[i])
         {
         CfOut(cf_error,""," !! Promised column \"%s\" missing from database table %s",name_table[i],pp->promiser);
         
         if (!DONTDO && a.transaction.action != cfa_warn)
            {
            if (size_table[i] > 0)
               {
               snprintf(query,CF_MAXVARSIZE-1,"ALTER TABLE %s ADD %s %s(%d)",table,name_table[i],type_table[i],size_table[i]);
               }
            else
               {
               snprintf(query,CF_MAXVARSIZE-1,"ALTER TABLE %s ADD %s %s",table,name_table[i],type_table[i]);
               }
            
            CfVoidQueryDB(cfdb,query);
            cfPS(cf_error,CF_CHG,"",pp,a," !! Adding promised column \"%s\" to database table %s",name_table[i],table);
            retval = true;
            }
         else
            {
            cfPS(cf_error,CF_WARN,"",pp,a," !! Promised column \"%s\" missing from database table %s but only a warning was promised",name_table[i],table);
            retval = false;
            }
         }
      }
   }

Nova_DeleteSQLColumns(name_table,type_table,size_table,done,no_of_cols);
return retval;
}

/*****************************************************************************/

int Nova_CreateTable(CfdbConn *cfdb,char *table,struct Rlist *columns,struct Attributes a,struct Promise *pp)

{ char entry[CF_MAXVARSIZE],query[CF_BUFSIZE];
  int i,count,size,*size_table,*done,identified,retval = true;
  char **name_table,**type_table;
  struct Rlist *rp, *cols;
  int no_of_cols = RlistLen(columns);

CfOut(cf_error,""," -> Trying to create table %s\n",table);
  
if (!Nova_NewSQLColumns(table,columns,&name_table,&type_table,&size_table,&done))
   {
   return false;
   }

if (no_of_cols > 0)
   {
   snprintf(query,CF_BUFSIZE-1,"create table %s(",table);
   
   for (i = 0; i < no_of_cols; i++)
      {
      CfOut(cf_verbose,""," -> Forming column template %s %s %d\n",name_table[i],type_table[i],size_table[i]);;
      
      if (size_table[i] > 0)
         {
         snprintf(entry,CF_MAXVARSIZE-1,"%s %s(%d)",name_table[i],type_table[i],size_table[i]);
         }
      else
         {
         snprintf(entry,CF_MAXVARSIZE-1,"%s %s",name_table[i],type_table[i]);
         }

      strcat(query,entry);

      if (i < no_of_cols -1)
         {
         strcat(query,",");
         }
      }

   strcat(query,")");
   }

CfVoidQueryDB(cfdb,query);
Nova_DeleteSQLColumns(name_table,type_table,size_table,done,no_of_cols);
return true;
}

#endif
