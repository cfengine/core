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

a = GetPackageAttributes(pp);

if (!PackageSanityCheck(a,pp))
   {
   return;
   }

if (!VerifyInstalledPackages(&INSTALLED_PACKAGE_LISTS,a,pp))
   {
   cfPS(cf_error,CF_FAIL,"",pp,a," !! Unable to obtain a list of installed packages - aborting");
   return;
   }

VerifyPromisedPackage(a,pp);

// Now got the existing packages

// Extract the name and version from the file and see if it exists

// Build the action schedule for ExecuteSchedule finale

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


return true;
}

/*****************************************************************************/

void ExecutePackageSchedule(struct Rlist *schedule)

{
}
          
/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int VerifyInstalledPackages(struct CfPackageManager **all_mgrs,struct Attributes a,struct Promise *pp)

{ struct CfPackageManager *manager = NewPackageManager(all_mgrs,a.packages.package_list_command);
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
      
      if (!PrependPackageItem(&(manager->pack_list),vbuff,a,pp))
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

{
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

struct CfPackageManager *NewPackageManager(struct CfPackageManager **lists,char *mgr)

{ struct CfPackageManager *np;

if (mgr == NULL || strlen(mgr) == 0)
   {
   return NULL;
   }
 
for (np = *lists; np != NULL; np=np->next)
   {
   if (strcmp(np->manager,mgr) == 0)
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

int PrependPackageItem(struct CfPackageItem **list,char *item,struct Attributes a,struct Promise *pp)

{ struct CfPackageItem *pi;
  char name[CF_MAXVARSIZE];
  char arch[CF_MAXVARSIZE];
  char version[CF_MAXVARSIZE];

if (!FullTextMatch(a.packages.package_installed_regex,item))
   {
   return false;
   }

strncpy(name,ExtractFirstReference(a.packages.package_list_name_regex,item),CF_MAXVARSIZE-1);
strncpy(version,ExtractFirstReference(a.packages.package_list_version_regex,item),CF_MAXVARSIZE-1);

if (a.packages.package_list_arch_regex)
   {
   strncpy(arch,ExtractFirstReference(a.packages.package_list_arch_regex,item),CF_MAXVARSIZE-1);
   }
else
   {
   strncpy(arch,"default",CF_MAXVARSIZE-1);
   }

Verbose(" -? Extracted package name %s\n",name);
Verbose(" -?      with version %s\n",version);
Verbose(" -?      with architecture %s\n",arch);

if (strlen(name) == 0 || strlen(version) == 0)
   {
   return false;
   }

for (pi = *list; pi != NULL; pi=pi->next)
   {
   if (strcmp(pi->name,name) == 0 && strcmp(pi->version,version) == 0 && strcmp(pi->arch,arch) == 0)
      {
      return true;
      }
   }
 
if ((pi = (struct CfPackageItem *)malloc(sizeof(struct CfPackageItem))) == NULL)
   {
   CfOut(cf_error,"malloc","Can't allocate new package\n");
   return false;
   }

pi->name = strdup(name);
pi->version = strdup(version);
pi->arch = strdup(arch);

return true;
}

/*****************************************************************************/

void DeletePackageItems(struct CfPackageItem *pi)

{
if (pi)
   {
   free(pi->name);
   free(pi->version);
   free(pi->arch);
   free(pi);
   }
}
