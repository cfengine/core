#!/usr/bin/env python3
import sys
import time
from os.path import basename
from collections import OrderedDict

from cf_remote.utils import os_release, column_print, pretty, user_error, parse_systeminfo, parse_version
from cf_remote.ssh import ssh_sudo, ssh_cmd, scp, auto_connect
from cf_remote import log
from cf_remote.web import download_package
from cf_remote.packages import Releases

import cf_remote.demo as demo_lib

def powershell(cmd):
    assert '"' not in cmd # TODO: How to escape in cmd / powershell
    # Note: Have to use double quotes, because single quotes are different
    #       in cmd
    return r'powershell.exe -Command "{}"'.format(cmd)

def print_info(data):
    output = OrderedDict()
    print()
    print(data["ssh"])
    os = like = None
    if "os_release" in data:
        os_release = data["os_release"]
        if os_release:
            if "ID" in os_release:
                os = os_release["ID"]
            if "ID_LIKE" in os_release:
                like = os_release["ID_LIKE"]
    elif "systeminfo" in data:
        os = "Windows"

    if not os:
        os = data["uname"]
    if os and like:
        output["OS"] = "{} ({})".format(os, like)
    elif os:
        output["OS"] = "{}".format(os)
    else:
        output["OS"] = "Unknown"

    if "arch" in data:
        output["Architecture"] = data["arch"]

    agent_version = data["agent_version"]
    if agent_version:
        output["CFEngine"] = agent_version
    else:
        output["CFEngine"] = "Not installed"

    output["Policy server"] = data.get("policy_server")

    binaries = []
    if "bin" in data:
        for key in data["bin"]:
            binaries.append(key)
    if binaries:
        output["Binaries"] = ", ".join(binaries)

    column_print(output)
    print()


def transfer_file(host, file, users=None, connection=None):
    assert not users or len(users) == 1
    if users:
        host = users[0] + "@" + host
    scp(file=file, remote=host, connection=connection)


@auto_connect
def run_command(host, command, *, users=None, connection=None, sudo=False):
    if sudo:
        return ssh_sudo(connection, command, errors=True)
    return ssh_cmd(connection, command, errors=True)


@auto_connect
def get_info(host, *, users=None, connection=None):
    log.debug("Getting info about '{}'".format(host))

    user, host = connection.ssh_user, connection.ssh_host
    data = OrderedDict()
    data["ssh_user"] = user
    data["ssh_host"] = host
    data["ssh"] = "{}@{}".format(user, host)
    data["whoami"] = ssh_cmd(connection, "whoami")
    systeminfo = ssh_cmd(connection, "systeminfo")
    if systeminfo:
        data["os"] = "windows"
        data["systeminfo"] = parse_systeminfo(systeminfo)
        data["package_tags"] = ["x86_64", "msi"]
        data["arch"] = "x86_64"
        agent = r'& "C:\Program Files\Cfengine\bin\cf-agent.exe"'
        data["agent"] = agent
        data["agent_version"] = parse_version(ssh_cmd(connection, '{} -V'.format(agent)))
    else:
        data["os"] = "unix"
        data["uname"] = ssh_cmd(connection, "uname")
        data["arch"] = ssh_cmd(connection, "uname -m")
        data["os_release"] = os_release(ssh_cmd(connection, "cat /etc/os-release"))

        tags = []
        if data["os_release"]:
            distro = data["os_release"]["ID"]
            major = data["os_release"]["VERSION_ID"].split(".")[0]
            platform_tag = distro + major

            # Add tags with version number first, to filter by them first:
            tags.append(platform_tag) # Example: ubuntu16
            if distro == "centos":
                tags.append("el" + major)

            # Then add more generic tags (lower priority):
            tags.append(distro) # Example: ubuntu
            if distro == "centos":
                tags.append("rhel")
                tags.append("el")
        else:
            redhat_release = ssh_cmd(connection, "cat /etc/redhat-release")
            if redhat_release:
                # Examples:
                # CentOS release 6.10 (Final)
                # Red Hat Enterprise Linux release 8.0 (Ootpa)
                before, after = redhat_release.split(" release ")
                distro = "rhel"
                if before.lower().startswith("centos"):
                    distro = "centos"
                major = after.split(".")[0]
                tags.append(distro + major)
                tags.append("el" + major)
                if "rhel" not in tags:
                    tags.append("rhel" + major)

                tags.append(distro)
                if "rhel" not in tags:
                    tags.append("rhel")
                tags.append("el")

        data["package_tags"] = tags

        data["agent_location"] = ssh_cmd(connection, "which cf-agent")
        data["policy_server"] = ssh_cmd(connection, "cat /var/cfengine/policy_server.dat")

        agent = r'/var/cfengine/bin/cf-agent'
        data["agent"] = agent
        data["agent_version"] = parse_version(ssh_cmd(connection, "{} --version".format(agent)))

        data["bin"] = {}
        for bin in ["dpkg", "rpm", "yum", "apt", "pkg"]:
            path = ssh_cmd(connection, "which {}".format(bin))
            if path:
                data["bin"][bin] = path

    log.debug("JSON data from host info: \n" + pretty(data))
    return data


@auto_connect
def install_package(host, pkg, data, *, connection=None):

    print("Installing: '{}' on '{}'".format(pkg, host))
    if ".deb" in pkg:
        output = ssh_sudo(connection, "dpkg -i {}".format(pkg), True)
    elif ".msi" in pkg:
        # Windows is crazy, be careful if you decide to change this;
        # This needs to work in both powershell and cmd, and in
        # Windows 2012 Server, 2016, and so on...
        # sleep is powershell specific,
        # timeout doesn't work over ssh.
        output = ssh_cmd(connection, powershell(r'.\{} ; sleep 10'.format(pkg)), True)
    else:
        output = ssh_sudo(connection, "rpm -i {}".format(pkg), True)
    if output is None:
        sys.exit("Installation failed on '{}'".format(host))


@auto_connect
def uninstall_cfengine(host, data, *, connection=None):
    print("Uninstalling CFEngine on '{}'".format(host))

    if "dpkg" in data["bin"]:
        run_command(host, "dpkg --remove cfengine-community || true", connection=connection, sudo=True)
        run_command(host, "dpkg --remove cfengine-nova || true", connection=connection, sudo=True)
        run_command(host, "dpkg --remove cfengine-nova-hub || true", connection=connection, sudo=True)
    elif "rpm" in data["bin"]:
        run_command(host, "rpm --erase cfengine-community || true", connection=connection, sudo=True)
        run_command(host, "rpm --erase cfengine-nova || true", connection=connection, sudo=True)
        run_command(host, "rpm --erase cfengine-nova-hub || true", connection=connection, sudo=True)
    else:
        user_error("I don't know how to uninstall there!")

    run_command(host, "pkill -U cfapache || true", connection=connection, sudo=True)
    run_command(host, "rm -rf /var/cfengine /opt/cfengine", connection=connection, sudo=True)


@auto_connect
def bootstrap_host(host_data, policy_server, *, connection=None):
    host = host_data["ssh_host"]
    agent = host_data["agent"]
    print("Bootstrapping: '{}' -> '{}'".format(host, policy_server))
    command = "{} --bootstrap {}".format(agent, policy_server)
    if host_data["os"] == "windows":
        output = ssh_cmd(connection, command)
    else:
        output = ssh_sudo(connection, command)

    if output is None:
        sys.exit("Bootstrap failed on '{}'".format(host))
    if output and "completed successfully" in output:
        print("Bootstrap successful: '{}' -> '{}'".format(host, policy_server))
    else:
        user_error("Something went wrong while bootstrapping")


@auto_connect
def install_host(
        host,
        *,
        hub=False,
        package=None,
        bootstrap=None,
        version=None,
        demo=False,
        call_collect=False,
        connection=None,
        edition=None):
    data = get_info(host, connection=connection)
    print_info(data)

    if not package:
        tags = []
        if edition == "enterprise":
            tags.append("hub" if hub else "agent")
        tags.append("64" if data["arch"] in ["x86_64", "amd64"] else data["arch"])
        extension = None
        if "package_tags" in data and "msi" in data["package_tags"]:
            extension = ".msi"
            data["package_tags"].remove("msi")
        elif "dpkg" in data["bin"]:
            extension = ".deb"
        elif "rpm" in data["bin"]:
            extension = ".rpm"
        releases = Releases(edition)
        release = releases.default
        if version:
            release = releases.pick_version(version)

        if "package_tags" in data and data["package_tags"]:
            tags.extend(data["package_tags"])

        artifacts = release.find(tags, extension)
        if not artifacts:
            user_error(
                "Could not find an appropriate package for host, please use --{}-package".format(
                    "hub" if hub else "client"))
        artifact = artifacts[-1]
        package = download_package(artifact.url)

    scp(package, host, connection=connection)
    package = basename(package)
    install_package(host, package, data, connection=connection)
    data = get_info(host, connection=connection)
    if data["agent_version"] and len(data["agent_version"]) > 0:
        print(
            "CFEngine {} was successfully installed on '{}'".format(data["agent_version"],
                                                                    host))
    else:
        print("Installation failed!")
        sys.exit(1)
    if bootstrap:
        bootstrap_host(data, policy_server=bootstrap, connection=connection)
    if demo:
        if hub:
            demo_lib.install_def_json(host, connection=connection, call_collect=call_collect)
            demo_lib.agent_run(data, connection=connection)
            demo_lib.disable_password_dialog(host)
        demo_lib.agent_run(data, connection=connection)


@auto_connect
def uninstall_host(host, *, connection=None):
    data = get_info(host, connection=connection)
    print_info(data)

    if not data["agent_version"]:
        log.warning("CFEngine does not seem to be installed on '{}' - attempting uninstall anyway".format(host))

    uninstall_cfengine(host, data, connection=connection)
    data = get_info(host, connection=connection)

    if (not data) or data["agent_version"]:
        print("Failed to uninstall CFEngine on '{}'".format(host))
        return None

    print_info(data)

    print("Uninstallation successful on '{}'".format(host))
    return data
