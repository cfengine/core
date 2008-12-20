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
/* File: env_context.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

struct CfState
   {
   unsigned int expires;
   enum statepolicy policy;
   };

/*****************************************************************************/

void KeepClassContextPromise(struct Promise *pp)

{ struct Attributes a;

a = GetClassContextAttributes(pp);

if (a.context.broken)
   {
   cfPS(cf_error,CF_FAIL,"",pp,a,"Irreconcilable constraints in classes for %s (broken promise)",pp->promiser);
   return;
   }

if (strcmp(pp->bundletype,"common") == 0)
   {
   if (EvalClassExpression(a.context.expression,pp))
      {
      Verbose(" ?> defining additional global class %s\n",pp->promiser);
      NewClass(pp->promiser);
      }

   /* These are global and loaded once */
   *(pp->donep) = true;

   return;
   }

if (strcmp(pp->bundletype,THIS_AGENT) == 0 || FullTextMatch("edit_.*",pp->bundletype))
   {
   if (EvalClassExpression(a.context.expression,pp))
      {
      Debug(" ?> defining class %s\n",pp->promiser);
      NewBundleClass(pp->promiser,pp->bundle);
      }

   /* Private to bundle, can be reloaded */
   *(pp->donep) = false;
   
   return;
   }
}

/*****************************************************************************/

void DeletePrivateClassContext()

{
DeleteItemList(VADDCLASSES);
VADDCLASSES = NULL;
}
   

/*****************************************************************************/

void NewPersistentContext(char *name,unsigned int ttl_minutes,enum statepolicy policy)

{ int errno;
  DBT key,value;
  DB *dbp;
  struct CfState state;
  time_t now = time(NULL);
  char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);
  
if ((errno = db_create(&dbp,NULL,0)) != 0)
   {
   CfOut(cf_error,"db_open","Couldn't create persistent context database %s\n",filename);
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)    
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open persistent state database %s\n",filename);
   return;
   }

chmod(filename,0644);  
memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = name;
key.size = strlen(name)+1;

if ((errno = dbp->get(dbp,NULL,&key,&value,0)) != 0)
   {
   if (errno != DB_NOTFOUND)
      {
      dbp->err(dbp,errno,NULL);
      dbp->close(dbp,0);
      return;
      }
   }
 
if (value.data != NULL)
   {
   memcpy((void *)&state,value.data,sizeof(state));
   
   if (state.policy == cfpreserve)
      {
      if (now < state.expires)
         {
         Verbose("Persisent state %s is already in a preserved state --  %d minutes to go\n",name,(state.expires-now)/60);
         dbp->close(dbp,0);
         return;
         }
      }
   }
 else
    {
    Verbose("New persistent state %s but empty\n",key.data);
    }
 
 
memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = name;
key.size = strlen(name)+1;
 
state.expires = now + ttl_minutes * 60;
state.policy = policy; 
 
value.data = &state;
value.size = sizeof(state);
 
if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0)
   {
   CfOut(cf_error,"db->put","Database put failed in peristent class");
   }
else
   {
   Verbose("(Re)Set persistent state %s for %d minutes\n",name,ttl_minutes);
   }

dbp->close(dbp,0);
}

/*****************************************************************************/

void DeletePersistentContext(char *name)

{ int errno;
  DBT key,value;
  DB *dbp;
  char filename[CF_BUFSIZE];

snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);
  
if ((errno = db_create(&dbp,NULL,0)) != 0)
   {
   CfOut(cf_error,"db_open","Couldn't open the persistent state database %s\n",filename);
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)    
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open the persistent state database %s\n",filename);
   return;
   }

chmod(filename,0644); 

memset(&key,0,sizeof(key));       
memset(&value,0,sizeof(value));
      
key.data = name;
key.size = strlen(name)+1;

if ((errno = dbp->del(dbp,NULL,&key,0)) != 0)
   {
   CfOut(cf_error,"db_store","delete db failed");
   }
 
Debug("Deleted any persistent state %s\n",name); 
dbp->close(dbp,0);
}

/*****************************************************************************/

void LoadPersistentContext()

{ DBT key,value;
  DB *dbp;
  DBC *dbcp;
  DB_ENV *dbenv = NULL;
  int ret;
  time_t now = time(NULL);
  struct CfState q;
  char filename[CF_BUFSIZE];

Banner("Loading persistent classes");

snprintf(filename,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_STATEDB_FILE);
  
if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   CfOut(cf_error,"db_open","Couldn't open checksum database %s\n",filename);
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open persistent state database %s\n",filename);
   dbp->close(dbp,0);
   return;
   }

/* Acquire a cursor for the database. */

if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
   {
   CfOut(cf_error,"","Error reading from persistent state database");
   dbp->err(dbp, ret, "DB->cursor");
   return;
   }

 /* Initialize the key/data return pair. */

memset(&key, 0, sizeof(key));
memset(&value, 0, sizeof(value));
 
 /* Walk through the database and print out the key/data pairs. */

while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0)
   {
   memcpy((void *)&q,value.data,sizeof(struct CfState));

   Debug(" - Found key %s...\n",key.data);

   if (now > q.expires)
      {
      Verbose(" Persistent class %s expired\n",key.data);
      if ((errno = dbp->del(dbp,NULL,&key,0)) != 0)
         {
         CfOut(cf_error,"db_store","");
         }
      }
   else
      {
      Verbose(" Persistent class %s for %d more minutes\n",key.data,(q.expires-now)/60);
      Verbose(" Adding persistent class %s to heap\n",key.data);
      AddMultipleClasses(key.data);
      }
   }
 
dbcp->c_close(dbcp);
dbp->close(dbp,0);

Banner("Loaded persistent memory");
}

/*****************************************************************************/

void AddEphemeralClasses(struct Rlist *classlist)

{ struct Rlist *rp;

for (rp = classlist; rp != NULL; rp = rp->next)
   {
   NewClass(rp->item);
   }
}


/***************************************************************************/

int Abort()

{
if (ABORTBUNDLE)
   {
   ABORTBUNDLE = false;
   return true;
   }

return false;
}

/*****************************************************************************/

int VarClassExcluded(struct Promise *pp,char **classes)

{
*classes = (char *)GetConstraint("ifvarclass",pp->conlist,CF_SCALAR);

if (*classes == NULL)
   {
   return false;
   }

if (*classes && IsDefinedClass(*classes))
   {
   return false;
   }
else
   {
   return true;
   }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int EvalClassExpression(struct Constraint *cp,struct Promise *pp)

{ int result_and = true;
  int result_or = false;
  int result_xor = 0;
  int result,total = 0;
  char *lval = cp->lval,buffer[CF_MAXVARSIZE];
  struct Rlist *rp;
  double prob,cum = 0,fluct;

if (cp->type == CF_FNCALL)
   {
   FatalError("Software error - function call in EvalClassExpression (shouldn't happen)\n");
   }

if (strcmp(cp->lval,"expression") == 0)
   {
   if (IsDefinedClass((char *)cp->rval))
      {
      return true;
      }
   else
      {
      return false;
      }
   }

if (strcmp(cp->lval,"not") == 0)
   {
   if (IsDefinedClass((char *)cp->rval))
      {
      return false;
      }
   else
      {
      return true;
      }
   }

if (strcmp(cp->lval,"dist") == 0)
   {
   for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
      {
      result = Str2Int(rp->item);
      
      if (result < 0)
         {
         CfOut(cf_error,"","Non-positive integer in class distribution");
         PromiseRef(cf_error,pp);
         return false;
         }
      
      total += result;
      }
   }

fluct = drand48(); /* Get random number 0-1 */
cum = 0.0;

for (rp = (struct Rlist *)cp->rval; rp != NULL; rp = rp->next)
   {
   result = IsDefinedClass((char *)(rp->item));

   result_and = result_and && result;
   result_or  = result_or || result;
   result_xor += result;

   if (total > 0)
      {
      prob = ((double)Str2Int(rp->item))/((double)total);
      cum += prob;
      
      if ((fluct < cum) || rp->next == NULL)
         {
         snprintf(buffer,CF_MAXVARSIZE,"%s_%s",pp->promiser,rp->item);
         if (strcmp(pp->bundletype,"common") == 0)
            {
            NewClass(buffer);
            }
         else
            {
            NewBundleClass(buffer,pp->bundle);
            }
         Debug(" ?? \'Strategy\' distribution class interval -> %s\n",buffer);
         return true;
         }
      }
   }

if (strcmp(cp->lval,"or") == 0)
   {
   return result_or;
   }

if (strcmp(cp->lval,"xor") == 0)
   {
   return (result_xor == 1) ? true : false;
   }

if (strcmp(cp->lval,"and") == 0)
   {
   return result_and;
   }

return false;
}

/*******************************************************************/

void NewClass(char *class)

{
Chop(class);
Debug("NewClass(%s)\n",class);

if (strlen(class) == 0)
   {
   return;
   }

if (IsRegexItemIn(ABORTBUNDLEHEAP,class))
   {
   CfOut(cf_error,"","Bundle aborted on defined class \"%s\"\n",class);
   ABORTBUNDLE = true;
   }

if (IsRegexItemIn(ABORTHEAP,class))
   {
   CfOut(cf_error,"","cf-agent aborted on defined class \"%s\"\n",class);
   exit(1);
   }

if (IsItemIn(VHEAP,class))
   {
   return;
   }

AppendItem(&VHEAP,class,NULL);
}

/*******************************************************************/

void DeleteClass(char *class)

{
DeleteItemLiteral(&VHEAP,class);
DeleteItemLiteral(&VADDCLASSES,class);
}

/*******************************************************************/

void NewBundleClass(char *class,char *bundle)

{
Chop(class);
Debug("NewBundleClass(%s)\n",class);

if (strlen(class) == 0)
   {
   return;
   }

if (IsRegexItemIn(ABORTBUNDLEHEAP,class))
   {
   CfOut(cf_error,"","Bundle %s aborted on defined class \"%s\"\n",bundle,class);
   ABORTBUNDLE = true;
   }

if (IsRegexItemIn(ABORTHEAP,class))
   {
   CfOut(cf_error,"","cf-agent aborted on defined class \"%s\" defined in bundle %s\n",class,bundle);
   exit(1);
   }

if (IsItemIn(VADDCLASSES,class))
   {
   return;
   }

AppendItem(&VADDCLASSES,class,CONTEXTID);
}


