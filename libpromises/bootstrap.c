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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <bootstrap.h>

#include <env_context.h>
#include <files_names.h>
#include <scope.h>
#include <files_interfaces.h>
#include <exec_tools.h>
#include <generic_agent.h> // PrintVersionBanner
#include <audit.h>
#include <logging.h>
#include <string_lib.h>
#include <files_lib.h>

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

static char *AmPolicyHubFilename(const char *workdir)
{
    return StringFormat("%s%cstate%cam_policy_hub", workdir, FILE_SEPARATOR, FILE_SEPARATOR);
}

bool WriteAmPolicyHubFile(const char *workdir, bool am_policy_hub)
{
    char *filename = AmPolicyHubFilename(workdir);
    if (am_policy_hub)
    {
        if (!GetAmPolicyHub(workdir))
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
        if (GetAmPolicyHub(workdir))
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

void SetPolicyServer(EvalContext *ctx, const char *new_policy_server)
{
    assert(new_policy_server != NULL);

    if (new_policy_server)
    {
        snprintf(POLICY_SERVER, CF_MAX_IP_LEN, "%s", new_policy_server);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "policy_hub", new_policy_server, DATA_TYPE_STRING, "goal=update,source=bootstrap");
    }

    // Get the timestamp on policy update
    struct stat sb;
    {
        char cf_promises_validated_filename[CF_MAXVARSIZE];
        snprintf(cf_promises_validated_filename, CF_MAXVARSIZE, "%s/masterfiles/cf_promises_validated", CFWORKDIR);
        MapName(cf_promises_validated_filename);

        if ((stat(cf_promises_validated_filename, &sb)) != 0)
        {
            return;
        }
    }
    
    char timebuf[26];
    cf_strtimestamp_local(sb.st_mtime, timebuf);
    
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "last_policy_update", timebuf, DATA_TYPE_STRING, "goal=update,source=agent");
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

bool GetAmPolicyHub(const char *workdir)
{
    char path[CF_BUFSIZE] = { 0 };
    snprintf(path, sizeof(path), "%s/state/am_policy_hub", workdir);
    MapName(path);

    struct stat sb;
    return stat(path, &sb) == 0;
}

bool RemoveAllExistingPolicyInInputs(const char *workdir)
{
    char inputs_path[CF_BUFSIZE] = { 0 };
    snprintf(inputs_path, sizeof(inputs_path), "%s/inputs/", workdir);
    MapName(inputs_path);

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

bool MasterfileExists(const char *workdir)
{
    char filename[CF_BUFSIZE] = { 0 };
    snprintf(filename, sizeof(filename), "%s/masterfiles/promises.cf", workdir);
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
    Log(LOG_LEVEL_INFO, "Writing built-in failsafe policy to '%s'", filename);

    FILE *fout = fopen(filename, "w");
    if (!fout)
    {
        Log(LOG_LEVEL_ERR, "Unable to write failsafe to '%s' (fopen: %s)", filename, GetErrorStr());
        return false;
    }

    fprintf(fout,
            "################################################################################\n"
            "# THIS FILE REPRESENTS A FALL-BACK SOLUTION FOR THE PRIMARY FAILSAFE FILE.\n"
            "# IF THE PRIMARY FAILSAFE/UPDATE LOSES FUNCTIONALITY DUE TO MODIFICATIONS MADE\n" 
            "# BY THE USER, CFENGINE WILL RECOVER BY USING THIS FALL-BACK BOOTSTRAPPED FILE.\n"
            "# NEVER EDIT THIS FILE, YOU WILL HAVE TO LOG ON TO EVERY NODE MANAGED BY\n" 
            "# CFENGINE TO RECTIFY POTENTIAL ERRORS IF SOMETHING GOES WRONG.\n"
            "################################################################################\n"
            "\nbody common control\n"
            "{\n"
            " bundlesequence => { \"cfe_internal_update\" };\n"
            "}\n\n"
            "################################################################################\n"
            "\nbody agent control\n"
            "{\n"
            " skipidentify => \"true\";\n"
            "}\n\n"
            "################################################################################\n"
            "\nbundle agent cfe_internal_update\n"
            "{\n"
            " classes:\n\n"
            "  any::\n"
            "   \"have_ppkeys\"\n"
            "      expression => fileexists(\"$(sys.workdir)/ppkeys/localhost.pub\"),\n"
            "          handle => \"cfe_internal_bootstrap_update_classes_have_ppkeys\";\n"
            "   \"have_promises_cf\"\n"
            "      expression => fileexists(\"$(sys.workdir)/inputs/promises.cf\"),\n"
            "          handle => \"cfe_internal_bootstrap_update_classes_have_promises_cf\";\n"
            "\n#\n\n"
            " commands:\n\n"
            "  gen_keys::\n"
            "   \"$(sys.cf_key)\"\n"
            "      handle => \"cfe_internal_bootstrap_update_commands_generate_keys\";\n"
            "\n#\n\n"
            " files:\n\n"
            "  !windows.have_ppkeys::\n"
            "   \"$(sys.workdir)/inputs\" \n"
            "            handle => \"cfe_internal_bootstrap_update_files_sys_workdir_inputs_not_windows\",\n"
#ifdef __MINGW32__
            "         copy_from => u_scp(\"/var/cfengine/masterfiles\"),\n"
#else
            "         copy_from => u_scp(\"%s/masterfiles\"),\n"
#endif /* !__MINGW32__ */
            "      depth_search => u_recurse(\"inf\"),\n"
            "           classes => repaired(\"got_policy\");\n"
            "\n"
            "  windows.have_ppkeys::\n"
            "   \"$(sys.workdir)\\inputs\" \n"
            "            handle => \"cfe_internal_bootstrap_update_files_sys_workdir_inputs_windows\",\n"
#ifdef __MINGW32__
            "         copy_from => u_scp(\"/var/cfengine/masterfiles\"),\n"
#else
            "         copy_from => u_scp(\"%s/masterfiles\"),\n"
#endif /* !__MINGW32__ */
            "      depth_search => u_recurse(\"inf\"),\n"
            "           classes => repaired(\"got_policy\");\n\n"
            "   \"$(sys.workdir)\\bin-twin\\.\"\n"
            "            handle => \"cfe_internal_bootstrap_update_files_sys_workdir_bin_twin_windows\",\n"
            "           comment => \"Make sure we maintain a clone of the binaries and libraries for updating\",\n"
            "         copy_from => u_cp(\"$(sys.workdir)\\bin\\.\"),\n"
            "      depth_search => u_recurse(\"1\");\n"
            "\n#\n\n"
            " processes:\n\n"
            "  !windows.got_policy::\n"
            "   \"cf-execd\" restart_class => \"start_exec\",\n"
            "                     handle => \"cfe_internal_bootstrap_update_processes_start_cf_execd\";\n"
            "  am_policy_hub.got_policy::\n"
            "   \"cf-serverd\" restart_class => \"start_server\",\n"
            "                       handle => \"cfe_internal_bootstrap_update_processes_start_cf_serverd\";\n"
            "\n#\n\n"
            " commands:\n\n"
            "  start_exec.!windows::\n"
            "   \"$(sys.cf_execd)\"\n"
            "       handle => \"cfe_internal_bootstrap_update_commands_check_sys_cf_execd_start\",\n"
            "      classes => repaired(\"executor_started\");\n"
            "  start_server::\n"
            "   \"$(sys.cf_serverd)\"\n"
            "       handle => \"cfe_internal_bootstrap_update_commands_check_sys_cf_serverd_start\",\n"
            "       action => ifwin_bg,\n"
            "      classes => repaired(\"server_started\");\n"
            "\n#\n\n"
            " services:\n\n"
            "  windows.got_policy::\n"
            "   \"CfengineNovaExec\"\n"
            "              handle => \"cfe_internal_bootstrap_update_services_windows_executor\",\n"
            "      service_policy => \"start\",\n"
            "      service_method => bootstart,\n"
            "             classes => repaired(\"executor_started\");\n"
            "\n#\n\n"
            " reports:\n\n"
            "  !have_ppkeys::\n"
            "   \"Could not find key at $(sys.workdir)/ppkeys/localhost.pub\"\n"
            "     classes => repaired(\"gen_keys\");"
            "  bootstrap_mode.am_policy_hub::\n"
            "   \"This host assumes the role of policy server\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_assume_policy_hub\";\n"
            "  bootstrap_mode.!am_policy_hub::\n"
            "   \"This autonomous node assumes the role of voluntary client\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_assume_voluntary_client\";\n"
            "  got_policy::\n"
            "   \"Updated local policy from policy server\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_got_policy\";\n"
            "  !got_policy.!have_promises_cf.have_ppkeys::\n"
            "   \"Failed to copy policy from policy server at $(sys.policy_hub):$(sys.workdir)/masterfiles\n"
            "       Please check\n"
            "       * cf-serverd is running on $(sys.policy_hub)\n"
            "       * network connectivity to $(sys.policy_hub) on port 5308\n"
            "       * masterfiles 'body server control' - in particular allowconnects, trustkeysfrom and skipverify\n"
            "       * masterfiles 'bundle server' -> access: -> masterfiles -> admit/deny\n"
            "       It is often useful to restart cf-serverd in verbose mode (cf-serverd -v) on $(sys.policy_hub) to diagnose connection issues.\n"
            "       When updating masterfiles, wait (usually 5 minutes) for files to propagate to inputs on $(sys.policy_hub) before retrying.\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_did_not_get_policy\";\n"
            "  server_started::\n"
            "   \"Started the server\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_started_serverd\";\n"
            "  am_policy_hub.!server_started.!have_promises_cf.have_ppkeys::\n"
            "   \"Failed to start the server\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_failed_to_start_serverd\";\n"
            "  executor_started::\n"
            "   \"Started the scheduler\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_started_execd\";\n"
            "  !executor_started.!have_promises_cf.have_ppkeys::\n"
            "   \"Did not start the scheduler\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_failed_to_start_execd\";\n"
            "  !executor_started.have_promises_cf::\n"
            "   \"You are running a hard-coded failsafe. Please use the following command instead.\n"
            "      $(sys.cf_agent) -f $(sys.workdir)/inputs/update.cf\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_run_another_failsafe_instead\";\n"
            "}\n\n"
            "############################################\n"
            "body classes repaired(x)\n"
            "{\n"
            "promise_repaired => {\"$(x)\"};\n"
            "}\n"
            "############################################\n"
            "body perms u_p(p)\n"
            "{\n"
            "mode  => \"$(p)\";\n"
            "}\n"
            "#############################################\n"
            "body copy_from u_scp(from)\n"
            "{\n"
            "source      => \"$(from)\";\n"
            "compare     => \"digest\";\n"
            "trustkey    => \"true\";\n"
            "!am_policy_hub::\n"
            "servers => { \"$(sys.policy_hub)\" };\n"
            "}\n"
            "############################################\n"
            "body action u_background\n"
            "{\n"
            "background => \"true\";\n"
            "}\n"
            "############################################\n"
            "body depth_search u_recurse(d)\n"
            "{\n"
            "depth => \"$(d)\";\n"
            "exclude_dirs => { \"\\.svn\", \"\\.git\" };"
            "}\n"
            "############################################\n"
            "body service_method bootstart\n"
            "{\n"
            "service_autostart_policy => \"boot_time\";\n"
            "}\n"
            "############################################\n"
            "body action ifwin_bg\n"
            "{\n"
            "windows::\n"
            "background => \"true\";\n"
            "}\n"
            "############################################\n"
            "body copy_from u_cp(from)\n"
            "{\n"
            "source          => \"$(from)\";\n"
#ifdef __MINGW32__
            "compare         => \"digest\";\n" "copy_backup     => \"false\";\n" "}\n" "\n");
#else
            "compare         => \"digest\";\n" "copy_backup     => \"false\";\n" "}\n" "\n", CFWORKDIR, CFWORKDIR);
#endif /* !__MINGW32__ */
    fclose(fout);

    if (chmod(filename, S_IRUSR | S_IWUSR) == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed setting permissions on generated failsafe file '%s'", filename);
        return false;
    }

    return true;
}

bool WriteBuiltinFailsafePolicy(const char *workdir)
{
    char failsafe_path[CF_BUFSIZE];
    snprintf(failsafe_path, CF_BUFSIZE - 1, "%s/inputs/failsafe.cf", workdir);
    MapName(failsafe_path);

    return WriteBuiltinFailsafePolicyToPath(failsafe_path);
}
