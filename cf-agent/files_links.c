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

#include "files_links.h"

#include "promises.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "files_lib.h"
#include "transaction.h"
#include "cfstream.h"
#include "logging.h"
#include "string_lib.h"

#define CF_MAXLINKLEVEL 4

#if !defined(__MINGW32__)
static int MakeLink(const char *from, const char *to, Attributes attr, Promise *pp);
#endif
static char *AbsLinkPath(const char *from, const char *relto);

/*****************************************************************************/

#ifdef __MINGW32__

char VerifyLink(char *destination, const char *source, Attributes attr, Promise *pp)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Windows does not support symbolic links (at VerifyLink())");
    return CF_FAIL;
}

#else

char VerifyLink(char *destination, const char *source, Attributes attr, Promise *pp)
{
    char to[CF_BUFSIZE], linkbuf[CF_BUFSIZE], absto[CF_BUFSIZE];
    struct stat sb;

    CfDebug("Linkfiles(%s -> %s)\n", destination, source);

    memset(to, 0, CF_BUFSIZE);

    if ((!IsAbsoluteFileName(source)) && (*source != '.'))        /* links without a directory reference */
    {
        snprintf(to, CF_BUFSIZE - 1, "./%s", source);
    }
    else
    {
        strncpy(to, source, CF_BUFSIZE - 1);
    }

    if (!IsAbsoluteFileName(to))        /* relative path, must still check if exists */
    {
        CfDebug("Relative link destination detected: %s\n", to);
        strcpy(absto, AbsLinkPath(destination, to));
        CfDebug("Absolute path to relative link = %s, destination %s\n", absto, destination);
    }
    else
    {
        strcpy(absto, to);
    }

    bool source_file_exists = true;

    if (cfstat(absto, &sb) == -1)
    {
        CfDebug("No source file\n");
        source_file_exists = false;
    }

    if ((!source_file_exists) && (attr.link.when_no_file != cfa_force) && (attr.link.when_no_file != cfa_delete))
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Source %s for linking is absent", absto);
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, attr, " !! Unable to create link %s -> %s, no source", destination, to);
        return CF_WARN;
    }

    if ((!source_file_exists) && (attr.link.when_no_file == cfa_delete))
    {
        KillGhostLink(destination, attr, pp);
        return CF_CHG;
    }

    memset(linkbuf, 0, CF_BUFSIZE);

    if (readlink(destination, linkbuf, CF_BUFSIZE - 1) == -1)
    {

        if (!MakeParentDirectory2(destination, attr.move_obstructions, EnforcePromise(attr.transaction.action)))
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, " !! Unable to create parent directory of link %s -> %s (enforce=%d)",
                 destination, to, EnforcePromise(attr.transaction.action));
            return CF_FAIL;
        }
        else
        {
            if (!MoveObstruction(destination, attr, pp))
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, attr, " !! Unable to create link %s -> %s", destination, to);
                return CF_FAIL;
            }

            return MakeLink(destination, source, attr, pp) ? CF_CHG : CF_FAIL;
        }
    }
    else
    {
        int ok = false;

        if ((attr.link.link_type == FILE_LINK_TYPE_SYMLINK) && (strcmp(linkbuf, to) != 0) && (strcmp(linkbuf, source) != 0))
        {
            ok = true;
        }
        else if (strcmp(linkbuf, source) != 0)
        {
            ok = true;
        }

        if (ok)
        {
            if (attr.move_obstructions)
            {
                if (!DONTDO)
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, "Overriding incorrect link %s\n", destination);

                    if (unlink(destination) == -1)
                    {
                        cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, attr, " !! Link %s points to %s not %s - error removing link",
                             destination, linkbuf, to);
                        return CF_FAIL;
                    }

                    return MakeLink(destination, source, attr, pp);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", " !! Must remove incorrect link %s\n", destination);
                    return CF_NOP;
                }
            }
            else
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, attr, " !! Link %s points to %s not %s - not authorized to override",
                     destination, linkbuf, to);
                return true;
            }
        }
        else
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, attr, " -> Link %s points to %s - promise kept", destination, source);
            return CF_NOP;
        }
    }
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

char VerifyAbsoluteLink(char *destination, const char *source, Attributes attr, Promise *pp)
{
    char absto[CF_BUFSIZE];
    char expand[CF_BUFSIZE];
    char linkto[CF_BUFSIZE];

    CfDebug("VerifyAbsoluteLink(%s,%s)\n", destination, source);

    if (*source == '.')
    {
        strcpy(linkto, destination);
        ChopLastNode(linkto);
        AddSlash(linkto);
        strcat(linkto, source);
    }
    else
    {
        strcpy(linkto, source);
    }

    CompressPath(absto, linkto);

    expand[0] = '\0';

    if (attr.link.when_no_file == cfa_force)
    {
        if (!ExpandLinks(expand, absto, 0))     /* begin at level 1 and beam out at 15 */
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Failed to make absolute link in\n");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            return CF_FAIL;
        }
        else
        {
            CfDebug("ExpandLinks returned %s\n", expand);
        }
    }
    else
    {
        strcpy(expand, absto);
    }

    CompressPath(linkto, expand);

    return VerifyLink(destination, linkto, attr, pp);
}

/*****************************************************************************/

char VerifyRelativeLink(char *destination, const char *source, Attributes attr, Promise *pp)
{
    char *sp, *commonto, *commonfrom;
    char buff[CF_BUFSIZE], linkto[CF_BUFSIZE], add[CF_BUFSIZE];
    int levels = 0;

    CfDebug("RelativeLink(%s,%s)\n", destination, source);

    if (*source == '.')
    {
        return VerifyLink(destination, source, attr, pp);
    }

    if (!CompressPath(linkto, source))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, " !! Failed to link %s to %s\n", destination, source);
        return CF_FAIL;
    }

    commonto = linkto;
    commonfrom = destination;

    if (strcmp(commonto, commonfrom) == 0)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "", pp, attr, " !! Failed to link %s to %s - can't link file %s to itself\n",
             destination, source, commonto);
        return CF_FAIL;
    }

    while (*commonto == *commonfrom)
    {
        commonto++;
        commonfrom++;
    }

    while (!((IsAbsoluteFileName(commonto)) && (IsAbsoluteFileName(commonfrom))))
    {
        commonto--;
        commonfrom--;
    }

    commonto++;

    for (sp = commonfrom; *sp != '\0'; sp++)
    {
        if (IsFileSep(*sp))
        {
            levels++;
        }
    }

    memset(buff, 0, CF_BUFSIZE);

    strcat(buff, ".");
    strcat(buff, FILE_SEPARATOR_STR);

    while (--levels > 0)
    {
        snprintf(add, CF_BUFSIZE - 1, "..%c", FILE_SEPARATOR);

        if (!JoinPath(buff, add))
        {
            return CF_FAIL;
        }
    }

    if (!JoinPath(buff, commonto))
    {
        return CF_FAIL;
    }

    return VerifyLink(destination, buff, attr, pp);
}

/*****************************************************************************/

char VerifyHardLink(char *destination, const char *source, Attributes attr, Promise *pp)
{
    char to[CF_BUFSIZE], absto[CF_BUFSIZE];
    struct stat ssb, dsb;

    memset(to, 0, CF_BUFSIZE);

    if ((!IsAbsoluteFileName(source)) && (*source != '.'))        /* links without a directory reference */
    {
        snprintf(to, CF_BUFSIZE - 1, ".%c%s", FILE_SEPARATOR, source);
    }
    else
    {
        strncpy(to, source, CF_BUFSIZE - 1);
    }

    if (!IsAbsoluteFileName(to))        /* relative path, must still check if exists */
    {
        CfDebug("Relative link destination detected: %s\n", to);
        strcpy(absto, AbsLinkPath(destination, to));
        CfDebug("Absolute path to relative link = %s, destination %s\n", absto, destination);
    }
    else
    {
        strcpy(absto, to);
    }

    if (cfstat(absto, &ssb) == -1)
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, attr, " !! Source file %s doesn't exist\n", source);
        return CF_WARN;
    }

    if (!S_ISREG(ssb.st_mode))
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, attr,
             " !! Source file %s is not a regular file, not appropriate to hard-link\n", to);
        return CF_WARN;
    }

    CfDebug("Trying to (hard) link %s -> %s\n", destination, to);

    if (cfstat(destination, &dsb) == -1)
    {
        return MakeHardLink(destination, to, attr, pp) ? CF_CHG : CF_FAIL;
    }

    /* both files exist, but are they the same file? POSIX says  */
    /* the files could be on different devices, but unix doesn't */
    /* allow this behaviour so the tests below are theoretical... */

    if ((dsb.st_ino != ssb.st_ino) && (dsb.st_dev != ssb.st_dev))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! If this is POSIX, unable to determine if %s is hard link is correct\n", destination);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! since it points to a different filesystem!\n");

        if ((dsb.st_mode == ssb.st_mode) && (dsb.st_size == ssb.st_size))
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, attr, "Hard link (%s->%s) on different device APPEARS okay\n", destination,
                 to);
            return CF_NOP;
        }
    }

    if ((dsb.st_ino == ssb.st_ino) && (dsb.st_dev == ssb.st_dev))
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, attr, " -> Hard link (%s->%s) exists and is okay\n", destination, to);
        return CF_NOP;
    }

    CfOut(OUTPUT_LEVEL_INFORM, "", " !! %s does not appear to be a hard link to %s\n", destination, to);

    if (!MoveObstruction(destination, attr, pp))
    {
        return CF_FAIL;
    }

    return MakeHardLink(destination, to, attr, pp) ? CF_CHG : CF_FAIL;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

#ifdef __MINGW32__

int KillGhostLink(const char *name, Attributes attr, const Promise *pp)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Windows does not support symbolic links (at KillGhostLink())");
    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, attr, " !! Windows does not support killing link \"%s\"", name);
    return false;
}

#else                           /* !__MINGW32__ */

int KillGhostLink(const char *name, Attributes attr, const Promise *pp)
{
    char linkbuf[CF_BUFSIZE], tmp[CF_BUFSIZE];
    char linkpath[CF_BUFSIZE], *sp;
    struct stat statbuf;

    CfDebug("KillGhostLink(%s)\n", name);

    memset(linkbuf, 0, CF_BUFSIZE);
    memset(linkpath, 0, CF_BUFSIZE);

    if (readlink(name, linkbuf, CF_BUFSIZE - 1) == -1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! (Can't read link %s while checking for deadlinks)\n", name);
        return true;            /* ignore */
    }

    if (!IsAbsoluteFileName(linkbuf))
    {
        strcpy(linkpath, name); /* Get path to link */

        for (sp = linkpath + strlen(linkpath); (*sp != FILE_SEPARATOR) && (sp >= linkpath); sp--)
        {
            *sp = '\0';
        }
    }

    strcat(linkpath, linkbuf);
    CompressPath(tmp, linkpath);

    if (cfstat(tmp, &statbuf) == -1)    /* link points nowhere */
    {
        if ((attr.link.when_no_file == cfa_delete) || (attr.recursion.rmdeadlinks))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! %s is a link which points to %s, but that file doesn't seem to exist\n", name,
                  linkbuf);

            if (!DONTDO)
            {
                unlink(name);   /* May not work on a client-mounted system ! */
                cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr,
                     " -> Removing ghost %s - reference to something that is not there\n", name);
                return true;
            }
        }
    }

    return false;
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

#if !defined(__MINGW32__)
static int MakeLink(const char *from, const char *to, Attributes attr, Promise *pp)
{
    if (DONTDO || (attr.transaction.action == cfa_warn))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Need to link files %s -> %s\n", from, to);
        return false;
    }
    else
    {
        if (symlink(to, from) == -1)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "symlink", pp, attr, " !! Couldn't link %s to %s\n", to, from);
            return false;
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> Linked files %s -> %s\n", from, to);
            return true;
        }
    }
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

#ifdef __MINGW32__

int MakeHardLink(const char *from, const char *to, Attributes attr, const Promise *pp)
{                               // TODO: Implement ?
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Hard links are not yet supported on Windows");
    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "link", pp, attr, " !! Couldn't (hard) link %s to %s\n", to, from);
    return false;
}

#else                           /* !__MINGW32__ */

int MakeHardLink(const char *from, const char *to, Attributes attr, const Promise *pp)
{
    if (DONTDO)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Need to hard link files %s -> %s\n", from, to);
        return false;
    }
    else
    {
        if (link(to, from) == -1)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "link", pp, attr, " !! Couldn't (hard) link %s to %s\n", to, from);
            return false;
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, attr, " -> (Hard) Linked files %s -> %s\n", from, to);
            return true;
        }
    }
}

#endif /* !__MINGW32__ */

/*********************************************************************/

/* Expand a path contaning symbolic links, up to 4 levels  */
/* of symbolic links and then beam out in a hurry !        */

#ifdef __MINGW32__

int ExpandLinks(char *dest, const char *from, int level)
{
    CfOut(OUTPUT_LEVEL_ERROR, "", "!! Windows does not support symbolic links (at ExpandLinks(%s,%s))", dest, from);
    return false;
}

#else                           /* !__MINGW32__ */

int ExpandLinks(char *dest, const char *from, int level)
{
    char buff[CF_BUFSIZE];
    char node[CF_MAXLINKSIZE];
    struct stat statbuf;
    int lastnode = false;

    memset(dest, 0, CF_BUFSIZE);

    if (level >= CF_MAXLINKLEVEL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Too many levels of symbolic links to evaluate absolute path\n");
        return false;
    }

    const char *sp = from;

    while (*sp != '\0')
    {
        if (*sp == FILE_SEPARATOR)
        {
            sp++;
            continue;
        }

        sscanf(sp, "%[^/]", node);
        sp += strlen(node);

        if (*sp == '\0')
        {
            lastnode = true;
        }

        if (strcmp(node, ".") == 0)
        {
            continue;
        }

        if (strcmp(node, "..") == 0)
        {
            continue;
        }
        else
        {
            strcat(dest, "/");
        }

        strcat(dest, node);

        if (lstat(dest, &statbuf) == -1)        /* File doesn't exist so we can stop here */
        {
            CfOut(OUTPUT_LEVEL_ERROR, "lstat", " !! Can't stat %s in ExpandLinks\n", dest);
            return false;
        }

        if (S_ISLNK(statbuf.st_mode))
        {
            memset(buff, 0, CF_BUFSIZE);

            if (readlink(dest, buff, CF_BUFSIZE - 1) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "readlink", " !! Expand links can't stat %s\n", dest);
                return false;
            }
            else
            {
                if (buff[0] == '.')
                {
                    ChopLastNode(dest);

                    AddSlash(dest);

                    if (!JoinPath(dest, buff))
                    {
                        return false;
                    }
                }
                else if (IsAbsoluteFileName(buff))
                {
                    strcpy(dest, buff);
                    DeleteSlash(dest);

                    if (strcmp(dest, from) == 0)
                    {
                        CfDebug("No links to be expanded\n");
                        return true;
                    }

                    if ((!lastnode) && (!ExpandLinks(buff, dest, level + 1)))
                    {
                        return false;
                    }
                }
                else
                {
                    ChopLastNode(dest);
                    AddSlash(dest);
                    strcat(dest, buff);
                    DeleteSlash(dest);

                    if (strcmp(dest, from) == 0)
                    {
                        CfDebug("No links to be expanded\n");
                        return true;
                    }

                    memset(buff, 0, CF_BUFSIZE);

                    if ((!lastnode) && (!ExpandLinks(buff, dest, level + 1)))
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}
#endif /* !__MINGW32__ */

/*********************************************************************/

static char *AbsLinkPath(const char *from, const char *relto)
/* Take an abolute source and a relative destination object
   and find the absolute name of the to object */
{
    int pop = 1;
    static char destination[CF_BUFSIZE];

    if (IsAbsoluteFileName(relto))
    {
        FatalError("Cfengine internal error: call to AbsLInkPath with absolute pathname\n");
    }

    strcpy(destination, from);  /* reuse to save stack space */

    const char *sp = NULL;
    for (sp = relto; *sp != '\0'; sp++)
    {
        if (strncmp(sp, "../", 3) == 0)
        {
            pop++;
            sp += 2;
            continue;
        }

        if (strncmp(sp, "./", 2) == 0)
        {
            sp += 1;
            continue;
        }

        break;                  /* real link */
    }

    while (pop > 0)
    {
        ChopLastNode(destination);
        pop--;
    }

    if (strlen(destination) == 0)
    {
        strcpy(destination, "/");
    }
    else
    {
        AddSlash(destination);
    }

    strcat(destination, sp);
    CfDebug("Reconstructed absolute linkname = %s\n", destination);
    return destination;
}
