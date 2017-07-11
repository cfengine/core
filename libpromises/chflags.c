/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/* BSD flags */

#include <chflags.h>

typedef struct
{
    char *name;
    u_long bits;
} BSDFlag;

static const BSDFlag CF_BSDFLAGS[] =
{
    {"arch", (u_long) SF_ARCHIVED},
    {"archived", (u_long) SF_ARCHIVED},
    {"nodump", (u_long) UF_NODUMP},
    {"opaque", (u_long) UF_OPAQUE},
    {"sappnd", (u_long) SF_APPEND},
    {"sappend", (u_long) SF_APPEND},
    {"schg", (u_long) SF_IMMUTABLE},
    {"schange", (u_long) SF_IMMUTABLE},
    {"simmutable", (u_long) SF_IMMUTABLE},
    {"sunlnk", (u_long) SF_NOUNLINK},
    {"sunlink", (u_long) SF_NOUNLINK},
    {"uappnd", (u_long) UF_APPEND},
    {"uappend", (u_long) UF_APPEND},
    {"uchg", (u_long) UF_IMMUTABLE},
    {"uchange", (u_long) UF_IMMUTABLE},
    {"uimmutable", (u_long) UF_IMMUTABLE},
    {"uunlnk", (u_long) UF_NOUNLINK},
    {"uunlink", (u_long) UF_NOUNLINK},
    {NULL, (u_long) 0}
};

/***************************************************************/

static u_long ConvertBSDBits(const char *s);

/***************************************************************/

int ParseFlagString(Rlist *bitlist, u_long *plusmask, u_long *minusmask)
{
    if (bitlist == NULL)
    {
        return true;
    }

    *plusmask = 0;
    *minusmask = 0;

    for (const Rlist *rp = bitlist; rp != NULL; rp = rp->next)
    {
        const char *flag = RlistScalarValue(rp);
        char op = *RlistScalarValue(rp);

        switch (op)
        {
        case '-':
            *minusmask |= ConvertBSDBits(flag + 1);
            break;

        case '+':
            *plusmask |= ConvertBSDBits(flag + 1);
            break;

        default:
            *plusmask |= ConvertBSDBits(flag);
            break;

        }
    }

    Log(LOG_LEVEL_DEBUG, "ParseFlagString: [PLUS = %lo] [MINUS = %lo]", *plusmask, *minusmask);
    return true;
}

/***************************************************************/

static u_long ConvertBSDBits(const char *s)
{
    int i;

    for (i = 0; CF_BSDFLAGS[i].name != NULL; i++)
    {
        if (strcmp(s, CF_BSDFLAGS[i].name) == 0)
        {
            return CF_BSDFLAGS[i].bits;
        }
    }

    return 0;
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
