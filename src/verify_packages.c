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
/* File: verify_packages.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void VerifyPackagesPromise(struct Promise *pp)

{ struct Attributes a;
  struct CfLock thislock;
  char lockname[CF_BUFSIZE];

a = GetPackageAttributes(pp);

if (!PackageSanityCheck(a,pp))
   {
   return;
   }

PromiseBanner(pp);

snprintf(lockname,CF_BUFSIZE-1,"package-%s-%s",pp->promiser,a.packages.package_list_command);
 
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

if (!VerifyInstalledPackages(&INSTALLED_PACKAGE_LISTS,a,pp))
   {
   cfPS(cf_error,CF_FAIL,"",pp,a," !! Unable to obtain a list of installed packages - aborting");
   return;
   }

VerifyPromisedPackage(a,pp);

YieldCurrentLock(thislock);
}

/*****************************************************************************/

int PackageSanityCheck(struct Attributes a,struct Promise *pp)

{
if (a.packages.package_list_version_regex == NULL)
   {
   cfPS(cf_error,CF_FAIL,"",pp,a," !! You must supply a method for determining the version of existing packages");
   return false;
   }

if (a.packages.package_list_name_regex == NULL)
   {
   cfPS(cf_error,CF_FAIL,"",pp,a," !! You must supply a method for determining the name of existing packages");
   return false;
   }

if (a.packages.package_list_command == NULL && a.packages.package_file_repositories == NULL)
   {
   cfPS(cf_error,CF_FAIL,"",pp,a," !! You must supply a method for determining the list of existing packages (a command or repository list)");
   return false;
   }

if (a.packages.package_name_regex||a.packages.package_version_regex||a.packages.package_arch_regex)
   {
   if (a.packages.package_name_regex&&a.packages.package_version_regex&&a.packages.package_arch_regex)
      {
      if (!(a.packages.package_version && a.packages.package_architectures))
         {
         cfPS(cf_error,CF_FAIL,"",pp,a," !! You must supply all regexs for (name,version,arch) or a separate version number and architecture");
         return false;            
         }
      }
   else
      {
      if (a.packages.package_version && a.packages.package_architectures)
         {
         cfPS(cf_error,CF_FAIL,"",pp,a," !! You must supply all regexs for (name,version,arch) or a separate version number");
         return false;
         }
      }
   }

return true;
}

/*****************************************************************************/

void ExecutePackageSchedule(struct CfPackageManager *schedule)

{
Verbose(" >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
Verbose("   Offering these package-promise suggestions to the managers\n");
Verbose(" >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
 
 /* Normal ordering */

Verbose(" -> Deletion schedule...\n");

if (!ExecuteSchedule(schedule,cfa_deletepack))
   {
   CfOut(cf_error,"","Aborting package schedule");
   return;
   }

Verbose(" -> Addition schedule...\n");

if (!ExecuteSchedule(schedule,cfa_addpack))
   {
   return;
   }

Verbose(" -> Update schedule...\n");

if (!ExecuteSchedule(schedule,cfa_update))
   {
   return;
   }            

Verbose(" -> Patch schedule...\n");

if (!ExecuteSchedule(schedule,cfa_patch))
   {
   return;
   }
}
          
/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int VerifyInstalledPackages(struct CfPackageManager **all_mgrs,struct Attributes a,struct Promise *pp)

{ struct CfPackageManager *manager = NewPackageManager(all_mgrs,a.packages.package_list_command,cfa_pa_none,cfa_no_ppolicy);
  char vbuff[CF_BUFSIZE];
  struct Rlist *rp;
  struct dirent *dirp;
  FILE *prp;
  DIR *dirh;
 
if (!manager)
   {
   return false;
   }

if (manager->pack_list != NULL)
   {
   return true;
   }

if (a.packages.package_list_command != NULL)
   {
   Verbose(" ???????????????????????????????????????????????????????????????\n");
   Verbose("   Reading package list from %s\n",GetArg0(a.packages.package_list_command));
   Verbose(" ???????????????????????????????????????????????????????????????\n");

   if (!IsExecutable(GetArg0(a.packages.package_list_command)))
      {
      CfOut(cf_error,"","The proposed package list command \"%s\" was not executable",a.packages.package_list_command);
      return false;
      }
   
   if ((prp = cf_popen(a.packages.package_list_command,"r")) == NULL)
      {
      CfOut(cf_error,"cf_popen","Couldn't open the package list with command %s\n",a.packages.package_list_command);
      return false;
      }
   
   while (!feof(prp))
      {
      memset(vbuff,0,CF_BUFSIZE);
      ReadLine(vbuff,CF_BUFSIZE,prp);   
      
      if (!PrependListPackageItem(&(manager->pack_list),vbuff,a,pp))
         {
         continue;
         }
      }
   
   cf_pclose(prp);
   }

return true;
}

/*****************************************************************************/

void VerifyPromisedPackage(struct Attributes a,struct Promise *pp)

{ char version[CF_MAXVARSIZE];
  char name[CF_MAXVARSIZE];
  char arch[CF_MAXVARSIZE];
  char *package = pp->promiser;
  int matches = 0, installed = 0, no_version = false;
  struct Rlist *rp;
  
if (a.packages.package_version) 
   {
   /* The version is specified separately */

   for (rp = a.packages.package_architectures; rp != NULL; rp=rp->next)
      {
      strncpy(name,pp->promiser,CF_MAXVARSIZE-1);
      strncpy(version,a.packages.package_version,CF_MAXVARSIZE-1);
      strncpy(arch,rp->item,CF_MAXVARSIZE-1);
      installed += PackageMatch(name,"*","*",a,pp);
      matches += PackageMatch(name,version,arch,a,pp);
      }

   if (a.packages.package_architectures == NULL)
      {
      strncpy(name,pp->promiser,CF_MAXVARSIZE-1);
      strncpy(version,a.packages.package_version,CF_MAXVARSIZE-1);
      strncpy(arch,"*",CF_MAXVARSIZE-1);
      installed = PackageMatch(name,"*","*",a,pp);
      matches = PackageMatch(name,version,arch,a,pp);
      }
   }
else if (a.packages.package_version_regex)
   {
   /* The name, version and arch are to be extracted from the promiser */
   strncpy(version,ExtractFirstReference(a.packages.package_version_regex,package),CF_MAXVARSIZE-1);
   strncpy(name,ExtractFirstReference(a.packages.package_name_regex,package),CF_MAXVARSIZE-1);
   strncpy(arch,ExtractFirstReference(a.packages.package_arch_regex,package),CF_MAXVARSIZE-1);
   installed = PackageMatch(name,"*","*",a,pp);
   matches = PackageMatch(name,version,arch,a,pp);
   }
else
   {
   no_version = true;
   
   for (rp = a.packages.package_architectures; rp != NULL; rp=rp->next)
      {
      strncpy(name,pp->promiser,CF_MAXVARSIZE-1);
      strncpy(version,"*",CF_MAXVARSIZE-1);
      strncpy(arch,rp->item,CF_MAXVARSIZE-1);
      installed += PackageMatch(name,"*","*",a,pp);
      matches += PackageMatch(name,version,arch,a,pp);
      }
   
   if (a.packages.package_architectures == NULL)
      {
      strncpy(name,pp->promiser,CF_MAXVARSIZE-1);
      strncpy(version,"*",CF_MAXVARSIZE-1);
      strncpy(arch,"*",CF_MAXVARSIZE-1);
      installed = PackageMatch(name,"*","*",a,pp);
      matches = PackageMatch(name,version,arch,a,pp);
      }
   }

Verbose(" -> %d package(s) matching the name \"%s\" already installed\n",installed,name);
Verbose(" -> %d package(s) match the promise body's criteria fully\n",installed,name);

SchedulePackageOp(name,version,arch,installed,matches,no_version,a,pp);
}

/*****************************************************************************/

int ExecuteSchedule(struct CfPackageManager *schedule,enum package_actions action)

{ struct CfPackageItem *pi;
  struct CfPackageManager *pm;
  int size,estimated_size = 0,retval = true;
  char *command_string = NULL;
  struct Attributes a;
  struct Promise *pp;

for (pm = schedule; pm != NULL; pm = pm->next)
   {
   if (pm->action != action)
      {
      continue;
      }
   
   if (pm->pack_list == NULL)
      {
      continue;
      }
   
   for (pi = pm->pack_list; pi != NULL; pi=pi->next)
      {   
      size = strlen(pi->name) + strlen(" ");
      
      switch (pm->action)
         {
         case cfa_individual:             

             if (size > estimated_size)
                {
                estimated_size = size;
                }             
             break;
             
         case cfa_bulk:

             estimated_size += size;
             break;

         default:
             break;
         }
      }

   pp = pm->pack_list->pp;
   a = GetPackageAttributes(pp);

   switch (action)
      {
      case cfa_addpack:
          if (command_string = (malloc(estimated_size + strlen(a.packages.package_add_command) + 2)))
             {
             strcpy(command_string,a.packages.package_add_command);
             }
          break;

      case cfa_deletepack:
          if (command_string = (malloc(estimated_size + strlen(a.packages.package_delete_command) + 2)))
             {
             strcpy(command_string,a.packages.package_delete_command);
             }
          break;
          
      case cfa_update:
          if (command_string = (malloc(estimated_size + strlen(a.packages.package_update_command) + 2)))
             {
             strcpy(command_string,a.packages.package_update_command);
             }
          break;

      case cfa_patch:
          if (command_string = (malloc(estimated_size + strlen(a.packages.package_patch_command) + 2)))
             {
             strcpy(command_string,a.packages.package_patch_command);
             }
          break;

      }
      
   strcat(command_string," ");
   
   Verbose("Command prefix: %s\n",command_string);
   
   switch (pm->policy)
      {
      int ok;

      case cfa_individual:             

          for (pi = pm->pack_list; pi != NULL; pi=pi->next)
             {
             char *offset = command_string + strlen(command_string);
             
             strcpy(offset,pi->name);
             
             if (ExecPackageCommand(command_string,a,pp))
                {
                cfPS(cf_verbose,CF_CHG,"",pp,a,"Package schedule execution ok for %s (outcome cannot be promised by cf-agent)",pi->name);
                }
             else
                {
                cfPS(cf_verbose,CF_FAIL,"",pp,a,"Package schedule execution failed for %s",pi->name);
                }
             }
          
          break;
          
      case cfa_bulk:
          
          for (pi = pm->pack_list; pi != NULL; pi=pi->next)
             {
             strcat(command_string,pi->name);
             strcat(command_string," ");
             }

          ok = ExecPackageCommand(command_string,a,pp);

          for (pi = pm->pack_list; pi != NULL; pi=pi->next)
             {
             if (ok)
                {
                cfPS(cf_verbose,CF_CHG,"",pp,a,"Bulk package schedule execution ok for %s (outcome cannot be promised by cf-agent)",pi->name);
                }
             else
                {
                cfPS(cf_verbose,CF_INTERPT,"",pp,a,"Bulk package schedule execution failed somewhere - unknown outcome for %s",pi->name);
                }
             }
          
          break;
          
      default:
          break;
      }
   
   }

return retval;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

struct CfPackageManager *NewPackageManager(struct CfPackageManager **lists,char *mgr,enum package_actions pa,enum action_policy policy)

{ struct CfPackageManager *np;

if (mgr == NULL || strlen(mgr) == 0)
   {
   return NULL;
   }
 
for (np = *lists; np != NULL; np=np->next)
   {
   if ((strcmp(np->manager,mgr) == 0) && (policy == np->policy))
      {
      return np;
      }
   }
 
if ((np = (struct CfPackageManager *)malloc(sizeof(struct CfPackageManager))) == NULL)
   {
   CfOut(cf_error,"malloc","Can't allocate new package\n");
   return NULL;
   }

np->manager = strdup(mgr);
np->action = pa;
np->policy = policy;
np->pack_list = NULL;
np->next = *lists;
*lists = np;
return np;
}

/*****************************************************************************/

void DeletePackageManagers(struct CfPackageManager *newlist)

{ struct CfPackageManager *np,*next;

for (np = newlist; np != NULL; np = next)
   {
   next = np->next;
   DeletePackageItems(np->pack_list);
   free((char *)np);
   }
}

/*****************************************************************************/

int PrependListPackageItem(struct CfPackageItem **list,char *item,struct Attributes a,struct Promise *pp)

{ char name[CF_MAXVARSIZE];
  char arch[CF_MAXVARSIZE];
  char version[CF_MAXVARSIZE];
  char vbuff[CF_MAXVARSIZE];

if (!FullTextMatch(a.packages.package_installed_regex,item))
   {
   return false;
   }

strncpy(vbuff,ExtractFirstReference(a.packages.package_list_name_regex,item),CF_MAXVARSIZE-1);
sscanf(vbuff,"%s",name); /* trim */
strncpy(vbuff,ExtractFirstReference(a.packages.package_list_version_regex,item),CF_MAXVARSIZE-1);
sscanf(vbuff,"%s",version); /* trim */

if (a.packages.package_list_arch_regex)
   {
   strncpy(vbuff,ExtractFirstReference(a.packages.package_list_arch_regex,item),CF_MAXVARSIZE-1);
   sscanf(vbuff,"%s",arch); /* trim */
   }
else
   {
   strncpy(arch,"default",CF_MAXVARSIZE-1);
   }

Debug(" -? Extracted package name \"%s\"\n",name);
Debug(" -?      with version \"%s\"\n",version);
Debug(" -?      with architecture \"%s\"\n",arch);

return PrependPackageItem(list,name,version,arch,a,pp);
}

/*****************************************************************************/

void DeletePackageItems(struct CfPackageItem *pi)

{
if (pi)
   {
   free(pi->name);
   free(pi->version);
   free(pi->arch);
   DeletePromise(pi->pp);
   free(pi);
   }
}

/*****************************************************************************/

int PackageMatch(char *n,char *v,char *a,struct Attributes attr,struct Promise *pp)

{ struct CfPackageManager *mp = NULL;
  struct CfPackageItem  *pi;
  
for (mp = INSTALLED_PACKAGE_LISTS; mp != NULL; mp = mp->next)
   {
   if (strcmp(mp->manager,attr.packages.package_list_command) == 0)
      {
      break;
      }
   }

Verbose(" -> Looking for (%s,%s,%s)\n",n,v,a);

for (pi = mp->pack_list; pi != NULL; pi=pi->next)
   {
   if (ComparePackages(n,v,a,pi,attr.packages.package_select))
      {
      return true;
      }
   }

Verbose(" !! Unsatisfied constraints in promise (%s,%s,%s)\n",n,v,a);
return false;
}

/*****************************************************************************/

void SchedulePackageOp(char *name,char *version,char *arch,int installed,int matched,int no_version_specified,struct Attributes a,struct Promise *pp)

{ struct CfPackageManager *manager;
  char reference[CF_EXPANDSIZE];
  char *id;
  int package_select_in_range = false;
 
/* Now we need to know the name-convention expected by the package manager */

if (a.packages.package_name_convention)
   {
   SetNewScope("cf_pack_context");
   NewScalar("cf_pack_context","name",name,cf_str);
   NewScalar("cf_pack_context","version",version,cf_str);
   NewScalar("cf_pack_context","arch",arch,cf_str);
   ExpandScalar(a.packages.package_name_convention,reference);
   id = reference;
   DeleteScope("cf_pack_context");
   }
else
   {
   id = name;
   }

Verbose(" -> Package promises to refer to itself as \"%s\" to the manager\n",id);

if (a.packages.package_select == cfa_eq || a.packages.package_select == cfa_ge || a.packages.package_select == cfa_le)
   {
   package_select_in_range = true;
   }

switch(a.packages.package_policy)
   {
   case cfa_addpack:

       if (installed == 0)
          {
          Verbose(" -> Schedule package for addition\n");
          manager = NewPackageManager(&PACKAGE_SCHEDULE,a.packages.package_add_command,cfa_addpack,a.packages.package_changes);
          PrependPackageItem(&(manager->pack_list),id,"any","any",a,pp);
          }
       else
          {
          Verbose(" -> Package already installed, so we never add it again\n");
          }
       break;
       
   case cfa_deletepack:

       if (matched && package_select_in_range || installed && no_version_specified)
          {
          manager = NewPackageManager(&PACKAGE_SCHEDULE,a.packages.package_delete_command,cfa_deletepack,a.packages.package_changes);
          PrependPackageItem(&(manager->pack_list),id,"any","any",a,pp);
          }
       else
          {
          Verbose(" -> Package deletion cannot be promised -- no match\n");
          }
       
       break;
       
   case cfa_reinstall:

       if (!no_version_specified)
          {
          if (matched && package_select_in_range || installed && no_version_specified)
             {
             manager = NewPackageManager(&PACKAGE_SCHEDULE,a.packages.package_delete_command,cfa_deletepack,a.packages.package_changes);
             PrependPackageItem(&(manager->pack_list),id,"any","any",a,pp);
             }
          manager = NewPackageManager(&PACKAGE_SCHEDULE,a.packages.package_add_command,cfa_addpack,a.packages.package_changes);
          PrependPackageItem(&(manager->pack_list),id,"any","any",a,pp);
          }
       else
          {
          Verbose(" -> Package reinstallation cannot be promised -- insufficient version info or no match\n");
          }
       
       break;
       
   case cfa_update:

       if (matched && package_select_in_range && !no_version_specified || installed)
          {
          manager = NewPackageManager(&PACKAGE_SCHEDULE,a.packages.package_update_command,cfa_update,a.packages.package_changes);
          PrependPackageItem(&(manager->pack_list),id,"any","any",a,pp);
          }
       else
          {
          Verbose(" -> Package updating cannot be promised -- no match or not installed\n");
          }
       break;
       
   case cfa_patch:

       if (matched && package_select_in_range && !no_version_specified || installed)
          {
          manager = NewPackageManager(&PACKAGE_SCHEDULE,a.packages.package_patch_command,cfa_patch,a.packages.package_changes);
          PrependPackageItem(&(manager->pack_list),id,"any","any",a,pp);
          }
       else
          {
          Verbose(" -> Package patch update cannot be promised -- no match or not installed\n");
          }
       break;
       
   default:
       break;
   }
}

/*****************************************************************************/

int ExecPackageCommand(char *command,struct Attributes a,struct Promise *pp)

{ int offset = 0;
  char line[CF_BUFSIZE], *sp; 
  FILE *pfp;

if (!IsExecutable(GetArg0(command)))
   {
   cfPS(cf_error,CF_FAIL,"",pp,a,"The proposed package schedule command \"%s\" was not executable",command);
   return false;
   }

if (DONTDO)
   {
   printf(" Need to execute %39s...\n",command);
   return true;
   }

/* Use this form to avoid limited, intermediate argument processing - long lines */

if ((pfp = cf_popen_sh(command,"r")) == NULL)
   {
   cfPS(cf_error,CF_FAIL,"cf_popen",pp,a,"Couldn't start command %20s...\n",command);
   return false;
   }

Verbose("Executing %40s...\n",command);

while (!feof(pfp))
   {
   if (ferror(pfp))  /* abortable */
      {
      fflush(pfp);
      cfPS(cf_error,CF_INTERPT,"read",pp,a,"Couldn't start command %20s...\n",command);
      break;
      }

   ReadLine(line,CF_BUFSIZE-1,pfp);
   CfOut(cf_inform,"","Q: %s",line);
   
   if (ferror(pfp))  /* abortable */
      {
      fflush(pfp);
      cfPS(cf_error,CF_INTERPT,"read",pp,a,"Couldn't start command %20s...\n",command);
      break;
      }  
   }

cf_pclose(pfp);
return true; 
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ComparePackages(char *n,char *v,char *a,struct CfPackageItem *pi,enum version_cmp cmp)

{ struct Rlist *numbers_1 = NULL,*separators_1 = NULL;
  struct Rlist *numbers_2 = NULL,*separators_2 = NULL;
  struct Rlist *rp_1,*rp_2;
  int result = true;

Debug("Compare (%s,%s,%s) and (%s,%s,%s)\n",n,v,a,pi->name,pi->version,pi->arch);
  
if (strcmp(n,pi->name) != 0)
   {
   return false;
   }

Verbose(" -> Matched name %s\n",n);

if (strcmp(a,"*") != 0)
   {
   if (strcmp(a,pi->arch) != 0)
      {
      return false;
      }
   
   Verbose(" -> Matched arch %s\n",a);
   }

if (strcmp(v,"*") == 0)
   {   
   Verbose(" -> Matched version *\n");
   return true;
   }

ParsePackageVersion(v,numbers_1,separators_1);
ParsePackageVersion(pi->version,numbers_2,separators_2);

/* If the format of the version string doesn't match, we're already doomed */

for (rp_1 = separators_1,rp_2 = separators_2;
     rp_1 != NULL & rp_2 != NULL;
     rp_1= rp_1->next,rp_2=rp_2->next)
   {
   if (strcmp(rp_1->item,rp_2->item) != 0)
      {
      result = false;
      break;
      }
   }

Verbose(" -> Verified that versioning models are compatible\n");

if (result != false)
   {
   for (rp_1 = numbers_1,rp_2 = numbers_2;
        rp_1 != NULL & rp_2 != NULL;
        rp_1= rp_1->next,rp_2=rp_2->next)
      {
      switch (cmp)
         {
         case cfa_eq:
             if (strcmp(rp_1->item,rp_2->item) != 0)
                {
                result = false;
                }             
             break;
         case cfa_neq:
             if (strcmp(rp_1->item,rp_2->item) == 0)
                {
                result = false;
                }             
             break;
         case cfa_gt:
             if (strcmp(rp_1->item,rp_2->item) <= 0)
                {
                result = false;
                }
             break;
         case cfa_lt:
             if (strcmp(rp_1->item,rp_2->item) >= 0)
                {
                result = false;
                }             
             break;
         case cfa_ge:
             if (strcmp(rp_1->item,rp_2->item) < 0)
                {
                result = false;
                }             
             break;
         case cfa_le:
             if (strcmp(rp_1->item,rp_2->item) > 0)
                {
                result = false;
                }             
             break;
         default:
             break;
         }

      if (result == false)
         {
         break;
         }
      }
   }

Verbose(" -> Verified version constraint promise kept\n");

DeleteRlist(numbers_1);
DeleteRlist(numbers_2);
DeleteRlist(separators_1);
DeleteRlist(separators_2);
return result;
}

/*****************************************************************************/

int PrependPackageItem(struct CfPackageItem **list,char *name,char *version,char *arch,struct Attributes a,struct Promise *pp)

{ struct CfPackageItem *pi;

if (strlen(name) == 0 || strlen(version) == 0 || strlen(arch) == 0)
   {
   return false;
   }

if ((pi = (struct CfPackageItem *)malloc(sizeof(struct CfPackageItem))) == NULL)
   {
   CfOut(cf_error,"malloc","Can't allocate new package\n");
   return false;
   }

pi->next = *list;
pi->name = strdup(name);
pi->version = strdup(version);
pi->arch = strdup(arch);
*list = pi;

/* Finally we need these for later schedule exec, once this iteration context has gone */

pi->pp = DeRefCopyPromise("this",pp);
return true;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void ParsePackageVersion(char *version,struct Rlist *num,struct Rlist *sep)

{ char *sp,numeral[30],separator[2];

for (sp = version; *sp != '\0'; sp++)
   {
   memset(numeral,0,30);
   memset(separator,0,2);

   /* Split in 2's complement */
   
   sscanf(sp,"%29[0-9a-zA-Z]",numeral);
   sp += strlen(numeral);
   sscanf(sp,"%1[^0-9a-zA-Z]",separator);
   
   /* Prepend to end up with right->left comparison */

   PrependRScalar(&num,numeral,CF_SCALAR);
   PrependRScalar(&sep,separator,CF_SCALAR);
   }
}







