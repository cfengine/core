/*
  Copyright 2019 Northern.tech AS

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

#include <bootstrap.h>

#include <policy_server.h>
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
#include <addr_lib.h>
#include <communication.h>
#include <client_code.h>
#include <assert.h>
#include <crypto.h>
#include <openssl/rand.h>
#include <encode.h>

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


/**
 * @brief Sets both internal C variables as well as policy sys variables.
 *
 * Called at bootstrap and after reading policy_server.dat.
 * Changes sys.policy_hub and sys.policy_hub_port.
 * NULL is an acceptable value for new_policy_server. Could happen when an
 * already bootstrapped server re-parses its policies, and the
 * policy_server.dat file has been removed. Then this function will be called
 * with NULL as new_policy_server, and cf-serverd will keep running even
 * without a policy server set.
 *
 * @param ctx EvalContext is used to set related variables
 * @param new_policy_server can be 'host:port', same as policy_server.dat
 */
void EvalContextSetPolicyServer(EvalContext *ctx, const char *new_policy_server)
{
    // Remove variables if undefined policy server:
    if ( NULL_OR_EMPTY(new_policy_server) )
    {
        EvalContextVariableRemoveSpecial(   ctx, SPECIAL_SCOPE_SYS,
                                            "policy_hub" );
        EvalContextVariableRemoveSpecial(   ctx, SPECIAL_SCOPE_SYS,
                                            "policy_hub_port" );
        return;
    }

    PolicyServerSet(new_policy_server);
    const char *ip = PolicyServerGetIP();

    // Set the sys.policy_hub variable:
    if ( ip != NULL )
    {
        EvalContextVariablePutSpecial(  ctx,  SPECIAL_SCOPE_SYS,
                                        "policy_hub", ip,
                                        CF_DATA_TYPE_STRING,
                                        "source=bootstrap" );
    }
    else
    {
        EvalContextVariableRemoveSpecial(   ctx, SPECIAL_SCOPE_SYS,
                                            "policy_hub" );
    }

    // Set the sys.policy_hub_port variable:
    if (PolicyServerGetPort() != NULL)
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS,
                                      "policy_hub_port", PolicyServerGetPort(),
                                      CF_DATA_TYPE_STRING,
                                      "source=bootstrap" );
    }
    else // Default value (CFENGINE_PORT_STR = "5308") is set
    {
        EvalContextVariablePutSpecial( ctx, SPECIAL_SCOPE_SYS,
                                       "policy_hub_port",
                                       CFENGINE_PORT_STR,
                                       CF_DATA_TYPE_STRING,
                                       "source=bootstrap" );
    }
}

//*******************************************************************
// POLICY SERVER FILE FUNCTIONS:
//*******************************************************************

/**
 * @brief     Reads the policy_server.dat and sets the internal variables.
 * @param[in] ctx EvalContext used by EvalContextSetPolicyServer()
 * @param[in] workdir the directory of policy_server.dat usually GetWorkDir()
 */
 void EvalContextSetPolicyServerFromFile(EvalContext *ctx, const char *workdir)
 {
     char *contents = PolicyServerReadFile(workdir);
     EvalContextSetPolicyServer(ctx, contents);
     free(contents);
 }



//*******************************************************************
// POLICY HUB FUNCTIONS:
//*******************************************************************

/**
 * @brief Updates sys.last_policy_update variable from $(sys.masterdir)/cf_promises_validated
 * @param ctx EvalContext to put variable into
 */
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

/**
 * @return True if the file STATEDIR/am_policy_hub exists
 */
 bool GetAmPolicyHub(void)
 {
     char path[CF_BUFSIZE] = { 0 };
     snprintf(path, sizeof(path), "%s/am_policy_hub", GetStateDir());
     MapName(path);

     struct stat sb;
     return stat(path, &sb) == 0;
 }

/**
 * @brief Set the STATEDIR/am_policy_hub marker file.
 * @param am_policy_hub If true, create marker file. If false, delete it.
 * @return True if successful
 */
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


//*******************************************************************
// FAILSAFE FUNCTIONS:
//*******************************************************************

/**
 * @brief Write the builtin failsafe policy to the default location
 * @return True if successful
 */
bool WriteBuiltinFailsafePolicy(const char *inputdir)
{
    char failsafe_path[CF_BUFSIZE];
    snprintf(failsafe_path, CF_BUFSIZE - 1, "%s/failsafe.cf", inputdir);
    MapName(failsafe_path);

    return WriteBuiltinFailsafePolicyToPath(failsafe_path);
}

/**
 * @brief Exposed for testing. Use WriteBuiltinFailsafePolicy.
 */
bool WriteBuiltinFailsafePolicyToPath(const char *filename)
{
    // The bootstrap.inc file is generated by "make bootstrap-inc"
    const char *bootstrap_content =
#include "bootstrap.inc"
;

    Log(LOG_LEVEL_INFO, "Writing built-in failsafe policy to '%s'", filename);

    FILE *fout = safe_fopen(filename, "w");
    if (!fout)
    {
        Log(LOG_LEVEL_ERR, "Unable to write failsafe to '%s' (fopen: %s)", filename, GetErrorStr());
        return false;
    }

    fputs(bootstrap_content, fout);
    fclose(fout);

    return true;
}


//*******************************************************************
// POLICY FILE FUNCTIONS:
//*******************************************************************

/**
 * @brief Removes all files in $(sys.inputdir)
 * @param inputdir
 * @return True if successful
 */
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

/**
 * @return True if the file $(sys.masterdir)/promises.cf exists
 */
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

static char *BootstrapIDFilename(const char *workdir)
{
    assert(workdir != NULL);
    return StringFormat("%s%cbootstrap_id.dat", workdir, FILE_SEPARATOR);
}

char *CreateBootstrapIDFile(const char *workdir)
{
    assert(workdir != NULL);
    char *filename = BootstrapIDFilename(workdir);

    FILE *file = safe_fopen_create_perms(filename, "w", CF_PERMS_DEFAULT);
    if (file == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to write bootstrap id file '%s' (fopen: %s)", filename, GetErrorStr());
        free(filename);
        return NULL;
    }
    CryptoInitialize();
    #define RANDOM_BYTES 240 / 8 // 240 avoids padding (divisible by 6)
    #define BASE_64_LENGTH_NO_PADDING (4 * (RANDOM_BYTES / 3))
    unsigned char buf[RANDOM_BYTES];
    RAND_bytes(buf, RANDOM_BYTES);
    char *b64_id = StringEncodeBase64(buf, RANDOM_BYTES);
    fprintf(file, "%s\n", b64_id);
    fclose(file);

    free(filename);
    return b64_id;
}

char *ReadBootstrapIDFile(const char *workdir)
{
    assert(workdir != NULL);

    char *const path = BootstrapIDFilename(workdir);
    Writer *writer = FileRead(path, BASE_64_LENGTH_NO_PADDING + 1, NULL);
    if (writer == NULL)
    {
        // Not having a bootstrap id file is considered normal
        Log(LOG_LEVEL_DEBUG,
            "Could not read bootstrap ID from file: '%s'",
            path);
        free(path);
        return NULL;
    }
    char *data = StringWriterClose(writer);

    size_t data_length = strlen(data);
    assert(data_length == BASE_64_LENGTH_NO_PADDING + 1);
    assert(data[data_length - 1] == '\n');
    if (data_length != BASE_64_LENGTH_NO_PADDING + 1)
    {
        Log(LOG_LEVEL_ERR, "'%s' contains invalid data: '%s'", path, data);
        free(path);
        free(data);
        return NULL;
    }

    data[data_length - 1] = '\0';

    Log(LOG_LEVEL_VERBOSE,
        "Successfully read bootstrap ID '%s' from file '%s'",
        data,
        path);
    free(path);
    return data;
}

void EvalContextSetBootstrapID(EvalContext *ctx, char *bootstrap_id)
{
    EvalContextVariablePutSpecial(
        ctx,
        SPECIAL_SCOPE_SYS,
        "bootstrap_id",
        bootstrap_id,
        CF_DATA_TYPE_STRING,
        "source=bootstrap");
}
