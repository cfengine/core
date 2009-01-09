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
/* BSD flags.c                                                               */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/***************************************************************/
/* Flagstring toolkit                                          */
/***************************************************************/

/* adapted from FreeBSD /usr/src/bin/ls/stat_flags.c */

#define SETBIT(a, b, f) {       \
 if (!memcmp(a, b, sizeof(b))) {     \
  if (clear) {      \
   if (minusmask)     \
    *minusmask |= (f);   \
  } else if (plusmask)     \
   *plusmask |= (f);    \
  break;       \
 }        \
}

/***************************************************************/

int ParseFlagString(char *flagstring,u_long *plusmask,u_long *minusmask)

{ char *sp, *next;
  int clear;
  u_long value;

value = 0;

if (flagstring == NULL)
   {
   return true;
   }

Debug1("ParseFlagString(%s)\n",flagstring);

for (sp = flagstring; sp && *sp; sp = next)
   {
   if ( next = strchr( sp, ',' ))
      {
      *next ++ = '\0';
      }
   
   clear = 0;
   
   if ( sp[0] == 'n' && sp[1] == 'o' )
      {
      clear = 1;
      sp += 2;
      }
   
   switch ( *sp )
      {
      case 'a':
          SETBIT(sp, "arch", SF_ARCHIVED);
          SETBIT(sp, "archived", SF_ARCHIVED);
          goto Error;
      case 'd':
          clear = !clear;
          SETBIT(sp, "dump", UF_NODUMP);
          goto Error;
      case 'o':
          SETBIT(sp, "opaque", UF_OPAQUE);
          goto Error;
      case 's':
          SETBIT(sp, "sappnd", SF_APPEND);
          SETBIT(sp, "sappend", SF_APPEND);
          SETBIT(sp, "schg", SF_IMMUTABLE);
          SETBIT(sp, "schange", SF_IMMUTABLE);
          SETBIT(sp, "simmutable", SF_IMMUTABLE);
          SETBIT(sp, "sunlnk", SF_NOUNLINK);
          SETBIT(sp, "sunlink", SF_NOUNLINK);
          goto Error;
      case 'u':
          SETBIT(sp, "uappnd", UF_APPEND);
          SETBIT(sp, "uappend", UF_APPEND);
          SETBIT(sp, "uchg", UF_IMMUTABLE);
          SETBIT(sp, "uchange", UF_IMMUTABLE);
          SETBIT(sp, "uimmutable", UF_IMMUTABLE);
          SETBIT(sp, "uunlnk", UF_NOUNLINK);
          SETBIT(sp, "uunlink", UF_NOUNLINK);
          goto Error;
          
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
          sscanf(sp,"%o",&value);
          *plusmask = value;
          *minusmask = ~value & CHFLAGS_MASK;
          break;
          
      case '\0':
          break;
          
      Error:
      default:
          printf( "Invalid flag string '%s'\n", sp );
          yyerror ("Invalid flag string");
          return false;
      }
   }

Debug("ParseFlagString:[PLUS=%o][MINUS=%o]\n",*plusmask,*minusmask);
return true;
}


/*
CHFLAGS(1)              FreeBSD General Commands Manual             CHFLAGS(1)

NAME
     chflags - change file flags

SYNOPSIS
     chflags [-R [-H | -L | -P]] flags file ...

DESCRIPTION
     The chflags utility modifies the file flags of the listed files as speci-
     fied by the flags operand.

     The options are as follows:

     -H      If the -R option is specified, symbolic links on the command line
             are followed.  (Symbolic links encountered in the tree traversal
             are not followed.)

     -L      If the -R option is specified, all symbolic links are followed.

     -P      If the -R option is specified, no symbolic links are followed.

     -R      Change the file flags for the file hierarchies rooted in the
             files instead of just the files themselves.

     Flags are a comma separated list of keywords.  The following keywords are
     currently defined:

           arch    set the archived flag (super-user only)
           dump    set the dump flag
           sappnd  set the system append-only flag (super-user only)
           schg    set the system immutable flag (super-user only)
           sunlnk  set the system undeletable flag (super-user only)
           uappnd  set the user append-only flag (owner or super-user only)
           uchg    set the user immutable flag (owner or super-user only)
           uunlnk  set the user undeletable flag (owner or super-user only)
           archived, sappend, schange, simmutable, uappend, uchange, uimmutable,
           sunlink, uunlink
                   aliases for the above

     Putting the letters ``no'' before an option causes the flag to be turned
     off.  For example:

           nodump  the file should never be dumped

     Symbolic links do not have flags, so unless the -H or -L option is set,
     chflags on a symbolic link always succeeds and has no effect.  The -H, -L
     and -P options are ignored unless the -R option is specified.  In addi-
     tion, these options override each other and the command's actions are de-
     termined by the last one specified.

     You can use "ls -lo" to see the flags of existing files.

     The chflags utility exits 0 on success, and >0 if an error occurs.

SEE ALSO
     ls(1),  chflags(2),  stat(2),  fts(3),  symlink(7)

HISTORY
     The chflags command first appeared in 4.4BSD.

BSD                             March 31, 1994                               1
*/
