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

#include <bootstrap.h>

#include <eval_context.h>
#include <files_names.h>
#include <scope.h>
#include <files_interfaces.h>
#include <exec_tools.h>
#include <generic_agent.h> // PrintVersionBanner
#include <audit.h>
#include <logging.h>
#include <string_lib.h>
#include <files_lib.h>
#include <known_dirs.h>

#include <assert.h>

/*

Bootstrapping is a tricky sequence of fragile events. We need to map shakey/IP
and identify policy hub IP in a special order to bootstrap the license and agents.

During commercial bootstrap:

 - InitGA (generic-agent) loads the public key
 - The verifylicense function sets the policy hub but fails to verify license yet
   as there is no key/IP binding
 - Policy server gets set in workdir/state/am_policy_hub
 - The agents gets run and start this all over again, but this time
   the am_policy_hub is defined and caches the key/IP binding
 - Now the license has a binding, resolves the policy hub's key and succeeds

*/

/*****************************************************************************/

#if defined(__CYGWIN__) || defined(__ANDROID__)

bool BootstrapAllowed(void)
{
    return true;
}

#elif !defined(__MINGW32__)

bool BootstrapAllowed(void)
{
    return IsPrivileged();
}

#endif

/*****************************************************************************/

static char *AmPolicyHubFilename(void)
{
    return StringFormat("%s%cam_policy_hub", GetStateDir(), FILE_SEPARATOR);
}

bool WriteAmPolicyHubFile(bool am_policy_hub)
{
    char *filename = AmPolicyHubFilename();
    if (am_policy_hub)
    {
        if (!GetAmPolicyHub())
        {
            if (creat(filename, 0600) == -1)
            {
                Log(LOG_LEVEL_ERR, "Error writing marker file '%s'", filename);
                free(filename);
                return false;
            }
        }
    }
    else
    {
        if (GetAmPolicyHub())
        {
            if (unlink(filename) != 0)
            {
                Log(LOG_LEVEL_ERR, "Error removing marker file '%s'", filename);
                free(filename);
                return false;
            }
        }
    }
    free(filename);
    return true;
}

/* NULL is an acceptable value for new_policy_server. Could happen when an
 * already bootstrapped server re-parses its policies, and the
 * policy_server.dat file has been removed. Then this function will be called
 * with NULL as new_policy_server, and cf-serverd will keep running even
 * without a policy server set. */
void SetPolicyServer(EvalContext *ctx, const char *new_policy_server)
{
    if (new_policy_server)
    {
        xsnprintf(POLICY_SERVER, CF_MAX_IP_LEN, "%s", new_policy_server);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "policy_hub", new_policy_server, CF_DATA_TYPE_STRING, "source=bootstrap");
    }
    else
    {
        strcpy(POLICY_SERVER, "");
        EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_SYS, "policy_hub");
    }
}

/* Set "sys.last_policy_update" variable. */
void UpdateLastPolicyUpdateTime(EvalContext *ctx)
{
    // Get the timestamp on policy update
    struct stat sb;
    {
        char cf_promises_validated_filename[CF_MAXVARSIZE];
        snprintf(cf_promises_validated_filename, CF_MAXVARSIZE, "%s/cf_promises_validated", GetMasterDir());
        MapName(cf_promises_validated_filename);

        if ((stat(cf_promises_validated_filename, &sb)) != 0)
        {
            return;
        }
    }

    char timebuf[26] = { 0 };
    cf_strtimestamp_local(sb.st_mtime, timebuf);

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "last_policy_update", timebuf, CF_DATA_TYPE_STRING, "source=agent");
}

static char *PolicyServerFilename(const char *workdir)
{
    return StringFormat("%s%cpolicy_server.dat", workdir, FILE_SEPARATOR);
}

char *ReadPolicyServerFile(const char *workdir)
{
    char contents[CF_MAX_IP_LEN] = "";

    char *filename = PolicyServerFilename(workdir);
    FILE *fp = fopen(filename, "r");
    free(filename);

    if (fp)
    {
        if (fscanf(fp, "%63s", contents) != 1)
        {
            fclose(fp);
            return NULL;
        }
        fclose(fp);
        return xstrdup(contents);
    }
    else
    {
        return NULL;
    }
}

bool WritePolicyServerFile(const char *workdir, const char *new_policy_server)
{
    char *filename = PolicyServerFilename(workdir);

    FILE *file = fopen(filename, "w");
    if (!file)
    {
        Log(LOG_LEVEL_ERR, "Unable to write policy server file '%s' (fopen: %s)", filename, GetErrorStr());
        free(filename);
        return false;
    }

    fprintf(file, "%s", new_policy_server);
    fclose(file);

    free(filename);
    return true;
}

bool RemovePolicyServerFile(const char *workdir)
{
    char *filename = PolicyServerFilename(workdir);

    if (unlink(filename) != 0)
    {
        Log(LOG_LEVEL_ERR, "Unable to remove file '%s'. (unlink: %s)", filename, GetErrorStr());
        free(filename);
        return false;
    }

    return true;
}

bool GetAmPolicyHub(void)
{
    char path[CF_BUFSIZE] = { 0 };
    snprintf(path, sizeof(path), "%s/am_policy_hub", GetStateDir());
    MapName(path);

    struct stat sb;
    return stat(path, &sb) == 0;
}

bool RemoveAllExistingPolicyInInputs(const char *inputs_path)
{
    Log(LOG_LEVEL_INFO, "Removing all files in '%s'", inputs_path);

    struct stat sb;
    if (stat(inputs_path, &sb) == -1)
    {
        if (errno == ENOENT)
        {
            return true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not stat inputs directory at '%s'. (stat: %s)", inputs_path, GetErrorStr());
            return false;
        }
    }

    if (!S_ISDIR(sb.st_mode))
    {
        Log(LOG_LEVEL_ERR, "Inputs path exists at '%s', but it is not a directory", inputs_path);
        return false;
    }

    return DeleteDirectoryTree(inputs_path);
}

bool MasterfileExists(const char *masterdir)
{
    char filename[CF_BUFSIZE] = { 0 };
    snprintf(filename, sizeof(filename), "%s/promises.cf", masterdir);
    MapName(filename);

    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        if (errno == ENOENT)
        {
            return false;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not stat file '%s'. (stat: %s)", filename, GetErrorStr());
            return false;
        }
    }

    if (!S_ISREG(sb.st_mode))
    {
        Log(LOG_LEVEL_ERR, "Path exists at '%s', but it is not a regular file", filename);
        return false;
    }

    return true;
}

/********************************************************************/

bool WriteBuiltinFailsafePolicyToPath(const char *filename)
{
    // The bootstrap.inc file is generated by "make bootstrap-inc"
    const char * bootstrap_content =
#include "bootstrap.inc"
;

    Log(LOG_LEVEL_INFO, "Writing built-in failsafe policy to '%s'", filename);

    FILE *fout = fopen(filename, "w");
    if (!fout)
    {
        Log(LOG_LEVEL_ERR, "Unable to write failsafe to '%s' (fopen: %s)", filename, GetErrorStr());
        return false;
    }

    fputs(bootstrap_content, fout);
    fclose(fout);

    if (chmod(filename, S_IRUSR | S_IWUSR) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed setting permissions on generated failsafe file '%s'", filename);
        return false;
    }

    return true;
}

bool WriteBuiltinFailsafePolicy(const char *inputdir)
{
    char failsafe_path[CF_BUFSIZE];
    snprintf(failsafe_path, CF_BUFSIZE - 1, "%s/failsafe.cf", inputdir);
    MapName(failsafe_path);

    return WriteBuiltinFailsafePolicyToPath(failsafe_path);
}
