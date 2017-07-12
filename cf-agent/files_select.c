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

#include <files_select.h>

#include <eval_context.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <promises.h>
#include <match_scope.h>
#include <string_lib.h>
#include <files_lib.h>
#include <pipes.h>
#include <promises.h>
#include <exec_tools.h>
#include <chflags.h>

static int SelectTypeMatch(struct stat *lstatptr, Rlist *crit);
static int SelectOwnerMatch(EvalContext *ctx, char *path, struct stat *lstatptr, Rlist *crit);
static int SelectModeMatch(struct stat *lstatptr, Rlist *ls);
static int SelectTimeMatch(time_t stattime, time_t fromtime, time_t totime);
static int SelectNameRegexMatch(EvalContext *ctx, const char *filename, char *crit);
static int SelectPathRegexMatch(EvalContext *ctx, char *filename, char *crit);
static bool SelectExecRegexMatch(EvalContext *ctx, char *filename, char *crit, char *prog);
static int SelectIsSymLinkTo(EvalContext *ctx, char *filename, Rlist *crit);
static int SelectExecProgram(char *filename, char *command);
static int SelectSizeMatch(size_t size, size_t min, size_t max);

#if !defined(__MINGW32__)
static int SelectGroupMatch(EvalContext *ctx, struct stat *lstatptr, Rlist *crit);
#endif

#if defined HAVE_CHFLAGS
static int SelectBSDMatch(struct stat *lstatptr, Rlist *bsdflags);
#endif

int SelectLeaf(EvalContext *ctx, char *path, struct stat *sb, FileSelect fs)
{
    Rlist *rp;

    StringSet *leaf_attr = StringSetNew();

#ifdef __MINGW32__
    if (fs.issymlinkto != NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
              "files_select.issymlinkto is ignored on Windows (symbolic links are not supported by Windows)");
    }

    if (fs.groups != NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
              "files_select.search_groups is ignored on Windows (file groups are not supported by Windows)");
    }

    if (fs.bsdflags != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "files_select.search_bsdflags is ignored on Windows");
    }
#endif /* __MINGW32__ */

    if (fs.name == NULL)
    {
        StringSetAdd(leaf_attr, xstrdup("leaf_name"));
    }

    for (rp = fs.name; rp != NULL; rp = rp->next)
    {
        if (SelectNameRegexMatch(ctx, path, RlistScalarValue(rp)))
        {
            StringSetAdd(leaf_attr, xstrdup("leaf_name"));
            break;
        }
    }

    if (fs.path == NULL)
    {
        StringSetAdd(leaf_attr, xstrdup("leaf_path"));
    }

    for (rp = fs.path; rp != NULL; rp = rp->next)
    {
        if (SelectPathRegexMatch(ctx, path, RlistScalarValue(rp)))
        {
            StringSetAdd(leaf_attr, xstrdup("path_name"));
            break;
        }
    }

    if (SelectTypeMatch(sb, fs.filetypes))
    {
        StringSetAdd(leaf_attr, xstrdup("file_types"));
    }

    if ((fs.owners) && (SelectOwnerMatch(ctx, path, sb, fs.owners)))
    {
        StringSetAdd(leaf_attr, xstrdup("owner"));
    }

    if (fs.owners == NULL)
    {
        StringSetAdd(leaf_attr, xstrdup("owner"));
    }

#ifdef __MINGW32__
    StringSetAdd(leaf_attr, xstrdup("group"));

#else /* !__MINGW32__ */
    if ((fs.groups) && (SelectGroupMatch(ctx, sb, fs.groups)))
    {
        StringSetAdd(leaf_attr, xstrdup("group"));
    }

    if (fs.groups == NULL)
    {
        StringSetAdd(leaf_attr, xstrdup("group"));
    }
#endif /* !__MINGW32__ */

    if (SelectModeMatch(sb, fs.perms))
    {
        StringSetAdd(leaf_attr, xstrdup("mode"));
    }

#if defined HAVE_CHFLAGS
    if (SelectBSDMatch(sb, fs.bsdflags))
    {
        StringSetAdd(leaf_attr, xstrdup("bsdflags"));
    }
#endif

    if (SelectTimeMatch(sb->st_atime, fs.min_atime, fs.max_atime))
    {
        StringSetAdd(leaf_attr, xstrdup("atime"));
    }

    if (SelectTimeMatch(sb->st_ctime, fs.min_ctime, fs.max_ctime))
    {
        StringSetAdd(leaf_attr, xstrdup("ctime"));
    }

    if (SelectSizeMatch(sb->st_size, fs.min_size, fs.max_size))
    {
        StringSetAdd(leaf_attr, xstrdup("size"));
    }

    if (SelectTimeMatch(sb->st_mtime, fs.min_mtime, fs.max_mtime))
    {
        StringSetAdd(leaf_attr, xstrdup("mtime"));
    }

    if ((fs.issymlinkto) && (SelectIsSymLinkTo(ctx, path, fs.issymlinkto)))
    {
        StringSetAdd(leaf_attr, xstrdup("issymlinkto"));
    }

    if ((fs.exec_regex) && (SelectExecRegexMatch(ctx, path, fs.exec_regex, fs.exec_program)))
    {
        StringSetAdd(leaf_attr, xstrdup("exec_regex"));
    }

    if ((fs.exec_program) && (SelectExecProgram(path, fs.exec_program)))
    {
        StringSetAdd(leaf_attr, xstrdup("exec_program"));
    }

    bool result = EvalFileResult(fs.result, leaf_attr);

    Log(LOG_LEVEL_VERBOSE, "file_select result '%s' on '%s' was '%s'",
        fs.result, path,
        result ? "true" : "false");

    StringSetDestroy(leaf_attr);

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
    Rlist *rp;

    StringSet *leafattrib = StringSetNew();

    if (S_ISREG(lstatptr->st_mode))
    {
        StringSetAdd(leafattrib, xstrdup("reg"));
        StringSetAdd(leafattrib, xstrdup("plain"));
    }

    if (S_ISDIR(lstatptr->st_mode))
    {
        StringSetAdd(leafattrib, xstrdup("dir"));
    }

#ifndef __MINGW32__
    if (S_ISLNK(lstatptr->st_mode))
    {
        StringSetAdd(leafattrib, xstrdup("symlink"));
    }

    if (S_ISFIFO(lstatptr->st_mode))
    {
        StringSetAdd(leafattrib, xstrdup("fifo"));
    }

    if (S_ISSOCK(lstatptr->st_mode))
    {
        StringSetAdd(leafattrib, xstrdup("socket"));
    }

    if (S_ISCHR(lstatptr->st_mode))
    {
        StringSetAdd(leafattrib, xstrdup("char"));
    }

    if (S_ISBLK(lstatptr->st_mode))
    {
        StringSetAdd(leafattrib, xstrdup("block"));
    }
#endif /* !__MINGW32__ */

#ifdef HAVE_DOOR_CREATE
    if (S_ISDOOR(lstatptr->st_mode))
    {
        StringSetAdd(leafattrib, xstrdup("door"));
    }
#endif

    for (rp = crit; rp != NULL; rp = rp->next)
    {
        if (EvalFileResult(RlistScalarValue(rp), leafattrib))
        {
            StringSetDestroy(leafattrib);
            return true;
        }
    }

    StringSetDestroy(leafattrib);
    return false;
}

static int SelectOwnerMatch(EvalContext *ctx, char *path, struct stat *lstatptr, Rlist *crit)
{
    Rlist *rp;
    char ownerName[CF_BUFSIZE];
    int gotOwner;

    StringSet *leafattrib = StringSetNew();

#ifndef __MINGW32__                   // no uids on Windows
    char buffer[CF_SMALLBUF];
    snprintf(buffer, CF_SMALLBUF, "%jd", (uintmax_t) lstatptr->st_uid);
    StringSetAdd(leafattrib, xstrdup(buffer));
#endif /* __MINGW32__ */

    gotOwner = GetOwnerName(path, lstatptr, ownerName, sizeof(ownerName));

    if (gotOwner)
    {
        StringSetAdd(leafattrib, xstrdup(ownerName));
    }
    else
    {
        StringSetAdd(leafattrib, xstrdup("none"));
    }

    for (rp = crit; rp != NULL; rp = rp->next)
    {
        if (EvalFileResult(RlistScalarValue(rp), leafattrib))
        {
            Log(LOG_LEVEL_DEBUG, "Select owner match");
            StringSetDestroy(leafattrib);
            return true;
        }

        if (gotOwner && (FullTextMatch(ctx, RlistScalarValue(rp), ownerName)))
        {
            Log(LOG_LEVEL_DEBUG, "Select owner match");
            StringSetDestroy(leafattrib);
            return true;
        }

#ifndef __MINGW32__
        if (FullTextMatch(ctx, RlistScalarValue(rp), buffer))
        {
            Log(LOG_LEVEL_DEBUG, "Select owner match");
            StringSetDestroy(leafattrib);
            return true;
        }
#endif /* !__MINGW32__ */
    }

    StringSetDestroy(leafattrib);
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

        if (!ParseModeString(RlistScalarValue(rp), &plus, &minus))
        {
            Log(LOG_LEVEL_ERR, "Problem validating a mode string '%s' in search filter", RlistScalarValue(rp));
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
static int SelectBSDMatch(struct stat *lstatptr, Rlist *bsdflags)
{
    u_long newflags, plus, minus;

    if (!ParseFlagString(bsdflags, &plus, &minus))
    {
        Log(LOG_LEVEL_ERR, "Problem validating a BSD flag string");
    }

    newflags = (lstatptr->st_flags & CHFLAGS_MASK);
    newflags |= plus;
    newflags &= ~minus;

    if ((newflags & CHFLAGS_MASK) == (lstatptr->st_flags & CHFLAGS_MASK))       /* file okay */
    {
        return true;
    }

    return false;
}
#endif
/*******************************************************************/

static int SelectTimeMatch(time_t stattime, time_t fromtime, time_t totime)
{
    return ((fromtime < stattime) && (stattime < totime));
}

/*******************************************************************/

static int SelectNameRegexMatch(EvalContext *ctx, const char *filename, char *crit)
{
    if (FullTextMatch(ctx, crit, ReadLastNode(filename)))
    {
        return true;
    }

    return false;
}

/*******************************************************************/

static int SelectPathRegexMatch(EvalContext *ctx, char *filename, char *crit)
{
    if (FullTextMatch(ctx, crit, filename))
    {
        return true;
    }

    return false;
}

/*******************************************************************/

static bool SelectExecRegexMatch(EvalContext *ctx, char *filename, char *crit, char *prog)
{
    // insert real value of $(this.promiser) in command

    char *buf_tmp = SearchAndReplace(prog, "$(this.promiser)", filename);
    char *buf = SearchAndReplace(buf_tmp, "${this.promiser}", filename);
    free(buf_tmp);

    FILE *pp = cf_popen(buf, "r", true);
    if (pp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open pipe to command '%s'. (cf_popen: %s)", buf, GetErrorStr());
        free(buf);
        return false;
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    for (;;)
    {
        ssize_t res = CfReadLine(&line, &line_size, pp);
        if (res == -1)
        {
            if (!feof(pp))
            {
                Log(LOG_LEVEL_ERR, "Error reading output from command '%s'. (fgets: %s)", buf, GetErrorStr());
            }
            cf_pclose(pp);
            free(line);
            free(buf);
            return false;
        }

        if (FullTextMatch(ctx, crit, line))
        {
            cf_pclose(pp);
            free(line);
            free(buf);
            return true;
        }
    }

    cf_pclose(pp);
    free(line);
    free(buf);
    return false;
}

/*******************************************************************/

static int SelectIsSymLinkTo(EvalContext *ctx, char *filename, Rlist *crit)
{
#ifndef __MINGW32__
    char buffer[CF_BUFSIZE];
    Rlist *rp;

    for (rp = crit; rp != NULL; rp = rp->next)
    {
        memset(buffer, 0, CF_BUFSIZE);

        struct stat statbuf;

        // Don't worry if this gives an error, that's handled above us.

        // We're calling lstat() here to avoid dereferencing the
        // symlink... and we only care if the inode is a directory.
        if (lstat(filename, &statbuf) == -1)
        {
            // Do nothing.
        }
        else if (!S_ISLNK(statbuf.st_mode))
        {
            Log(LOG_LEVEL_DEBUG, "Skipping readlink() on non-symlink %s", filename);
            return false;
        }

        if (readlink(filename, buffer, CF_BUFSIZE - 1) == -1)
        {
            Log(LOG_LEVEL_ERR, "Unable to read link '%s' in filter. (readlink: %s)",
                filename, GetErrorStr());
            return false;
        }

        if (FullTextMatch(ctx, RlistScalarValue(rp), buffer))
        {
            return true;
        }
    }
#endif /* !__MINGW32__ */
    return false;
}

/*******************************************************************/

static int SelectExecProgram(char *filename, char *command)
  /* command can include $(this.promiser) for the name of the file */
{
// insert real value of $(this.promiser) in command

    char *buf_tmp = SearchAndReplace(command, "$(this.promiser)", filename);
    char *buf = SearchAndReplace(buf_tmp, "${this.promiser}", filename);
    free(buf_tmp);

    bool returns_zero = ShellCommandReturnsZero(buf, SHELL_TYPE_NONE);

    if (returns_zero)
    {
        Log(LOG_LEVEL_DEBUG, "Select ExecProgram match for '%s'", buf);
        free(buf);
        return true;
    }
    else
    {
        free(buf);
        return false;
    }
}

#ifndef __MINGW32__

/*******************************************************************/
/* Unix implementations                                            */
/*******************************************************************/

int GetOwnerName(ARG_UNUSED char *path, struct stat *lstatptr, char *owner, int ownerSz)
{
    struct passwd *pw;

    memset(owner, 0, ownerSz);
    pw = getpwuid(lstatptr->st_uid);

    if (pw == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not get owner name of user with 'uid=%ju'. (getpwuid: %s)",
              (uintmax_t)lstatptr->st_uid, GetErrorStr());
        return false;
    }

    strncpy(owner, pw->pw_name, ownerSz - 1);

    return true;
}

/*******************************************************************/

static int SelectGroupMatch(EvalContext *ctx, struct stat *lstatptr, Rlist *crit)
{
    char buffer[CF_SMALLBUF];
    struct group *gr;
    Rlist *rp;

    StringSet *leafattrib = StringSetNew();

    snprintf(buffer, CF_SMALLBUF, "%jd", (uintmax_t) lstatptr->st_gid);
    StringSetAdd(leafattrib, xstrdup(buffer));

    if ((gr = getgrgid(lstatptr->st_gid)) != NULL)
    {
        StringSetAdd(leafattrib, xstrdup(gr->gr_name));
    }
    else
    {
        StringSetAdd(leafattrib, xstrdup("none"));
    }

    for (rp = crit; rp != NULL; rp = rp->next)
    {
        if (EvalFileResult(RlistScalarValue(rp), leafattrib))
        {
            Log(LOG_LEVEL_DEBUG, "Select group match");
            StringSetDestroy(leafattrib);
            return true;
        }

        if (gr && (FullTextMatch(ctx, RlistScalarValue(rp), gr->gr_name)))
        {
            Log(LOG_LEVEL_DEBUG, "Select group match");
            StringSetDestroy(leafattrib);
            return true;
        }

        if (FullTextMatch(ctx, RlistScalarValue(rp), buffer))
        {
            Log(LOG_LEVEL_DEBUG, "Select group match");
            StringSetDestroy(leafattrib);
            return true;
        }
    }

    StringSetDestroy(leafattrib);
    return false;
}

#endif /* !__MINGW32__ */
