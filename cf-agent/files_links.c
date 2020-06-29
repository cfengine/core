/*
  Copyright 2020 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <files_links.h>

#include <actuator.h>
#include <promises.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_operators.h>
#include <files_lib.h>
#include <locks.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <eval_context.h>

#if !defined(__MINGW32__)
static bool MakeLink(EvalContext *ctx, const char *from, const char *to, const Attributes *attr, const Promise *pp, PromiseResult *result);
#endif
static char *AbsLinkPath(const char *from, const char *relto);

/*****************************************************************************/

#ifdef __MINGW32__

PromiseResult VerifyLink(EvalContext *ctx, char *destination, const char *source, const Attributes *attr, const Promise *pp)
{
    RecordFailure(ctx, pp, attr, "Windows does not support symbolic links (at VerifyLink())");
    return PROMISE_RESULT_FAIL;
}

#else

PromiseResult VerifyLink(EvalContext *ctx, char *destination, const char *source, const Attributes *attr, const Promise *pp)
{
    char to[CF_BUFSIZE], linkbuf[CF_BUFSIZE], absto[CF_BUFSIZE];
    struct stat sb;

    memset(to, 0, CF_BUFSIZE);

    if ((!IsAbsoluteFileName(source)) && (*source != '.'))        /* links without a directory reference */
    {
        snprintf(to, CF_BUFSIZE - 1, "./%s", source);
    }
    else
    {
        strlcpy(to, source, CF_BUFSIZE);
    }

    if (!IsAbsoluteFileName(to))        /* relative path, must still check if exists */
    {
        Log(LOG_LEVEL_DEBUG, "Relative link destination detected '%s'", to);
        strcpy(absto, AbsLinkPath(destination, to));
        Log(LOG_LEVEL_DEBUG, "Absolute path to relative link '%s', '%s'", absto, destination);
    }
    else
    {
        strcpy(absto, to);
    }

    bool source_file_exists = true;

    if (stat(absto, &sb) == -1)
    {
        Log(LOG_LEVEL_DEBUG, "No source file '%s'", absto);
        source_file_exists = false;
    }

    if ((!source_file_exists) && (attr->link.when_no_file != cfa_force) && (attr->link.when_no_file != cfa_delete))
    {
        Log(LOG_LEVEL_VERBOSE, "Source '%s' for linking is absent", absto);
        RecordFailure(ctx, pp, attr, "Unable to create link '%s' -> '%s', no source", destination, to);
        return PROMISE_RESULT_FAIL;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if ((!source_file_exists) && (attr->link.when_no_file == cfa_delete))
    {
        KillGhostLink(ctx, destination, attr, pp, &result);
        return result;
    }

    memset(linkbuf, 0, CF_BUFSIZE);

    if (readlink(destination, linkbuf, CF_BUFSIZE - 1) == -1)
    {
        if (!MakingChanges(ctx, pp, attr, &result, "create link '%s'", destination))
        {
            return result;
        }

        bool dir_created = false;
        if (!MakeParentDirectory2(destination, attr->move_obstructions, true, &dir_created))
        {
            RecordFailure(ctx, pp, attr, "Unable to create parent directory of link '%s' -> '%s'",
                          destination, to);
            return PROMISE_RESULT_FAIL;
        }
        else
        {
            if (dir_created)
            {
                RecordChange(ctx, pp, attr, "Created parent directory for link '%s'", destination);
                result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            }
            if (!MoveObstruction(ctx, destination, attr, pp, &result))
            {
                RecordFailure(ctx, pp, attr,
                              "Unable to create link '%s' -> '%s', failed to move obstruction",
                              destination, to);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                return result;
            }

            if (!MakeLink(ctx, destination, source, attr, pp, &result))
            {
                RecordFailure(ctx, pp, attr, "Unable to create link '%s' -> '%s'", destination, to);
                result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            }

            return result;
        }
    }
    else
    {
        int ok = false;

        if ((attr->link.link_type == FILE_LINK_TYPE_SYMLINK) && (strcmp(linkbuf, to) != 0) && (strcmp(linkbuf, source) != 0))
        {
            ok = true;
        }
        else if (strcmp(linkbuf, source) != 0)
        {
            ok = true;
        }

        if (ok)
        {
            if (attr->move_obstructions)
            {
                if (MakingChanges(ctx, pp, attr, &result, "remove incorrect link '%s'", destination))
                {
                    if (unlink(destination) == -1)
                    {
                        RecordFailure(ctx, pp, attr, "Error removing link '%s' (points to '%s' not '%s')",
                                      destination, linkbuf, to);
                        return PROMISE_RESULT_FAIL;
                    }
                    RecordChange(ctx, pp, attr, "Overrode incorrect link '%s'", destination);
                    result = PROMISE_RESULT_CHANGE;

                    MakeLink(ctx, destination, source, attr, pp, &result);
                    return result;
                }
                else
                {
                    return result;
                }
            }
            else
            {
                RecordFailure(ctx, pp, attr,
                              "Link '%s' points to '%s' not '%s', but not moving obstructions",
                              destination, linkbuf, to);
                return PROMISE_RESULT_FAIL;
            }
        }
        else
        {
            RecordNoChange(ctx, pp, attr, "Link '%s' points to '%s', promise kept", destination, source);
            return PROMISE_RESULT_NOOP;
        }
    }
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

PromiseResult VerifyAbsoluteLink(EvalContext *ctx, char *destination, const char *source, const Attributes *attr, const Promise *pp)
{
    char absto[CF_BUFSIZE];
    char expand[CF_BUFSIZE];
    char linkto[CF_BUFSIZE];

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

    CompressPath(absto, sizeof(absto), linkto);

    expand[0] = '\0';

    if (attr->link.when_no_file == cfa_force)
    {
        if (!ExpandLinks(expand, absto, 0, CF_MAXLINKLEVEL))     /* begin at level 1 and beam out at 15 */
        {
            RecordFailure(ctx, pp, attr, "Failed to expand absolute link to '%s'", source);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return PROMISE_RESULT_FAIL;
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "ExpandLinks returned '%s'", expand);
        }
    }
    else
    {
        strcpy(expand, absto);
    }

    CompressPath(linkto, sizeof(linkto), expand);

    return VerifyLink(ctx, destination, linkto, attr, pp);
}

/*****************************************************************************/

PromiseResult VerifyRelativeLink(EvalContext *ctx, char *destination, const char *source, const Attributes *attr, const Promise *pp)
{
    char *sp, *commonto, *commonfrom;
    char buff[CF_BUFSIZE], linkto[CF_BUFSIZE];
    int levels = 0;

    if (*source == '.')
    {
        return VerifyLink(ctx, destination, source, attr, pp);
    }

    if (!CompressPath(linkto, sizeof(linkto), source))
    {
        RecordInterruption(ctx, pp, attr, "Failed to link '%s' to '%s'", destination, source);
        return PROMISE_RESULT_INTERRUPTED;
    }

    commonto = linkto;
    commonfrom = destination;

    if (strcmp(commonto, commonfrom) == 0)
    {
        RecordInterruption(ctx, pp, attr, "Failed to link '%s' to '%s', can't link file '%s' to itself",
                           destination, source, commonto);
        return PROMISE_RESULT_INTERRUPTED;
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
        const char add[] = ".." FILE_SEPARATOR_STR;

        if (!PathAppend(buff, sizeof(buff), add, FILE_SEPARATOR))
        {
            RecordFailure(ctx, pp, attr,
                          "Internal limit reached in VerifyRelativeLink(),"
                          " path too long: '%s' + '%s'",
                          buff, add);
            return PROMISE_RESULT_FAIL;
        }
    }

    if (!PathAppend(buff, sizeof(buff), commonto, FILE_SEPARATOR))
    {
        RecordFailure(ctx, pp, attr,
                      "Internal limit reached in VerifyRelativeLink() end,"
                      " path too long: '%s' + '%s'",
                      buff, commonto);
        return PROMISE_RESULT_FAIL;
    }

    return VerifyLink(ctx, destination, buff, attr, pp);
}

/*****************************************************************************/

PromiseResult VerifyHardLink(EvalContext *ctx, char *destination, const char *source, const Attributes *attr, const Promise *pp)
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
        strlcpy(to, source, CF_BUFSIZE);
    }

    if (!IsAbsoluteFileName(to))        /* relative path, must still check if exists */
    {
        Log(LOG_LEVEL_DEBUG, "Relative link destination detected '%s'", to);
        strcpy(absto, AbsLinkPath(destination, to));
        Log(LOG_LEVEL_DEBUG, "Absolute path to relative link '%s', destination '%s'", absto, destination);
    }
    else
    {
        strcpy(absto, to);
    }

    if (stat(absto, &ssb) == -1)
    {
        RecordInterruption(ctx, pp, attr, "Source file '%s' doesn't exist", source);
        return PROMISE_RESULT_INTERRUPTED;
    }

    if (!S_ISREG(ssb.st_mode))
    {
        RecordFailure(ctx, pp, attr,
                      "Source file '%s' is not a regular file, not appropriate to hard-link", to);
        return PROMISE_RESULT_FAIL;
    }

    Log(LOG_LEVEL_DEBUG, "Trying to hard link '%s' -> '%s'", destination, to);

    if (stat(destination, &dsb) == -1)
    {
        PromiseResult result = PROMISE_RESULT_NOOP;
        MakeHardLink(ctx, destination, to, attr, pp, &result);
        return result;
    }

    /* both files exist, but are they the same file? POSIX says  */
    /* the files could be on different devices, but unix doesn't */
    /* allow this behaviour so the tests below are theoretical... */

    if ((dsb.st_ino != ssb.st_ino) && (dsb.st_dev != ssb.st_dev))
    {
        Log(LOG_LEVEL_VERBOSE,
            "If this is POSIX, unable to determine if %s is hard link is correct"
            " since it points to a different filesystem",
            destination);

        if ((dsb.st_mode == ssb.st_mode) && (dsb.st_size == ssb.st_size))
        {
            RecordNoChange(ctx, pp, attr, "Hard link '%s' -> '%s' on different device appears okay",
                           destination, to);
            return PROMISE_RESULT_NOOP;
        }
    }

    if ((dsb.st_ino == ssb.st_ino) && (dsb.st_dev == ssb.st_dev))
    {
        RecordNoChange(ctx, pp, attr, "Hard link '%s' -> '%s' exists and is okay",
                     destination, to);
        return PROMISE_RESULT_NOOP;
    }

    Log(LOG_LEVEL_INFO, "'%s' does not appear to be a hard link to '%s'", destination, to);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (!MakingChanges(ctx, pp, attr, &result,  "hard link '%s' -> '%s'", destination, to))
    {
        return result;
    }

    if (!MoveObstruction(ctx, destination, attr, pp, &result))
    {
        RecordFailure(ctx, pp, attr,
                      "Unable to create hard link '%s' -> '%s', failed to move obstruction",
                      destination, to);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        return result;
    }

    MakeHardLink(ctx, destination, to, attr, pp, &result);
    return result;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

#ifdef __MINGW32__

bool KillGhostLink(EvalContext *ctx, const char *name, const Attributes *attr, const Promise *pp, PromiseResult *result)
{
    RecordFailure(ctx, pp, attr, "Cannot remove dead link '%s' (Windows does not support symbolic links)", name);
    PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
    return false;
}

#else                           /* !__MINGW32__ */

bool KillGhostLink(EvalContext *ctx, const char *name, const Attributes *attr, const Promise *pp,
                   PromiseResult *result)
{
    char linkbuf[CF_BUFSIZE], tmp[CF_BUFSIZE];
    char linkpath[CF_BUFSIZE], *sp;
    struct stat statbuf;

    memset(linkbuf, 0, CF_BUFSIZE);
    memset(linkpath, 0, CF_BUFSIZE);

    if (readlink(name, linkbuf, CF_BUFSIZE - 1) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Can't read link '%s' while checking for deadlinks", name);
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
    CompressPath(tmp, sizeof(tmp), linkpath);

    if (stat(tmp, &statbuf) == -1)    /* link points nowhere */
    {
        if ((attr->link.when_no_file == cfa_delete) || (attr->recursion.rmdeadlinks))
        {
            Log(LOG_LEVEL_VERBOSE,
                "'%s' is a link which points to '%s', but the target doesn't seem to exist",
                name, linkbuf);

            if (MakingChanges(ctx, pp, attr, result, "remove dead link '%s'", name))
            {
                unlink(name);   /* May not work on a client-mounted system ! */
                RecordChange(ctx, pp, attr, "Removed dead link '%s'", name);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                return true;
            }
            else
            {
                return true;
            }
        }
    }

    return false;
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

#if !defined(__MINGW32__)
static bool MakeLink(EvalContext *ctx, const char *from, const char *to, const Attributes *attr, const Promise *pp,
                     PromiseResult *result)
{
    if (MakingChanges(ctx, pp, attr, result, "link files '%s' -> '%s'", from, to))
    {
        if (symlink(to, from) == -1)
        {
            RecordFailure(ctx, pp, attr, "Couldn't link '%s' to '%s'. (symlink: %s)",
                          to, from, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return false;
        }
        else
        {
            RecordChange(ctx, pp, attr, "Linked files '%s' -> '%s'", from, to);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            return true;
        }
    }
    else
    {
        return false;
    }
}
#endif /* !__MINGW32__ */

/*****************************************************************************/

#ifdef __MINGW32__

bool MakeHardLink(EvalContext *ctx, const char *from, const char *to, const Attributes *attr, const Promise *pp,
                  PromiseResult *result)
{                               // TODO: Implement ?
    RecordFailure(ctx, pp, attr,
                  "Couldn't hard link '%s' to '%s' (Hard links are not yet supported on Windows)",
                  to, from);
    *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
    return false;
}

#else                           /* !__MINGW32__ */

bool MakeHardLink(EvalContext *ctx, const char *from, const char *to, const Attributes *attr, const Promise *pp,
                  PromiseResult *result)
{
    if (MakingChanges(ctx, pp, attr, result, "hard link files '%s' -> '%s'", from, to))
    {
        if (link(to, from) == -1)
        {
            RecordFailure(ctx, pp, attr, "Failed to hard link '%s' to '%s'. (link: %s)",
                          to, from, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return false;
        }
        else
        {
            RecordChange(ctx, pp, attr, "Hard linked file '%s' -> '%s'", from, to);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
            return true;
        }
    }
    else
    {
        return false;
    }
}

#endif /* !__MINGW32__ */

/*********************************************************************/

/* Expand a path contaning symbolic links, up to 4 levels  */
/* of symbolic links and then beam out in a hurry !        */

#ifdef __MINGW32__

bool ExpandLinks(char *dest, const char *from, int level, int max_level)
{
    Log(LOG_LEVEL_ERR, "Windows does not support symbolic links (at ExpandLinks(%s,%s))", dest, from);
    return false;
}

#else                           /* !__MINGW32__ */

bool ExpandLinks(char *dest, const char *from, int level, int max_level)
{
    char buff[CF_BUFSIZE];
    char node[CF_MAXLINKSIZE];
    struct stat statbuf;
    int lastnode = false;

    memset(dest, 0, CF_BUFSIZE);

    if (level >= CF_MAXLINKLEVEL)
    {
        Log(LOG_LEVEL_ERR, "Too many levels of symbolic links to evaluate absolute path");
        return false;
    }

    if (level >= max_level)
    {
        Log(LOG_LEVEL_DEBUG, "Reached maximum level of symbolic link resolution");
        return true;
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
            strcat(dest, "/..");
            continue;
        }
        else
        {
            strcat(dest, "/");
        }

        strcat(dest, node);

        if (lstat(dest, &statbuf) == -1)        /* File doesn't exist so we can stop here */
        {
            Log(LOG_LEVEL_ERR, "Can't stat '%s' in ExpandLinks. (lstat: %s)", dest, GetErrorStr());
            return false;
        }

        if (S_ISLNK(statbuf.st_mode))
        {
            memset(buff, 0, CF_BUFSIZE);

            if (readlink(dest, buff, CF_BUFSIZE - 1) == -1)
            {
                Log(LOG_LEVEL_ERR, "Expand links can't stat '%s'. (readlink: %s)", dest, GetErrorStr());
                return false;
            }
            else
            {
                if (buff[0] == '.')
                {
                    ChopLastNode(dest);
                    AddSlash(dest);

                    /* TODO pass and use parameter dest_size. */
                    size_t ret = strlcat(dest, buff, CF_BUFSIZE);
                    if (ret >= CF_BUFSIZE)
                    {
                        Log(LOG_LEVEL_ERR,
                            "Internal limit reached in ExpandLinks(),"
                            " path too long: '%s' + '%s'",
                            dest, buff);
                        return false;
                    }
                }
                else if (IsAbsoluteFileName(buff))
                {
                    strcpy(dest, buff);
                    DeleteSlash(dest);

                    if (strcmp(dest, from) == 0)
                    {
                        Log(LOG_LEVEL_DEBUG, "No links to be expanded");
                        return true;
                    }

                    if ((!lastnode) && (!ExpandLinks(buff, dest, level + 1, max_level)))
                    {
                        return false;
                    }
                }
                else
                {
                    ChopLastNode(dest);
                    AddSlash(dest);

                    /* TODO use param dest_size. */
                    size_t ret = strlcat(dest, buff, CF_BUFSIZE);
                    if (ret >= CF_BUFSIZE)
                    {
                        Log(LOG_LEVEL_ERR,
                            "Internal limit reached in ExpandLinks end,"
                            " path too long: '%s' + '%s'", dest, buff);
                        return false;
                    }

                    DeleteSlash(dest);

                    if (strcmp(dest, from) == 0)
                    {
                        Log(LOG_LEVEL_DEBUG, "No links to be expanded");
                        return true;
                    }

                    memset(buff, 0, CF_BUFSIZE);

                    if ((!lastnode) && (!ExpandLinks(buff, dest, level + 1, max_level)))
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
    static char destination[CF_BUFSIZE]; /* GLOBAL_R, no need to initialize */

    if (IsAbsoluteFileName(relto))
    {
        ProgrammingError("Call to AbsLInkPath with absolute pathname");
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
    Log(LOG_LEVEL_DEBUG, "Reconstructed absolute linkname '%s'", destination);
    return destination;
}
