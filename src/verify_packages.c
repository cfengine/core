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

if (!VerifyInstalledPackages(&INSTALLEDPACKAGELISTS,a,pp))
   {
   cfPS(cf_error,CF_FAIL,"",pp,a," !! Unable to obtain a list of installed packages - aborting");
   return;
   }

VerifyPromisedPackage(a,pp);
}

/*****************************************************************************/

int PackageSanityCheck(struct Attributes a,struct Promise *pp)

{ int must_supply_version = false;

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

if (a.packages.package_name_regex && a.packages.package_name_regex)
   {
   must_supply_version = true;
   }

if (a.packages.package_version)
   {
   }


return true;
}

/*****************************************************************************/



/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int VerifyInstalledPackages(struct CfPackageList **alllists,struct Attributes a,struct Promise *pp)

{ struct CfPackageManager *manager = NewPackageManager(alllists,a.packages.package_list_command);
  FILE *prp;
  char vbuff[CF_BUFSIZE];
  
if (!manager)
   {
   return false;
   }

if (manager->pack_list != NULL)
   {
   return true;
   }

if (!IsExecutable(GetArg0(a.packages.package_list_command)))
   {
   CfOut(cf_error,"","The proposed package list command \"%s\" was not executable",a.packages.package_list_command);
   return false;
   }
  
if ((prp = cf_popen(pscomm,"r")) == NULL)
   {
   CfOut(cf_error,"popen","Couldn't open the package list with command %s\n",comm);
   return false;
   }

while (!feof(prp))
   {
   memset(vbuff,0,CF_BUFSIZE);
   ReadLine(vbuff,CF_BUFSIZE,prp);   
   if (!PrependPackageItem(&(manager->pack_list),vbuff,a,pp))
      {
      cf_pclose(prp);
      return false;
      }
   }

cf_pclose(prp);
}

/*****************************************************************************/

void VerifyPromisedPackage(struct Attributes a,struct Promise *pp)

{
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

struct CfPackageManager *NewPackageManager(struct CfPackageList **lists,char *mgr)

{ struct CfPackageManager *np;

if (mgr == NULL || strlen(mgr) == 0)
   {
   return NULL;
   }
 
for (np = lists; np != NULL; np=np->next)
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

void DeletePackageManager(struct CfPackageManager *newlist)

{ struct CfPackageManager *np,*next;

for (np = lists; np != NULL; np = next)
   {
   next = np->next;
   DeletePackageItems(np->pack_list);
   free((char *)np);
   }
}

/*****************************************************************************/

int PrependPackageItem(struct CfPackageItem **list,char *item,struct Attributes a,struct Promise *pp)

{ struct CfPackageItem *ppp;
  char name[CF_MAXVARSIZE];
  char arch[CF_MAXVARSIZE];
  char version[CF_MAXVARSIZE];
 
strncpy(name,ExtractFirstReference(item,a.packages.package_list_version_regex),CF_MAXVARSIZE-1);
strncpy(version,ExtractFirstReference(item,a.packages.package_list_version_regex),CF_MAXVARSIZE-1);

if (a.packages.package_arch_version_regex)
   {
   strncpy(arch,ExtractFirstReference(item,a.packages.package_arch_version_regex),CF_MAXVARSIZE-1);
   }
else
   {
   strncpy(arch,"default",CF_MAXVARSIZE-1);
   }

if (strlen(name) == 0 || strlen(version) == 0)
   {
   CfOut(cf_error,"","Failed to extract the name and version of the package (check the body regex)");
   return false;
   }

for (ppp = list; ppp != NULL; ppp=ppp->next)
   {
   if (strcmp(ppp->name,name) == 0 && strcmp(ppp->version,version) == 0 && strcmp(ppp->arch,arch) == 0)
      {
      return true;
      }
   }
 
if ((ppp = (struct CfPackageItem *)malloc(sizeof(struct CfPackageItem))) == NULL)
   {
   CfOut(cf_error,"malloc","Can't allocate new package\n");
   return NULL;
   }

ppp->name = strdup(name);
ppp->version = strdup(version);
pp->arch = strdup(arch);
return true;
}

