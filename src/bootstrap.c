
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
#include "vars.h"
#include "files_interfaces.h"
#include "cfstream.h"

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

static void CreateFailSafe(char *name);

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

void CheckAutoBootstrap()
{
    struct stat sb;
    char name[CF_BUFSIZE];
    int repaired = false, have_policy = false, am_appliance = false;

    CfOut(cf_cmdout, "", "** CFEngine BOOTSTRAP probe initiated");

    PrintVersionBanner("CFEngine");
    printf("\n");

    printf(" -> This host is: %s\n", VSYSNAME.nodename);
    printf(" -> Operating System Type is %s\n", VSYSNAME.sysname);
    printf(" -> Operating System Release is %s\n", VSYSNAME.release);
    printf(" -> Architecture = %s\n", VSYSNAME.machine);
    printf(" -> Internal soft-class is %s\n", CLASSTEXT[VSYSTEMHARDCLASS]);

    if (!BootstrapAllowed())
    {
        FatalError(" !! Not enough privileges to bootstrap CFEngine");
    }

    snprintf(name, CF_BUFSIZE - 1, "%s/inputs/failsafe.cf", CFWORKDIR);
    MapName(name);

    if (cfstat(name, &sb) == -1)
    {
        CreateFailSafe(name);
        repaired = true;
    }

    snprintf(name, CF_BUFSIZE - 1, "%s/inputs/promises.cf", CFWORKDIR);
    MapName(name);

    if (cfstat(name, &sb) == -1)
    {
        CfOut(cf_cmdout, "", " -> No previous policy has been cached on this host");
    }
    else
    {
        CfOut(cf_cmdout, "", " -> An existing policy was cached on this host in %s/inputs", CFWORKDIR);
        have_policy = true;
    }

    if (strlen(POLICY_SERVER) > 0)
    {
        CfOut(cf_cmdout, "", " -> Assuming the policy distribution point at: %s:/var/cfengine/masterfiles\n",
              POLICY_SERVER);
    }
    else
    {
        if (have_policy)
        {
            CfOut(cf_cmdout, "",
                  " -> No policy distribution host was discovered - it might be contained in the existing policy, otherwise this will function autonomously\n");
        }
        else if (repaired)
        {
            CfOut(cf_cmdout, "", " -> No policy distribution host was defined - use --policy-server to set one\n");
        }
    }

    printf(" -> Attempting to initiate promised autonomous services...\n\n");

    am_appliance = IsDefinedClass(CanonifyName(POLICY_SERVER), NULL);
    snprintf(name, CF_MAXVARSIZE, "ipv4_%s", CanonifyName(POLICY_SERVER));
    am_appliance |= IsDefinedClass(name, NULL);

    if (strlen(POLICY_SERVER) == 0)
    {
        am_appliance = false;
    }

    snprintf(name, sizeof(name), "%s/state/am_policy_hub", CFWORKDIR);
    MapName(name);

    if (am_appliance)
    {
        HardClass("am_policy_hub");
        printf
            (" ** This host recognizes itself as a CFEngine Policy Hub, with policy distribution and knowledge base.\n");
        printf
            (" -> The system is now converging. Full initialisation and self-analysis could take up to 30 minutes\n\n");
        creat(name, 0600);
    }
    else
    {
        unlink(name);
    }
}

/********************************************************************/

void SetPolicyServer(char *name)
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
        fscanf(fin, "%1023s", fileContents);
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
            CfOut(cf_error, "fopen", "Unable to write policy server file! (%s)", file);
            return;
        }

        fprintf(fout, "%s", name);
        fclose(fout);
    }

    if (NULL_OR_EMPTY(name))
    {
        // avoids "Scalar item in servers => {  } in rvalue is out of bounds ..."
        // when NovaBase is checked with unprivileged (not bootstrapped) cf-promises 
        NewScalar("sys", "policy_hub", "undefined", cf_str);
    }
    else
    {
        NewScalar("sys", "policy_hub", name, cf_str);
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
    
    NewScalar("sys", "last_policy_update", timebuf, cf_str);
}

/********************************************************************/

static void CreateFailSafe(char *name)
{
    FILE *fout;

    if ((fout = fopen(name, "w")) == NULL)
    {
        CfOut(cf_error, "fopen", "Unable to write failsafe file! (%s)", name);
        return;
    }

    CfOut(cf_cmdout, "", " -> No policy failsafe discovered, assume temporary bootstrap vector\n");

    fprintf(fout,
            "body common control\n"
            "{\n"
            "bundlesequence => { \"cfe_internal_update\" };\n"
            "}\n\n"
            "body agent control\n"
            "{\n"
            "skipidentify => \"true\";\n"
            "}\n\n"
            "bundle agent cfe_internal_update\n"
            "{\n"
            "classes:\n"
            "  \"have_ppkeys\" expression => fileexists(\"$(sys.workdir)/ppkeys/localhost.pub\"),\n"
            "     handle => \"cfe_internal_bootstrap_update_classes_have_ppkeys\";\n"
            "\ncommands:\n"
            " !have_ppkeys::\n"
            "   \"$(sys.cf_key)\",\n"
            "      handle => \"cfe_internal_bootstrap_update_commands_generate_keys\";\n"
            "\nfiles:\n"
            " !windows::\n"
            "  \"$(sys.workdir)/inputs\" \n"
            "    handle => \"cfe_internal_bootstrap_update_files_sys_workdir_inputs_not_windows\",\n"
            "    copy_from => u_scp(\"/var/cfengine/masterfiles\"),\n"
            "    depth_search => u_recurse(\"inf\"),\n"
            "    classes => success(\"got_policy\");\n"
            "\n"
            "  windows::\n"
            "  \"$(sys.workdir)\\inputs\" \n"
            "    handle => \"cfe_internal_bootstrap_update_files_sys_workdir_inputs_windows\",\n"
            "    copy_from => u_scp(\"/var/cfengine/masterfiles\"),\n"
            "    depth_search => u_recurse(\"inf\"),\n"
            "    classes => success(\"got_policy\");\n\n"
            "\n"
            "     \"$(sys.workdir)\\bin-twin\\.\"\n"
            "          handle => \"cfe_internal_bootstrap_update_files_sys_workdir_bin_twin_windows\",\n"
            "         comment => \"Make sure we maintain a clone of the binaries and libraries for updating\",\n"
            "       copy_from => u_cp(\"$(sys.workdir)\\bin\\.\"),\n"
            "    depth_search => u_recurse(\"1\");\n"
            "\n"
            "\n"
            "\nprocesses:\n"
            "!windows.got_policy::\n"
            "\"cf-execd\" restart_class => \"start_exec\",\n"
            "                    handle => \"cfe_internal_bootstrap_update_processes_start_cf_execd\";\n"
            "am_policy_hub.got_policy::\n"
            "\"cf-serverd\" restart_class => \"start_server\",\n"
            "                      handle => \"cfe_internal_bootstrap_update_processes_start_cf_serverd\";\n\n"
            "\ncommands:\n"
            "start_exec.!windows::\n"
            "\"$(sys.cf_execd)\","
            " handle => \"cfe_internal_bootstrap_update_commands_check_sys_cf_execd_start\",\n"
            "classes => outcome(\"executor\");\n"
            "start_server::\n"
            "\"$(sys.cf_serverd)\"\n"
            "handle => \"cfe_internal_bootstrap_update_commands_check_sys_cf_serverd_start\",\n"
            "action => ifwin_bg,\n"
            "classes => outcome(\"server\");\n\n"
            "\nservices:\n"
            "windows.got_policy::\n"
            "\"CfengineNovaExec\"\n"
            "           handle => \"cfe_internal_bootstrap_update_services_windows_executor\",\n"
            "   service_policy => \"start\",\n"
            "   service_method => bootstart,\n"
            "   classes => outcome(\"executor\");\n\n"
            "reports:\n"
            "  bootstrap_mode.am_policy_hub::\n"
            "      \"This host assumes the role of policy distribution host\",\n"
            "           handle => \"cfe_internal_bootstrap_update_reports_assume_policy_hub\";\n"
            "  bootstrap_mode.!am_policy_hub::\n"
            "      \"This autonomous node assumes the role of voluntary client\",\n"
            "           handle => \"cfe_internal_bootstrap_update_reports_assume_voluntary_client\";\n"
            "  got_policy::      \" -> Updated local policy from policy server\",\n"
            "           handle => \"cfe_internal_bootstrap_update_reports_got_policy\";\n"
            " !got_policy::      \" !! Failed to pull policy from policy server\",\n"
            "           handle => \"cfe_internal_bootstrap_update_reports_did_not_get_policy\";\n"
            "  server_ok::      \" -> Started the server\",\n"
            "           handle => \"cfe_internal_bootstrap_update_reports_started_serverd\";\n"
            " am_policy_hub.!server_ok::      \" !! Failed to start the server\",\n"
            "           handle => \"cfe_internal_bootstrap_update_reports_failed_to_start_serverd\";\n"
            "  executor_ok::      \" -> Started the scheduler\",\n"
            "           handle => \"cfe_internal_bootstrap_update_reports_started_execd\";\n"
            " !executor_ok::      \" !! Did not start the scheduler\",\n"
            "           handle => \"cfe_internal_bootstrap_update_reports_failed_to_start_execd\";\n"
            "}\n"
            "############################################\n"
            "body classes outcome(x)\n"
            "{\n"
            "promise_repaired => {\"$(x)_ok\"};\n"
            "}\n"
            "############################################\n"
            "body classes success(x)\n"
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
            "compare         => \"digest\";\n" "copy_backup     => \"false\";\n" "}\n" "\n");

    fclose(fout);

    if (cf_chmod(name, S_IRUSR | S_IWUSR) == -1)
    {
        CfOut(cf_error, "cf_chmod", "!! Failed setting permissions on bootstrap policy (%s)", name);
    }
}
