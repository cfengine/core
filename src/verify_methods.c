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
/* File: verify_methods.c                                                    */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void VerifyMethodsPromise(struct Promise *pp)

{ struct Attributes a;

a = GetMethodAttributes(pp);

VerifyMethod(a,pp);
DeleteScalar("this","promiser");
}

/*****************************************************************************/

int VerifyMethod(struct Attributes a,struct Promise *pp)

{ struct Bundle *bp;
  void *vp;
  struct FnCall *fp;
  char *method_name = NULL;
  struct Rlist *params = NULL;
  int retval = false;
  struct CfLock thislock;
  char lockname[CF_BUFSIZE];

if (a.havebundle)
   {
   if (vp = GetConstraint("usebundle",pp->conlist,CF_FNCALL))
      {
      fp = (struct FnCall *)vp;
      method_name = fp->name;
      params = fp->args;;
      }
   else if (vp = GetConstraint("usebundle",pp->conlist,CF_SCALAR))
      {
      method_name = (char *)vp;
      params = NULL;
      }
   else
      {
      return false;
      }   
   }

GetLockName(lockname,"method",pp->promiser,params);
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return false;
   }

PromiseBanner(pp);

if (bp = GetBundle(method_name,"agent"))
   {
   BannerSubBundle(bp,params);
   NewScope(bp->name);
   AugmentScope(bp->name,bp->args,params);
   retval = ScheduleAgentOperations(bp);
   
   if (retval)
      {
      cfPS(cf_inform,CF_CHG,"",pp,a,"Method invoked successfully\n");
      }
   else
      {
      cfPS(cf_inform,CF_FAIL,"",pp,a,"Method could not be invoked successfully\n");
      }
   
   DeleteFromScope(bp->name,bp->args);
   }

YieldCurrentLock(thislock);
return retval;
}

