
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

#include "bootstrap.h"

#include "env_context.h"
#include "files_names.h"
#include "scope.h"
#include "files_interfaces.h"
#include "logging.h"
#include "exec_tools.h"
#include "generic_agent.h" // PrintVersionBanner
#include "audit.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

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

static bool BootstrapAllowed(void)
{
    return true;
}

#elif !defined(__MINGW32__)

static bool BootstrapAllowed(void)
{
    return IsPrivileged();
}

#endif

/*****************************************************************************/

void CheckAutoBootstrap(EvalContext *ctx)
{
    struct stat sb;
    char name[CF_BUFSIZE];
    int have_policy = false, am_appliance = false;

    CfOut(OUTPUT_LEVEL_CMDOUT, "", "** CFEngine BOOTSTRAP probe initiated");

    PrintVersion();
    printf("\n");

    printf(" -> This host is: %s\n", VSYSNAME.nodename);
    printf(" -> Operating System Type is %s\n", VSYSNAME.sysname);
    printf(" -> Operating System Release is %s\n", VSYSNAME.release);
    printf(" -> Architecture = %s\n", VSYSNAME.machine);
    printf(" -> Internal soft-class is %s\n", CLASSTEXT[VSYSTEMHARDCLASS]);

    if (!BootstrapAllowed())
    {
        FatalError(ctx, " !! Not enough privileges to bootstrap CFEngine");
    }

    snprintf(name, CF_BUFSIZE - 1, "%s/inputs/failsafe.cf", CFWORKDIR);
    MapName(name);

    CreateFailSafe(name);

    snprintf(name, CF_BUFSIZE - 1, "%s/inputs/promises.cf", CFWORKDIR);
    MapName(name);

    if (cfstat(name, &sb) == -1)
    {
        CfOut(OUTPUT_LEVEL_CMDOUT, "", " -> No previous policy has been cached on this host");
    }
    else
    {
        CfOut(OUTPUT_LEVEL_CMDOUT, "", " -> An existing policy was cached on this host in %s/inputs", CFWORKDIR);
        have_policy = true;
    }

    if (strlen(POLICY_SERVER) > 0)
    {
        CfOut(OUTPUT_LEVEL_CMDOUT, "", " -> Assuming the policy distribution point at: %s:%s/masterfiles\n", CFWORKDIR,
              POLICY_SERVER);
    }
    else
    {
        if (have_policy)
        {
            CfOut(OUTPUT_LEVEL_CMDOUT, "",
                  " -> No policy distribution host was discovered - it might be contained in the existing policy, otherwise this will function autonomously\n");
        }
        else
        {
            CfOut(OUTPUT_LEVEL_CMDOUT, "", " -> No policy distribution host was defined - use --policy-server to set one\n");
        }
    }

    printf(" -> Attempting to initiate promised autonomous services...\n\n");

    am_appliance = IsDefinedClass(ctx, CanonifyName(POLICY_SERVER), NULL);
    snprintf(name, CF_MAXVARSIZE, "ipv4_%s", CanonifyName(POLICY_SERVER));
    am_appliance |= IsDefinedClass(ctx, name, NULL);

    if (strlen(POLICY_SERVER) == 0)
    {
        am_appliance = false;
    }

    snprintf(name, sizeof(name), "%s/state/am_policy_hub", CFWORKDIR);
    MapName(name);

    if (am_appliance)
    {
        EvalContextHeapAddHard(ctx, "am_policy_hub");
        printf
            (" ** This host recognizes itself as a CFEngine policy server, with policy distribution from %s/masterfiles.\n", WORKDIR);
        creat(name, 0600);
    }
    else
    {
        unlink(name);
    }

}

/********************************************************************/

void SetPolicyServer(EvalContext *ctx, char *name)
/* 
 * If name contains a string, it's written to file,
 * if not, name is filled with the contents of file.
 */
{
    char file[CF_BUFSIZE];
    FILE *fout, *fin;
    char fileContents[CF_MAXVARSIZE] = { 0 };

    snprintf(file, CF_BUFSIZE - 1, "%s/policy_server.dat", CFWORKDIR);
    MapName(file);

    if ((fin = fopen(file, "r")) != NULL)
    {
        if (fscanf(fin, "%1023s", fileContents) != 1)
        {
            CfDebug("Couldn't read string from policy_server.dat");
        }
        fclose(fin);
    }

    // update file if different and we know what to put there

    if ((NULL_OR_EMPTY(name)) && (!NULL_OR_EMPTY(fileContents)))
    {
        snprintf(name, CF_MAXVARSIZE, "%s", fileContents);
    }
    else if ((!NULL_OR_EMPTY(name)) && (strcmp(name, fileContents) != 0))
    {
        if ((fout = fopen(file, "w")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Unable to write policy server file! (%s)", file);
            return;
        }

        fprintf(fout, "%s", name);
        fclose(fout);
    }

    if (NULL_OR_EMPTY(name))
    {
        // avoids "Scalar item in servers => {  } in rvalue is out of bounds ..."
        // when NovaBase is checked with unprivileged (not bootstrapped) cf-promises 
        ScopeNewSpecialScalar(ctx, "sys", "policy_hub", "undefined", DATA_TYPE_STRING);
    }
    else
    {
        ScopeNewSpecialScalar(ctx, "sys", "policy_hub", name, DATA_TYPE_STRING);
    }

// Get the timestamp on policy update

    snprintf(file, CF_MAXVARSIZE, "%s/masterfiles/cf_promises_validated", CFWORKDIR);
    MapName(file);

    struct stat sb;
    
    if ((cfstat(file, &sb)) != 0)
    {
        return;
    }
    
    char timebuf[26];
    cf_strtimestamp_local(sb.st_mtime, timebuf);
    
    ScopeNewSpecialScalar(ctx, "sys", "last_policy_update", timebuf, DATA_TYPE_STRING);
}

char *GetPolicyServer(const char *workdir)
{
    char path[CF_BUFSIZE] = { 0 };
    snprintf(path, sizeof(path), "%s%cpolicy_server.dat", workdir, FILE_SEPARATOR);
    char contents[CF_BUFSIZE] = { 0 };

    FILE *fp = fopen(path, "r");
    if (fp)
    {
        if (fscanf(fp, "%4095s", contents) != 1)
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

bool GetAmPolicyServer(const char *workdir)
{
    char path[CF_BUFSIZE] = { 0 };
    snprintf(path, sizeof(path), "%s/state/am_policy_hub", workdir);
    MapName(path);

    struct stat sb;
    return cfstat(path, &sb) == 0;
}

/********************************************************************/

void CreateFailSafe(char *name)
{
    FILE *fout;

    if ((fout = fopen(name, "w")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Unable to write failsafe file! (%s)", name);
        return;
    }

    CfOut(OUTPUT_LEVEL_CMDOUT, "", " -> No policy failsafe discovered, assume temporary bootstrap vector\n");

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
            "  !have_ppkeys::\n"
            "   \"$(sys.cf_key)\"\n"
            "      handle => \"cfe_internal_bootstrap_update_commands_generate_keys\";\n"
            "\n#\n\n"
            " files:\n\n"
            "  !windows::\n"
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
            "  windows::\n"
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
            "  bootstrap_mode.am_policy_hub::\n"
            "   \"This host assumes the role of policy distribution host\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_assume_policy_hub\";\n"
            "  bootstrap_mode.!am_policy_hub::\n"
            "   \"This autonomous node assumes the role of voluntary client\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_assume_voluntary_client\";\n"
            "  got_policy::\n"
            "   \" -> Updated local policy from policy server\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_got_policy\";\n"
            "  !got_policy.!have_promises_cf::\n"
            "   \" !! Failed to copy policy from policy server at $(sys.policy_hub):/var/cfengine/masterfiles\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_did_not_get_policy\";\n"
            "  server_started::\n"
            "   \" -> Started the server\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_started_serverd\";\n"
            "  am_policy_hub.!server_started.!have_promises_cf::\n"
            "   \" !! Failed to start the server\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_failed_to_start_serverd\";\n"
            "  executor_started::\n"
            "   \" -> Started the scheduler\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_started_execd\";\n"
            "  !executor_started.!have_promises_cf::\n"
            "   \" !! Did not start the scheduler\"\n"
            "      handle => \"cfe_internal_bootstrap_update_reports_failed_to_start_execd\";\n"
            "  !executor_started.have_promises_cf::\n"
            "   \" -> You are running a hard-coded failsafe. Please use the following command instead.\n"
            "    - 3.0.0: $(sys.cf_agent) -f $(sys.workdir)/inputs/failsafe/failsafe.cf\n"
            "    - 3.0.1: $(sys.cf_agent) -f $(sys.workdir)/inputs/update.cf\"\n"
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

    if (cf_chmod(name, S_IRUSR | S_IWUSR) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "cf_chmod", "!! Failed setting permissions on bootstrap policy (%s)", name);
    }
}
