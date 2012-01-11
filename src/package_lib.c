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

#ifdef HAVE_CONFIG_H
# include <conf.h>
#endif

#include "cf3.defs.h"
#include "prototypes3.h"

int PrependPackageItem(struct CfPackageItem **list, const char *name, const char *version, const char *arch,Attributes a,Promise *pp)

{ struct CfPackageItem *pi;

if (strlen(name) == 0 || strlen(version) == 0 || strlen(arch) == 0)
   {
   return false;
   }

CfOut(cf_verbose,""," -> Package (%s,%s,%s) found",name,version,arch);

pi = xmalloc(sizeof(struct CfPackageItem));

if (list)
   {
   pi->next = *list;
   }
else
   {
   pi->next = NULL;
   }

pi->name = xstrdup(name);
pi->version = xstrdup(version);
pi->arch = xstrdup(arch);
*list = pi;

/* Finally we need these for later schedule exec, once this iteration context has gone */

pi->pp = DeRefCopyPromise("this",pp);
return true;
}

/*****************************************************************************/

int PrependListPackageItem(struct CfPackageItem **list,char *item,Attributes a,Promise *pp)

{ char name[CF_MAXVARSIZE];
  char arch[CF_MAXVARSIZE];
  char version[CF_MAXVARSIZE];
  char vbuff[CF_MAXVARSIZE];

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

if (strcmp(name,"CF_NOMATCH") == 0 || strcmp(version,"CF_NOMATCH") == 0 || strcmp(arch,"CF_NOMATCH") == 0)
   {
   return false;
   }

CfDebug(" -? Package line \"%s\"\n",item);
CfDebug(" -?      with name \"%s\"\n",name);
CfDebug(" -?      with version \"%s\"\n",version);
CfDebug(" -?      with architecture \"%s\"\n",arch);

return PrependPackageItem(list,name,version,arch,a,pp);
}
