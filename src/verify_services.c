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
/* File: verify_services.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static int ServicesSanityChecks(struct Attributes a,struct Promise *pp);
static void SetServiceDefaults(struct Attributes *a);
/*****************************************************************************/

void VerifyServicesPromise(struct Promise *pp)

{ struct Attributes a = {{0}};

a = GetServicesAttributes(pp);

SetServiceDefaults(&a);

if (ServicesSanityChecks(a,pp))
   {
   VerifyServices(a,pp);
   }
}

/*****************************************************************************/

static int ServicesSanityChecks(struct Attributes a,struct Promise *pp)
    
{ struct Rlist *dep;

 switch(a.service.service_policy)
    {
    case cfsrv_start:
        break;
        
    case cfsrv_stop:
    case cfsrv_disable:
        if(strcmp(a.service.service_autostart_policy, "none") != 0)
           {
           CfOut(cf_error,"","!! Autostart policy of service promiser \"%s\" needs to be \"none\" when service policy is not \"start\", but is \"%s\"",
                 pp->promiser, a.service.service_autostart_policy);
           PromiseRef(cf_error,pp);
           return false;
           }
        break;
        
    default:
        CfOut(cf_error,"","!! Invalid service policy for service \"%s\"", pp->promiser);
        PromiseRef(cf_error,pp);
        return false;
    }

 for(dep = a.service.service_depend; dep != NULL; dep = dep->next)
    {
    if(strcmp(pp->promiser, dep->item) == 0)
       {
       CfOut(cf_error,"","!! Service promiser \"%s\" has itself as dependency", pp->promiser);
       PromiseRef(cf_error,pp);
       return false;
       }
    }

 if(a.service.service_type ==  NULL)
    {
    CfOut(cf_error,"","!! Service type for service \"%s\" is not known", pp->promiser);
    PromiseRef(cf_error,pp);
    return false;
    }

#ifdef MINGW

 if(strcmp(a.service.service_type, "windows") != 0)
    {
    CfOut(cf_error,"","!! Service type for promiser \"%s\" must be \"windows\" on this system, but is \"%s\"", pp->promiser, a.service.service_type);
    PromiseRef(cf_error,pp);
    return false;
    }
 
#endif  /* MINGW */

 return true;
}

/*****************************************************************************/

static void SetServiceDefaults(struct Attributes *a)
{
 
 if(a->service.service_autostart_policy == NULL)
    {
    a->service.service_autostart_policy = "none";
    }

 if(a->service.service_depend_chain == NULL)
    {
    a->service.service_depend_chain = "ignore";
    }

 
 // default service type to "windows" on windows platforms
#ifdef MINGW
 
 if(a->service.service_type == NULL)
    {
    a->service.service_type = "windows";
    }
 
#endif  /* MINGW */
     
}








