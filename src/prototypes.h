/* cfengine for GNU
 
        Copyright (C) 1995
        Free Software Foundation, Inc.
 
   This file is part of GNU cfengine - written and maintained 
   by Cfengine AS, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */
 

/*******************************************************************/
/*                                                                 */
/*  cfengine function prototypes                                   */
/*                                                                 */
/*  contributed by Stuart Sharpe, September 2000                   */
/*                                                                 */
/*******************************************************************/


/* pub/full-write.c */

int cf_full_write (int desc, char *ptr, size_t len);

/* acl.c */

void aclsortperror (int error);

#if defined SOLARIS && defined HAVE_SYS_ACL_H
struct acl;
enum cffstype StringToFstype (char *string);
struct CFACL *GetACL (char *acl_alias);
int ParseSolarisMode (char* mode, mode_t oldmode);
int BuildAclEntry (struct stat *sb, char *acltype, char *name, struct acl *newaclbufp);
#endif

void InstallACL (char *alias, char *classes);

void AddACE (char *acl, char *string, char *classes);
int CheckACLs (char *filename, enum fileactions action, struct Item *acl_aliases);
enum cffstype StringToFstype (char *string);
struct CFACL *GetACL (char *acl_alias);

int CheckPosixACE (struct CFACE *aces, char method, char *filename, enum fileactions action);


#ifdef HPuUX
int Error;
#endif
