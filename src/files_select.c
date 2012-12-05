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

#include "cf3.defs.h"

#include "env_context.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "promises.h"
#include "matching.h"
#include "cfstream.h"
#include "string_lib.h"
#include "pipes.h"

static int SelectTypeMatch(struct stat *lstatptr, Rlist *crit);
static int SelectOwnerMatch(char *path, struct stat *lstatptr, Rlist *crit);
static int SelectModeMatch(struct stat *lstatptr, Rlist *ls);
static int SelectTimeMatch(time_t stattime, time_t fromtime, time_t totime);
static int SelectNameRegexMatch(const char *filename, char *crit);
static int SelectPathRegexMatch(char *filename, char *crit);
static int SelectExecRegexMatch(char *filename, char *crit, char *prog);
static int SelectIsSymLinkTo(char *filename, Rlist *crit);
static int SelectExecProgram(char *filename, char *command);
static int SelectSizeMatch(size_t size, size_t min, size_t max);

#if defined HAVE_CHFLAGS
static int SelectBSDMatch(struct stat *lstatptr, Rlist *bsdflags, Promise *pp);
#endif
#ifndef MINGW
static int SelectGroupMatch(struct stat *lstatptr, Rlist *crit);
#endif

int SelectLeaf(char *path, struct stat *sb, Attributes attr, Promise *pp)
{
    AlphaList leaf_attr;
    int result = true;
    Rlist *rp;

    InitAlphaList(&leaf_attr);

#ifdef MINGW
    if (attr.select.issymlinkto != NULL)
    {
        CfOut(cf_verbose, "",
              "files_select.issymlinkto is ignored on Windows (symbolic links are not supported by Windows)");
    }

    if (attr.select.groups != NULL)
    {
        CfOut(cf_verbose, "",
              "files_select.search_groups is ignored on Windows (file groups are not supported by Windows)");
    }

    if (attr.select.bsdflags != NULL)
    {
        CfOut(cf_verbose, "", "files_select.search_bsdflags is ignored on Windows");
    }
#endif /* MINGW */

    if (!attr.haveselect)
    {
        return true;
    }

    if (attr.select.name == NULL)
    {
        PrependAlphaList(&leaf_attr, "leaf_name");
    }

    for (rp = attr.select.name; rp != NULL; rp = rp->next)
    {
        if (SelectNameRegexMatch(path, rp->item))
        {
            PrependAlphaList(&leaf_attr, "leaf_name");
            break;
        }
    }

    if (attr.select.path == NULL)
    {
        PrependAlphaList(&leaf_attr, "leaf_path");
    }

    for (rp = attr.select.path; rp != NULL; rp = rp->next)
    {
        if (SelectPathRegexMatch(path, rp->item))
        {
            PrependAlphaList(&leaf_attr, "path_name");
            break;
        }
    }

    if (SelectTypeMatch(sb, attr.select.filetypes))
    {
        PrependAlphaList(&leaf_attr, "file_types");
    }

    if ((attr.select.owners) && (SelectOwnerMatch(path, sb, attr.select.owners)))
    {
        PrependAlphaList(&leaf_attr, "owner");
    }

    if (attr.select.owners == NULL)
    {
        PrependAlphaList(&leaf_attr, "owner");
    }

#ifdef MINGW
    PrependAlphaList(&leaf_attr, "group");

#else /* NOT MINGW */
    if ((attr.select.groups) && (SelectGroupMatch(sb, attr.select.groups)))
    {
        PrependAlphaList(&leaf_attr, "group");
    }

    if (attr.select.groups == NULL)
    {
        PrependAlphaList(&leaf_attr, "group");
    }
#endif /* NOT MINGW */

    if (SelectModeMatch(sb, attr.select.perms))
    {
        PrependAlphaList(&leaf_attr, "mode");
    }

#if defined HAVE_CHFLAGS
    if (SelectBSDMatch(sb, attr.select.bsdflags, pp))
    {
        PrependAlphaList(&leaf_attr, "bsdflags");
    }
#endif

    if (SelectTimeMatch(sb->st_atime, attr.select.min_atime, attr.select.max_atime))
    {
        PrependAlphaList(&leaf_attr, "atime");
    }

    if (SelectTimeMatch(sb->st_ctime, attr.select.min_ctime, attr.select.max_ctime))
    {
        PrependAlphaList(&leaf_attr, "ctime");
    }

    if (SelectSizeMatch(sb->st_size, attr.select.min_size, attr.select.max_size))
    {
        PrependAlphaList(&leaf_attr, "size");
    }

    if (SelectTimeMatch(sb->st_mtime, attr.select.min_mtime, attr.select.max_mtime))
    {
        PrependAlphaList(&leaf_attr, "mtime");
    }

    if ((attr.select.issymlinkto) && (SelectIsSymLinkTo(path, attr.select.issymlinkto)))
    {
        PrependAlphaList(&leaf_attr, "issymlinkto");
    }

    if ((attr.select.exec_regex) && (SelectExecRegexMatch(path, attr.select.exec_regex, attr.select.exec_program)))
    {
        PrependAlphaList(&leaf_attr, "exec_regex");
    }

    if ((attr.select.exec_program) && (SelectExecProgram(path, attr.select.exec_program)))
    {
        PrependAlphaList(&leaf_attr, "exec_program");
    }

    result = EvalFileResult(attr.select.result, &leaf_attr);

    CfDebug("Select result \"%s\"on %s was %d\n", attr.select.result, path, result);

    DeleteAlphaList(&leaf_attr);

    return result;
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static int SelectSizeMatch(size_t size, size_t min, size_t max)
{
    if ((size <= max) && (size >= min))
    {
        return true;
    }

    return false;
}

/*******************************************************************/

static int SelectTypeMatch(struct stat *lstatptr, Rlist *crit)
{
    AlphaList leafattrib;
    Rlist *rp;

    InitAlphaList(&leafattrib);

    if (S_ISREG(lstatptr->st_mode))
    {
        PrependAlphaList(&leafattrib, "reg");
        PrependAlphaList(&leafattrib, "plain");
    }

    if (S_ISDIR(lstatptr->st_mode))
    {
        PrependAlphaList(&leafattrib, "dir");
    }

#ifndef MINGW
    if (S_ISLNK(lstatptr->st_mode))
    {
        PrependAlphaList(&leafattrib, "symlink");
    }

    if (S_ISFIFO(lstatptr->st_mode))
    {
        PrependAlphaList(&leafattrib, "fifo");
    }

    if (S_ISSOCK(lstatptr->st_mode))
    {
        PrependAlphaList(&leafattrib, "socket");
    }

    if (S_ISCHR(lstatptr->st_mode))
    {
        PrependAlphaList(&leafattrib, "char");
    }

    if (S_ISBLK(lstatptr->st_mode))
    {
        PrependAlphaList(&leafattrib, "block");
    }
#endif /* NOT MINGW */

#ifdef HAVE_DOOR_CREATE
    if (S_ISDOOR(lstatptr->st_mode))
    {
        PrependAlphaList(&leafattrib, "door");
    }
#endif

    for (rp = crit; rp != NULL; rp = rp->next)
    {
        if (EvalFileResult((char *) rp->item, &leafattrib))
        {
            DeleteAlphaList(&leafattrib);
            return true;
        }
    }

    DeleteAlphaList(&leafattrib);
    return false;
}

static int SelectOwnerMatch(char *path, struct stat *lstatptr, Rlist *crit)
{
    AlphaList leafattrib;
    Rlist *rp;
    char ownerName[CF_BUFSIZE];
    int gotOwner;

    InitAlphaList(&leafattrib);

#ifndef MINGW                   // no uids on Windows
    char buffer[CF_SMALLBUF];
    snprintf(buffer, CF_SMALLBUF, "%jd", (uintmax_t) lstatptr->st_uid);
    PrependAlphaList(&leafattrib, buffer);
#endif /* MINGW */

    gotOwner = GetOwnerName(path, lstatptr, ownerName, sizeof(ownerName));

    if (gotOwner)
    {
        PrependAlphaList(&leafattrib, ownerName);
    }
    else
    {
        PrependAlphaList(&leafattrib, "none");
    }

    for (rp = crit; rp != NULL; rp = rp->next)
    {
        if (EvalFileResult((char *) rp->item, &leafattrib))
        {
            CfDebug(" - ? Select owner match\n");
            DeleteAlphaList(&leafattrib);
            return true;
        }

        if (gotOwner && (FullTextMatch((char *) rp->item, ownerName)))
        {
            CfDebug(" - ? Select owner match\n");
            DeleteAlphaList(&leafattrib);
            return true;
        }

#ifndef MINGW
        if (FullTextMatch((char *) rp->item, buffer))
        {
            CfDebug(" - ? Select owner match\n");
            DeleteAlphaList(&leafattrib);
            return true;
        }
#endif /* NOT MINGW */
    }

    DeleteAlphaList(&leafattrib);
    return false;
}

/*******************************************************************/

static int SelectModeMatch(struct stat *lstatptr, Rlist *list)
{
    mode_t newperm, plus, minus;
    Rlist *rp;

    for (rp = list; rp != NULL; rp = rp->next)
    {
        plus = 0;
        minus = 0;

        if (!ParseModeString(rp->item, &plus, &minus))
        {
            CfOut(cf_error, "", " !! Problem validating a mode string \"%s\" in search filter", ScalarValue(rp));
            continue;
        }

        newperm = (lstatptr->st_mode & 07777);
        newperm |= plus;
        newperm &= ~minus;

        if ((newperm & 07777) == (lstatptr->st_mode & 07777))
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

#if defined HAVE_CHFLAGS
static int SelectBSDMatch(struct stat *lstatptr, Rlist *bsdflags, Promise *pp)
{
# if defined HAVE_CHFLAGS
    u_long newflags, plus, minus;
    Rlist *rp;

    if (!ParseFlagString(bsdflags, &plus, &minus))
    {
        CfOut(cf_error, "", " !! Problem validating a BSD flag string");
        PromiseRef(cf_error, pp);
    }

    newflags = (lstatptr->st_flags & CHFLAGS_MASK);
    newflags |= plus;
    newflags &= ~minus;

    if ((newflags & CHFLAGS_MASK) == (lstatptr->st_flags & CHFLAGS_MASK))       /* file okay */
    {
        return true;
    }
# endif

    return false;
}
#endif
/*******************************************************************/

static int SelectTimeMatch(time_t stattime, time_t fromtime, time_t totime)
{
    return ((fromtime < stattime) && (stattime < totime));
}

/*******************************************************************/

static int SelectNameRegexMatch(const char *filename, char *crit)
{
    if (FullTextMatch(crit, ReadLastNode(filename)))
    {
        return true;
    }

    return false;
}

/*******************************************************************/

static int SelectPathRegexMatch(char *filename, char *crit)
{
    if (FullTextMatch(crit, filename))
    {
        return true;
    }

    return false;
}

/*******************************************************************/

static int SelectExecRegexMatch(char *filename, char *crit, char *prog)
{
    char line[CF_BUFSIZE];
    FILE *pp;
    char buf[CF_MAXVARSIZE];

// insert real value of $(this.promiser) in command

    ReplaceStr(prog, buf, sizeof(buf), "$(this.promiser)", filename);
    ReplaceStr(prog, buf, sizeof(buf), "${this.promiser}", filename);

    if ((pp = cf_popen(buf, "r")) == NULL)
    {
        CfOut(cf_error, "cf_popen", "Couldn't open pipe to command %s\n", buf);
        return false;
    }

    while (!feof(pp))
    {
        line[0] = '\0';
        CfReadLine(line, CF_BUFSIZE, pp);       /* One buffer only */

        if (FullTextMatch(crit, line))
        {
            cf_pclose(pp);
            return true;
        }
    }

    cf_pclose(pp);
    return false;
}

/*******************************************************************/

static int SelectIsSymLinkTo(char *filename, Rlist *crit)
{
#ifndef MINGW
    char buffer[CF_BUFSIZE];
    Rlist *rp;

    for (rp = crit; rp != NULL; rp = rp->next)
    {
        memset(buffer, 0, CF_BUFSIZE);

        if (readlink(filename, buffer, CF_BUFSIZE - 1) == -1)
        {
            CfOut(cf_error, "readlink", "Unable to read link %s in filter", filename);
            return false;
        }

        if (FullTextMatch(rp->item, buffer))
        {
            return true;
        }
    }
#endif /* NOT MINGW */
    return false;
}

/*******************************************************************/

static int SelectExecProgram(char *filename, char *command)
  /* command can include $(this.promiser) for the name of the file */
{
    char buf[CF_MAXVARSIZE];

// insert real value of $(this.promiser) in command

    ReplaceStr(command, buf, sizeof(buf), "$(this.promiser)", filename);
    ReplaceStr(command, buf, sizeof(buf), "${this.promiser}", filename);

    if (ShellCommandReturnsZero(buf, false))
    {
        CfDebug(" - ? Select ExecProgram match for %s\n", buf);
        return true;
    }
    else
    {
        return false;
    }
}

#ifndef MINGW

/*******************************************************************/
/* Unix implementations                                            */
/*******************************************************************/

int GetOwnerName(char *path, struct stat *lstatptr, char *owner, int ownerSz)
{
    struct passwd *pw;

    memset(owner, 0, ownerSz);
    pw = getpwuid(lstatptr->st_uid);

    if (pw == NULL)
    {
        CfOut(cf_error, "getpwuid", "!! Could not get owner name of user with uid=%ju",
              (uintmax_t)lstatptr->st_uid);
        return false;
    }

    strncpy(owner, pw->pw_name, ownerSz - 1);

    return true;
}

/*******************************************************************/

static int SelectGroupMatch(struct stat *lstatptr, Rlist *crit)
{
    AlphaList leafattrib;
    char buffer[CF_SMALLBUF];
    struct group *gr;
    Rlist *rp;

    InitAlphaList(&leafattrib);

    snprintf(buffer, CF_SMALLBUF, "%jd", (uintmax_t) lstatptr->st_gid);
    PrependAlphaList(&leafattrib, buffer);

    if ((gr = getgrgid(lstatptr->st_gid)) != NULL)
    {
        PrependAlphaList(&leafattrib, gr->gr_name);
    }
    else
    {
        PrependAlphaList(&leafattrib, "none");
    }

    for (rp = crit; rp != NULL; rp = rp->next)
    {
        if (EvalFileResult((char *) rp->item, &leafattrib))
        {
            CfDebug(" - ? Select group match\n");
            DeleteAlphaList(&leafattrib);
            return true;
        }

        if (gr && (FullTextMatch((char *) rp->item, gr->gr_name)))
        {
            CfDebug(" - ? Select owner match\n");
            DeleteAlphaList(&leafattrib);
            return true;
        }

        if (FullTextMatch((char *) rp->item, buffer))
        {
            CfDebug(" - ? Select owner match\n");
            DeleteAlphaList(&leafattrib);
            return true;
        }
    }

    DeleteAlphaList(&leafattrib);
    return false;
}

#endif /* NOT MINGW */
