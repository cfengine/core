
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

void MapPromiseToTopic(struct Promise *pp,char *version)
{
#ifdef HAVE_LIBCFNOVA
 Nova_MapPromiseToTopic(pp,version); 
#else
 Verbose("MapPromiseToTopic is only available in commerical version Nova and above");
#endif
}

/*****************************************************************************/

void ShowTopicRepresentation(void)
{
#ifdef HAVE_LIBCF_NOVA
 Nova_ShowTopicRepresentation();
#else
 Verbose("ShowTopicRepresentation is only available in commerical version Nova and above");
#endif
}


