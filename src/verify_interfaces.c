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
/* File: verify_interfaces.c                                                 */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void VerifyInterfacesPromise(struct Promise *pp)

{ struct Attributes a;

a = GetInterfacesAttributes(pp);
VerifyInterface(a,pp);
DeleteScalar("this","promiser");
}

/*****************************************************************************/

void VerifyInterface(struct Attributes a,struct Promise *pp)

{
/* How shall we deal with this now ? */

 printf("NOT YET IMPLEMENTED\n");
}
