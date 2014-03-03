/*
   Copyright (C) CFEngine AS

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <verify_users.h>

#include <string_lib.h>
#include <exec_tools.h>
#include <policy.h>
#include <misc_lib.h>
#include <rlist.h>
#include <pipes.h>
#include <files_copy.h>
#include <files_interfaces.h>
#include <eval_context.h>

#include <cf3.defs.h>
#include <verify_methods.h>

#include <stdio.h>
#include <string.h>

#include <security/pam_appl.h>

#include <sys/types.h>
#include <grp.h>
#include <pwd.h>

#ifdef HAVE_SHADOW_H
# include <shadow.h>
#endif

#define CFUSR_CHECKBIT(v,p) ((v) & (1UL << (p)))
#define CFUSR_SETBIT(v,p)   ((v)   |= ((1UL) << (p)))
#define CFUSR_CLEARBIT(v,p) ((v) &= ~((1UL) << (p)))

typedef enum
{
    i_uid,
    i_password,
    i_comment,
    i_group,
    i_groups,
    i_home,
    i_shell,
    i_locked
} which;

static const char *GetPlatformSpecificExpirationDate()
{
     // 2nd January 1970.

#if defined(_AIX)
    return "0102000070";
#elif defined(__hpux) || defined(__SVR4)
    return "02/01/70";
#elif defined(__linux__)
    return "1970-01-02";
#else
# error Your operating system lacks the proper string for the "usermod -e" utility.
#endif
}

static int PasswordSupplier(int num_msg, const struct pam_message **msg,
           struct pam_response **resp, void *appdata_ptr)
{
    // All allocations here will be freed by the pam framework.
    *resp = xmalloc(num_msg * sizeof(struct pam_response));
    for (int i = 0; i < num_msg; i++)
    {
        if ((*msg)[i].msg_style == PAM_PROMPT_ECHO_OFF)
        {
            (*resp)[i].resp = xstrdup((const char *)appdata_ptr);
        }
        else
        {
            (*resp)[i].resp = xstrdup("");
        }
        (*resp)[i].resp_retcode = 0;
    }

    return PAM_SUCCESS;
}

static bool GetPasswordHash(const char *puser, const struct passwd *passwd_info, const char **result)
{
    // Silence warning.
    (void)puser;

#ifdef HAVE_GETSPNAM
    // If the hash is very short, it's probably a stub. Try getting the shadow password instead.
    if (strlen(passwd_info->pw_passwd) <= 4)
    {
        Log(LOG_LEVEL_VERBOSE, "Getting user '%s' password hash from shadow database.", puser);

        struct spwd *spwd_info;
        errno = 0;
        spwd_info = getspnam(puser);
        if (!spwd_info)
        {
            if (errno)
            {
                Log(LOG_LEVEL_ERR, "Could not get information from user shadow database. (getspnam: '%s')", GetErrorStr());
                return false;
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Could not find user when checking password.");
                return false;
            }
        }
        else if (spwd_info)
        {
            *result = spwd_info->sp_pwdp;
            return true;
        }
    }
#endif // HAVE_GETSPNAM
    Log(LOG_LEVEL_VERBOSE, "Getting user '%s' password hash from passwd database.", puser);
    *result = passwd_info->pw_passwd;
    return true;
}

static bool IsPasswordCorrect(const char *puser, const char* password, PasswordFormat format, const struct passwd *passwd_info)
{
    /*
     * Check if password is already correct. If format is 'hash' we just do a simple
     * comparison with the supplied hash value, otherwise we try a pam login using
     * the real password.
     */

    if (format == PASSWORD_FORMAT_HASH)
    {
        const char *system_hash;
        if (!GetPasswordHash(puser, passwd_info, &system_hash))
        {
            return false;
        }
        bool result = (strcmp(password, system_hash) == 0);
        Log(LOG_LEVEL_VERBOSE, "Verifying password hash for user '%s': %s.", puser, result ? "correct" : "incorrect");
        return result;
    }
    else if (format != PASSWORD_FORMAT_PLAINTEXT)
    {
        ProgrammingError("Unknown PasswordFormat value");
    }

    int status;
    pam_handle_t *handle;
    struct pam_conv conv;
    conv.conv = PasswordSupplier;
    conv.appdata_ptr = (void*)password;

    status = pam_start("login", puser, &conv, &handle);
    if (status != PAM_SUCCESS)
    {
        Log(LOG_LEVEL_ERR, "Could not initialize pam session. (pam_start: '%s')", pam_strerror(NULL, status));
        return false;
    }
    status = pam_authenticate(handle, PAM_SILENT);
    pam_end(handle, status);
    if (status == PAM_SUCCESS)
    {
        Log(LOG_LEVEL_VERBOSE, "Verifying plaintext password for user '%s': correct.", puser);
        return true;
    }
    else if (status != PAM_AUTH_ERR)
    {
        Log(LOG_LEVEL_ERR, "Could not check password for user '%s' against stored password. (pam_authenticate: '%s')",
            puser, pam_strerror(NULL, status));
        return false;
    }

    Log(LOG_LEVEL_VERBOSE, "Verifying plaintext password for user '%s': incorrect.", puser);
    return false;
}

static bool ChangePlaintextPasswordUsingLibPam(const char *puser, const char *password)
{
    int status;
    pam_handle_t *handle;
    struct pam_conv conv;
    conv.conv = PasswordSupplier;
    conv.appdata_ptr = (void*)password;

    status = pam_start("passwd", puser, &conv, &handle);
    if (status != PAM_SUCCESS)
    {
        Log(LOG_LEVEL_ERR, "Could not initialize pam session. (pam_start: '%s')", pam_strerror(NULL, status));
        return false;
    }
    Log(LOG_LEVEL_VERBOSE, "Changing password for user '%s'.", puser);
    status = pam_chauthtok(handle, PAM_SILENT);
    pam_end(handle, status);
    if (status == PAM_SUCCESS)
    {
        return true;
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not change password for user '%s'. (pam_chauthtok: '%s')",
            puser, pam_strerror(handle, status));
        return false;
    }
}

static bool ClearPasswordAdministrationFlags(const char *puser)
{
    (void)puser; // Avoid warning.

#ifdef HAVE_PWDADM
    const char *cmd_str = PWDADM " -c ";
    char final_cmd[strlen(cmd_str) + strlen(puser) + 1];

    sprintf(final_cmd, "%s%s", cmd_str, puser);

    Log(LOG_LEVEL_VERBOSE, "Clearing password administration flags for user '%s'. (command: '%s')", puser, final_cmd);

    int status;
    status = system(final_cmd);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        Log(LOG_LEVEL_ERR, "Command failed while trying to clear password flags for user '%s'. (Command: '%s')",
            puser, final_cmd);
        return false;
    }
#endif // HAVE_PWDADM

    return true;
}

#ifdef HAVE_CHPASSWD
static bool ChangePasswordHashUsingChpasswd(const char *puser, const char *password)
{
    int status;
    const char *cmd_str = CHPASSWD " -e";
    Log(LOG_LEVEL_VERBOSE, "Changing password hash for user '%s'. (command: '%s')", puser, cmd_str);
    FILE *cmd = cf_popen_sh(cmd_str, "w");
    if (!cmd)
    {
        Log(LOG_LEVEL_ERR, "Could not launch password changing command '%s': %s.", cmd_str, GetErrorStr());
        return false;
    }

    // String lengths plus a ':' and a '\n', but not including '\0'.
    size_t total_len = strlen(puser) + strlen(password) + 2;
    char change_string[total_len + 1];
    snprintf(change_string, total_len + 1, "%s:%s\n", puser, password);
    clearerr(cmd);
    if (fwrite(change_string, total_len, 1, cmd) != 1)
    {
        const char *error_str;
        if (ferror(cmd))
        {
            error_str = GetErrorStr();
        }
        else
        {
            error_str = "Unknown error";
        }
        Log(LOG_LEVEL_ERR, "Could not write password to password changing command '%s': %s.", cmd_str, error_str);
        cf_pclose(cmd);
        return false;
    }
    status = cf_pclose(cmd);
    if (status)
    {
        Log(LOG_LEVEL_ERR, "'%s' returned non-zero status: %i\n", cmd_str, status);
        return false;
    }

    return ClearPasswordAdministrationFlags(puser);
}
#endif // HAVE_CHPASSWD

#if defined(HAVE_LCKPWDF) && defined(HAVE_ULCKPWDF)
static bool ChangePasswordHashUsingLckpwdf(const char *puser, const char *password)
{
    bool result = false;

    struct stat statbuf;
    const char *passwd_file = "/etc/shadow";
    if (stat(passwd_file, &statbuf) == -1)
    {
        passwd_file = "/etc/passwd";
    }

    Log(LOG_LEVEL_VERBOSE, "Changing password hash for user '%s' by editing '%s'.", puser, passwd_file);

    if (lckpwdf() != 0)
    {
        Log(LOG_LEVEL_ERR, "Not able to obtain lock on password database.");
        return false;
    }

    char backup_file[strlen(passwd_file) + strlen(".cf-backup") + 1];
    snprintf(backup_file, sizeof(backup_file), "%s.cf-backup", passwd_file);
    unlink(backup_file);

    char edit_file[strlen(passwd_file) + strlen(".cf-edit") + 1];
    snprintf(edit_file, sizeof(edit_file), "%s.cf-edit", passwd_file);
    unlink(edit_file);

    if (!CopyRegularFileDisk(passwd_file, backup_file))
    {
        Log(LOG_LEVEL_ERR, "Could not back up existing password database '%s' to '%s'.", passwd_file, backup_file);
        goto unlock_passwd;
    }

    FILE *passwd_fd = fopen(passwd_file, "r");
    if (!passwd_fd)
    {
        Log(LOG_LEVEL_ERR, "Could not open password database '%s'. (fopen: '%s')", passwd_file, GetErrorStr());
        goto unlock_passwd;
    }
    int edit_fd_int = open(edit_file, O_WRONLY | O_CREAT | O_EXCL, S_IWUSR);
    if (edit_fd_int < 0)
    {
        if (errno == EEXIST)
        {
            Log(LOG_LEVEL_CRIT, "Temporary file already existed when trying to open '%s'. (open: '%s') "
                "This should NEVER happen and could mean that someone is trying to break into your system!!",
                edit_file, GetErrorStr());
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not open password database temporary file '%s'. (open: '%s')", edit_file, GetErrorStr());
        }
        goto close_passwd_fd;
    }
    FILE *edit_fd = fdopen(edit_fd_int, "w");
    if (!edit_fd)
    {
        Log(LOG_LEVEL_ERR, "Could not open password database temporary file '%s'. (fopen: '%s')", edit_file, GetErrorStr());
        close(edit_fd_int);
        goto close_passwd_fd;
    }

    while (true)
    {
        size_t line_size = CF_BUFSIZE;
        char *line = xmalloc(line_size);

        int read_result = getline(&line, &line_size, passwd_fd);
        if (read_result < 0)
        {
            if (!feof(passwd_fd))
            {
                Log(LOG_LEVEL_ERR, "Error while reading password database: %s", GetErrorStr());
                free(line);
                goto close_both;
            }
            else
            {
                break;
            }
        }
        else if (read_result >= sizeof(line))
        {
            Log(LOG_LEVEL_ERR, "Unusually long line found in password database while editing user '%s'. Not updating.",
                puser);
        }

        // Editing the password database is risky business, so do as little parsing as possible.
        // Just enough to get the hash in there.
        char *field_start = NULL;
        char *field_end = NULL;
        field_start = strchr(line, ':');
        if (field_start)
        {
            field_end = strchr(field_start + 1, ':');
        }
        if (!field_start || !field_end)
        {
            Log(LOG_LEVEL_ERR, "Unexpected format found in password database while editing user '%s'. Not updating.",
                puser);
            free(line);
            goto close_both;
        }

        // Worst case length: Existing password is empty plus one '\n' and one '\0'.
        char new_line[strlen(line) + strlen(password) + 2];
        *field_start = '\0';
        *field_end = '\0';
        if (strcmp(line, puser) == 0)
        {
            sprintf(new_line, "%s:%s:%s\n", line, password, field_end + 1);
        }
        else
        {
            sprintf(new_line, "%s:%s:%s\n", line, field_start + 1, field_end + 1);
        }

        free(line);

        size_t new_line_size = strlen(new_line);
        size_t written_so_far = 0;
        while (written_so_far < new_line_size)
        {
            clearerr(edit_fd);
            size_t written = fwrite(new_line, 1, new_line_size, edit_fd);
            if (written == 0)
            {
                const char *err_str;
                if (ferror(edit_fd))
                {
                    err_str = GetErrorStr();
                }
                else
                {
                    err_str = "Unknown error";
                }
                Log(LOG_LEVEL_ERR, "Error while writing to file '%s'. (fwrite: '%s')", edit_file, err_str);
                goto close_both;
            }
            written_so_far += written;
        }
    }

    fclose(edit_fd);
    fclose(passwd_fd);

    if (!CopyFilePermissionsDisk(passwd_file, edit_file))
    {
        Log(LOG_LEVEL_ERR, "Could not copy permissions from '%s' to '%s'", passwd_file, edit_file);
        goto unlock_passwd;
    }

    if (rename(edit_file, passwd_file) < 0)
    {
        Log(LOG_LEVEL_ERR, "Could not replace '%s' with edited password database '%s'. (rename: '%s')",
            passwd_file, edit_file, GetErrorStr());
        goto unlock_passwd;
    }

    result = true;

    goto unlock_passwd;

close_both:
    fclose(edit_fd);
    unlink(edit_file);
close_passwd_fd:
    fclose(passwd_fd);
unlock_passwd:
    ulckpwdf();

    return result;
}
#endif // defined(HAVE_LCKPWDF) && defined(HAVE_ULCKPWDF)

static bool ChangePassword(const char *puser, const char *password, PasswordFormat format)
{
    if (format == PASSWORD_FORMAT_PLAINTEXT)
    {
        return ChangePlaintextPasswordUsingLibPam(puser, password);
    }

    assert(format == PASSWORD_FORMAT_HASH);

#ifdef HAVE_CHPASSWD
    struct stat statbuf;
    if (stat(CHPASSWD, &statbuf) != -1)
    {
        return ChangePasswordHashUsingChpasswd(puser, password);
    }
    else
#endif
#if defined(HAVE_LCKPWDF) && defined(HAVE_ULCKPWDF)
    {
        return ChangePasswordHashUsingLckpwdf(puser, password);
    }
#elif defined(HAVE_CHPASSWD)
    {
        Log(LOG_LEVEL_ERR, "No means to set password for user '%s' was found. Tried using the '%s' tool with no luck.",
            puser, CHPASSWD);
        return false;
    }
#else
    {
        Log(LOG_LEVEL_WARNING, "Setting hashed password or locking user '%s' not supported on this platform.", puser);
        return false;
    }
#endif
}

static bool IsAccountLocked(const char *puser, const struct passwd *passwd_info)
{
    /* Note that when we lock an account, we do two things, we make the password hash invalid
     * by adding a '!', and we set the expiry date far in the past. However, we only have the
     * possibility of checking the password hash, because the expire field is not exposed by
     * POSIX functions. This is not a problem as long as you stick to CFEngine, but if the user
     * unlocks the account manually, but forgets to reset the expiry time, CFEngine could think
     * that the account is unlocked when it really isn't.
     */

    const char *system_hash;
    if (!GetPasswordHash(puser, passwd_info, &system_hash))
    {
        return false;
    }
    return (system_hash[0] == '!');
}

static bool SetAccountLocked(const char *puser, const char *hash, bool lock)
{
    char cmd[CF_BUFSIZE + strlen(hash)];

    strcpy (cmd, USERMOD);
    StringAppend(cmd, " -e \"", sizeof(cmd));

    if (lock)
    {
        if (hash[0] != '!')
        {
            char new_hash[strlen(hash) + 2];
            sprintf(new_hash, "!%s", hash);
            if (!ChangePassword(puser, new_hash, PASSWORD_FORMAT_HASH))
            {
                return false;
            }
        }
        StringAppend(cmd, GetPlatformSpecificExpirationDate(), sizeof(cmd));
    }
    else
    {
        // Important to check. Password may already have been changed if that was also
        // specified in the policy.
        if (hash[0] == '!')
        {
            if (!ChangePassword(puser, &hash[1], PASSWORD_FORMAT_HASH))
            {
                return false;
            }
        }
    }

    StringAppend(cmd, "\" ", sizeof(cmd));
    StringAppend(cmd, puser, sizeof(cmd));

    Log(LOG_LEVEL_VERBOSE, "%s user '%s' by setting expiry date. (command: '%s')",
        lock ? "Locking" : "Unlocking", puser, cmd);

    int status;
    status = system(cmd);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        Log(LOG_LEVEL_ERR, "Command returned error while %s user '%s'. (Command line: '%s')",
            lock ? "locking" : "unlocking", puser, cmd);
        return false;
    }

    return true;
}

static bool GroupGetUserMembership (const char *user, StringSet *result)
{
    bool ret = true;
    struct group *group_info;

    setgrent();
    while (true)
    {
        errno = 0;
        group_info = getgrent();
        if (!group_info)
        {
            if (errno)
            {
                Log(LOG_LEVEL_ERR, "Error while getting group list. (getgrent: '%s')", GetErrorStr());
                ret = false;
            }
            break;
        }
        for (int i = 0; group_info->gr_mem[i] != NULL; i++)
        {
            if (strcmp(user, group_info->gr_mem[i]) == 0)
            {
                StringSetAdd(result, xstrdup(group_info->gr_name));
                break;
            }
        }
    }
    endgrent();

    return ret;
}

static void TransformGidsToGroups(StringSet **list)
{
    StringSet *new_list = StringSetNew();
    StringSetIterator i = StringSetIteratorInit(*list);
    const char *data;
    for (data = StringSetIteratorNext(&i); data; data = StringSetIteratorNext(&i))
    {
        if (strlen(data) != strspn(data, "0123456789"))
        {
            // Cannot possibly be a gid.
            StringSetAdd(new_list, xstrdup(data));
            continue;
        }
        // In groups vs gids, groups take precedence. So check if it exists.
        errno = 0;
        struct group *group_info = getgrnam(data);
        if (!group_info)
        {
            switch (errno)
            {
            case 0:
            case ENOENT:
            case EBADF:
            case ESRCH:
            case EWOULDBLOCK:
            case EPERM:
                // POSIX is apparently ambiguous here. All values mean "not found".
                errno = 0;
                group_info = getgrgid(atoi(data));
                if (!group_info)
                {
                    switch (errno)
                    {
                    case 0:
                    case ENOENT:
                    case EBADF:
                    case ESRCH:
                    case EWOULDBLOCK:
                    case EPERM:
                        // POSIX is apparently ambiguous here. All values mean "not found".
                        //
                        // Neither group nor gid is found. This will lead to an error later, but we don't
                        // handle that here.
                        break;
                    default:
                        Log(LOG_LEVEL_ERR, "Error while checking group name '%s'. (getgrgid: '%s')", data, GetErrorStr());
                        StringSetDestroy(new_list);
                        return;
                    }
                }
                else
                {
                    // Replace gid with group name.
                    StringSetAdd(new_list, xstrdup(group_info->gr_name));
                }
                break;
            default:
                Log(LOG_LEVEL_ERR, "Error while checking group name '%s'. (getgrnam: '%s')", data, GetErrorStr());
                StringSetDestroy(new_list);
                return;
            }
        }
        else
        {
            StringSetAdd(new_list, xstrdup(data));
        }
    }
    StringSet *old_list = *list;
    *list = new_list;
    StringSetDestroy(old_list);
}

static bool VerifyIfUserNeedsModifs (const char *puser, User u, const struct passwd *passwd_info,
                             uint32_t *changemap)
{
    if (u.description != NULL && strcmp (u.description, passwd_info->pw_gecos))
    {
        CFUSR_SETBIT (*changemap, i_comment);
    }
    if (u.uid != NULL && (atoi (u.uid) != passwd_info->pw_uid))
    {
        CFUSR_SETBIT (*changemap, i_uid);
    }
    if (u.home_dir != NULL && strcmp (u.home_dir, passwd_info->pw_dir))
    {
        CFUSR_SETBIT (*changemap, i_home);
    }
    if (u.shell != NULL && strcmp (u.shell, passwd_info->pw_shell))
    {
        CFUSR_SETBIT (*changemap, i_shell);
    }
    bool account_is_locked = IsAccountLocked(puser, passwd_info);
    if ((!account_is_locked && u.policy == USER_STATE_LOCKED)
        || (account_is_locked && u.policy != USER_STATE_LOCKED))
    {
        CFUSR_SETBIT(*changemap, i_locked);
    }
    // Don't bother with passwords if the account is going to be locked anyway.
    if (u.password != NULL && strcmp (u.password, "")
        && u.policy != USER_STATE_LOCKED)
    {
        if (!IsPasswordCorrect(puser, u.password, u.password_format, passwd_info))
        {
            CFUSR_SETBIT (*changemap, i_password);
        }
    }

    if (SafeStringLength(u.group_primary))
    {
        bool group_could_be_gid = (strlen(u.group_primary) == strspn(u.group_primary, "0123456789"));
        int gid;

        // We try name first, even if it looks like a gid. Only fall back to gid.
        struct group *group_info;
        errno = 0;
        group_info = getgrnam(u.group_primary);
        // Apparently POSIX is ambiguous here. All the values below mean "not found".
        if (!group_info && errno != 0 && errno != ENOENT && errno != EBADF && errno != ESRCH
            && errno != EWOULDBLOCK && errno != EPERM)
        {
            Log(LOG_LEVEL_ERR, "Could not obtain information about group '%s'. (getgrnam: '%s')", u.group_primary, GetErrorStr());
            gid = -1;
        }
        else if (!group_info)
        {
            if (group_could_be_gid)
            {
                gid = atoi(u.group_primary);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "No such group '%s'.", u.group_primary);
                gid = -1;
            }
        }
        else
        {
            gid = group_info->gr_gid;
        }

        if (gid != passwd_info->pw_gid)
        {
            CFUSR_SETBIT (*changemap, i_group);
        }
    }
    if (u.groups_secondary != NULL)
    {
        StringSet *wanted_groups = StringSetNew();
        for (Rlist *ptr = u.groups_secondary; ptr; ptr = ptr->next)
        {
            if (strcmp(RvalScalarValue(ptr->val), CF_NULL_VALUE) != 0)
            {
                StringSetAdd(wanted_groups, xstrdup(RvalScalarValue(ptr->val)));
            }
        }
        TransformGidsToGroups(&wanted_groups);
        StringSet *current_groups = StringSetNew();
        if (!GroupGetUserMembership (puser, current_groups))
        {
            CFUSR_SETBIT (*changemap, i_groups);
        }
        else if (!StringSetIsEqual (current_groups, wanted_groups))
        {
            CFUSR_SETBIT (*changemap, i_groups);
        }
        StringSetDestroy(current_groups);
        StringSetDestroy(wanted_groups);
    }

    ////////////////////////////////////////////
    if (*changemap == 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

static bool DoCreateUser(const char *puser, User u, enum cfopaction action,
                         EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    char cmd[CF_BUFSIZE];
    if (puser == NULL || !strcmp (puser, ""))
    {
        return false;
    }
    strcpy (cmd, USERADD);

    if (u.uid != NULL && strcmp (u.uid, ""))
    {
        StringAppend(cmd, " -u \"", sizeof(cmd));
        StringAppend(cmd, u.uid, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }

    if (u.description != NULL)
    {
        StringAppend(cmd, " -c \"", sizeof(cmd));
        StringAppend(cmd, u.description, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }

    if (u.group_primary != NULL && strcmp (u.group_primary, ""))
    {
        // TODO: Should check that group exists
        StringAppend(cmd, " -g \"", sizeof(cmd));
        StringAppend(cmd, u.group_primary, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }
    if (u.groups_secondary != NULL)
    {
        // TODO: Should check that groups exist
        StringAppend(cmd, " -G \"", sizeof(cmd));
        char sep[2] = { '\0', '\0' };
        for (Rlist *i = u.groups_secondary; i; i = i->next)
        {
            if (strcmp(RvalScalarValue(i->val), CF_NULL_VALUE) != 0)
            {
                StringAppend(cmd, sep, sizeof(cmd));
                StringAppend(cmd, RvalScalarValue(i->val), sizeof(cmd));
                sep[0] = ',';
            }
        }
        StringAppend(cmd, "\"", sizeof(cmd));
    }
    if (u.home_dir != NULL && strcmp (u.home_dir, ""))
    {
        StringAppend(cmd, " -d \"", sizeof(cmd));
        StringAppend(cmd, u.home_dir, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }
    if (u.shell != NULL && strcmp (u.shell, ""))
    {
        StringAppend(cmd, " -s \"", sizeof(cmd));
        StringAppend(cmd, u.shell, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }
    StringAppend(cmd, " ", sizeof(cmd));
    StringAppend(cmd, puser, sizeof(cmd));

    if (action == cfa_warn || DONTDO)
    {
        Log(LOG_LEVEL_NOTICE, "Need to create user '%s'.", puser);
        return false;
    }
    else
    {
        if (strlen(cmd) >= sizeof(cmd) - 1)
        {
            // Instead of checking every string call above, assume that a maxed out
            // string length overflowed the string.
            Log(LOG_LEVEL_ERR, "Command line too long while creating user '%s'", puser);
            return false;
        }

        Log(LOG_LEVEL_VERBOSE, "Creating user '%s'. (command: '%s')", puser, cmd);

        int status;
        status = system(cmd);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            Log(LOG_LEVEL_ERR, "Command returned error while creating user '%s'. (Command line: '%s')", puser, cmd);
            return false;
        }

        // Initially, "useradd" may set the password to '!', which confuses our detection for
        // locked accounts. So reset it to 'x' hash instead, which will never match anything.
        if (!ChangePassword(puser, "x", PASSWORD_FORMAT_HASH))
        {
            return false;
        }

        if (u.policy == USER_STATE_LOCKED)
        {
            if (!SetAccountLocked(puser, "", true))
            {
                return false;
            }
        }

        if (a->havebundle)
        {
            const Constraint *method_attrib = PromiseGetConstraint(pp, "home_bundle");
            VerifyMethod(ctx, method_attrib->rval, *a, pp);
        }

        if (u.policy != USER_STATE_LOCKED && u.password != NULL && strcmp (u.password, ""))
        {
            if (!ChangePassword(puser, u.password, u.password_format))
            {
                return false;
            }
        }
    }

    return true;
}

static bool DoRemoveUser (const char *puser, enum cfopaction action)
{
    char cmd[CF_BUFSIZE];

    strcpy (cmd, USERDEL);

    StringAppend(cmd, " ", sizeof(cmd));
    StringAppend(cmd, puser, sizeof(cmd));

    if (action == cfa_warn || DONTDO)
    {
        Log(LOG_LEVEL_NOTICE, "Need to remove user '%s'.", puser);
        return false;
    }
    else
    {
        if (strlen(cmd) >= sizeof(cmd) - 1)
        {
            // Instead of checking every string call above, assume that a maxed out
            // string length overflowed the string.
            Log(LOG_LEVEL_ERR, "Command line too long while removing user '%s'", puser);
            return false;
        }

        Log(LOG_LEVEL_VERBOSE, "Removing user '%s'. (command: '%s')", puser, cmd);

        int status;
        status = system(cmd);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            Log(LOG_LEVEL_ERR, "Command returned error while removing user '%s'. (Command line: '%s')", puser, cmd);
            return false;
        }
    }
    return true;
}

static bool DoModifyUser (const char *puser, User u, const struct passwd *passwd_info, uint32_t changemap, enum cfopaction action)
{
    char cmd[CF_BUFSIZE];

    strcpy (cmd, USERMOD);

    if (CFUSR_CHECKBIT (changemap, i_uid) != 0)
    {
        StringAppend(cmd, " -u \"", sizeof(cmd));
        StringAppend(cmd, u.uid, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }

    if (CFUSR_CHECKBIT (changemap, i_comment) != 0)
    {
        StringAppend(cmd, " -c \"", sizeof(cmd));
        StringAppend(cmd, u.description, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }

    if (CFUSR_CHECKBIT (changemap, i_group) != 0)
    {
        StringAppend(cmd, " -g \"", sizeof(cmd));
        StringAppend(cmd, u.group_primary, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }

    if (CFUSR_CHECKBIT (changemap, i_groups) != 0)
    {
        StringAppend(cmd, " -G \"", sizeof(cmd));
        char sep[2] = { '\0', '\0' };
        for (Rlist *i = u.groups_secondary; i; i = i->next)
        {
            if (strcmp(RvalScalarValue(i->val), CF_NULL_VALUE) != 0)
            {
                StringAppend(cmd, sep, sizeof(cmd));
                StringAppend(cmd, RvalScalarValue(i->val), sizeof(cmd));
                sep[0] = ',';
            }
        }
        StringAppend(cmd, "\"", sizeof(cmd));
    }

    if (CFUSR_CHECKBIT (changemap, i_home) != 0)
    {
        StringAppend(cmd, " -d \"", sizeof(cmd));
        StringAppend(cmd, u.home_dir, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }

    if (CFUSR_CHECKBIT (changemap, i_shell) != 0)
    {
        StringAppend(cmd, " -s \"", sizeof(cmd));
        StringAppend(cmd, u.shell, sizeof(cmd));
        StringAppend(cmd, "\"", sizeof(cmd));
    }

    StringAppend(cmd, " ", sizeof(cmd));
    StringAppend(cmd, puser, sizeof(cmd));

    if (CFUSR_CHECKBIT (changemap, i_password) != 0)
    {
        if (action == cfa_warn || DONTDO)
        {
            Log(LOG_LEVEL_NOTICE, "Need to change password for user '%s'.", puser);
            return false;
        }
        else
        {
            if (!ChangePassword(puser, u.password, u.password_format))
            {
                return false;
            }
        }
    }

    if (CFUSR_CHECKBIT (changemap, i_locked) != 0)
    {
        if (action == cfa_warn || DONTDO)
        {
            Log(LOG_LEVEL_NOTICE, "Need to %s account for user '%s'.",
                (u.policy == USER_STATE_LOCKED) ? "lock" : "unlock", puser);
            return false;
        }
        else
        {
            const char *hash;
            if (!GetPasswordHash(puser, passwd_info, &hash))
            {
                return false;
            }
            if (!SetAccountLocked(puser, hash, (u.policy == USER_STATE_LOCKED)))
            {
                return false;
            }
        }
    }

    // If password and locking were the only things changed, don't run the command.
    CFUSR_CLEARBIT(changemap, i_password);
    CFUSR_CLEARBIT(changemap, i_locked);
    if (action == cfa_warn || DONTDO)
    {
        Log(LOG_LEVEL_NOTICE, "Need to update user attributes (command '%s').", cmd);
        return false;
    }
    else if (changemap != 0)
    {
        if (strlen(cmd) >= sizeof(cmd) - 1)
        {
            // Instead of checking every string call above, assume that a maxed out
            // string length overflowed the string.
            Log(LOG_LEVEL_ERR, "Command line too long while modifying user '%s'", puser);
            return false;
        }

        Log(LOG_LEVEL_VERBOSE, "Modifying user '%s'. (command: '%s')", puser, cmd);

        int status;
        status = system(cmd);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            Log(LOG_LEVEL_ERR, "Command returned error while modifying user '%s'. (Command line: '%s')", puser, cmd);
            return false;
        }
    }
    return true;
}

void VerifyOneUsersPromise (const char *puser, User u, PromiseResult *result, enum cfopaction action,
                            EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    bool res;

    struct passwd *passwd_info;
    errno = 0;
    passwd_info = getpwnam(puser);
    // Apparently POSIX is ambiguous here. All the values below mean "not found".
    if (!passwd_info && errno != 0 && errno != ENOENT && errno != EBADF && errno != ESRCH
        && errno != EWOULDBLOCK && errno != EPERM)
    {
        Log(LOG_LEVEL_ERR, "Could not get information from user database. (getpwnam: '%s')", GetErrorStr());
        return;
    }

    if (u.policy == USER_STATE_PRESENT || u.policy == USER_STATE_LOCKED)
    {
        if (passwd_info)
        {
            uint32_t cmap = 0;
            if (VerifyIfUserNeedsModifs (puser, u, passwd_info, &cmap))
            {
                res = DoModifyUser (puser, u, passwd_info, cmap, action);
                if (res)
                {
                    *result = PROMISE_RESULT_CHANGE;
                }
                else
                {
                    *result = PROMISE_RESULT_FAIL;
                }
            }
            else
            {
                *result = PROMISE_RESULT_NOOP;
            }
        }
        else
        {
            res = DoCreateUser (puser, u, action, ctx, a, pp);
            if (res)
            {
                *result = PROMISE_RESULT_CHANGE;
            }
            else
            {
                *result = PROMISE_RESULT_FAIL;
            }
        }
    }
    else if (u.policy == USER_STATE_ABSENT)
    {
        if (passwd_info)
        {
            res = DoRemoveUser (puser, action);
            if (res)
            {
                *result = PROMISE_RESULT_CHANGE;
            }
            else
            {
                *result = PROMISE_RESULT_FAIL;
            }
        }
        else
        {
            *result = PROMISE_RESULT_NOOP;
        }
    }
}
