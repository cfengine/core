#!/usr/bin/env python3
import sys
from os.path import basename
from collections import OrderedDict

import fabric
from paramiko.ssh_exception import AuthenticationException
from invoke.exceptions import UnexpectedExit

from cf_remote.utils import os_release, column_print, pretty, user_error
from cf_remote import log
from cf_remote.web import download_package
from cf_remote.packages import Releases

import cf_remote.demo as demo_lib


def ssh_cmd(connection, cmd, errors=False):
    try:
        log.debug("Running over SSH: '{}'".format(cmd))
        result = connection.run(cmd, hide=True)
        output = result.stdout.strip()
        log.debug("'{}' -> '{}'".format(cmd, output))
        return output
    except UnexpectedExit as e:
        msg = "Non-sudo command unexpectedly exited: '{}'".format(cmd)
        if errors:
            print(e)
            log.error(msg)
        else:
            log.debug(msg)
        return None


def ssh_sudo(connection, cmd, errors=False):
    try:
        log.debug("Running(sudo) over SSH: '{}'".format(cmd))
        result = connection.sudo(cmd, hide=True)
        output = result.stdout.strip()
        log.debug("'{}' -> '{}'".format(cmd, output))
        return output
    except UnexpectedExit as e:
        msg = "Sudo command unexpectedly exited: '{}'".format(cmd)
        if errors:
            print(e)
            log.error(msg)
        else:
            log.debug(msg)
        return None


def print_info(data):
    output = OrderedDict()
    print()
    print(data["ssh"])
    os_release = data["os_release"]
    os = like = None
    if os_release:
        if "ID" in os_release:
            os = os_release["ID"]
        if "ID_LIKE" in os_release:
            like = os_release["ID_LIKE"]
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


def connect(host, users=None):
    log.debug("Connecting to '{}'".format(host))
    if "@" in host:
        parts = host.split("@")
        assert len(parts) == 2
        host = parts[1]
        if not users:
            users = [parts[0]]
    if not users:
        users = ["ubuntu", "ec2-user", "centos", "vagrant", "root"]
    for user in users:
        try:
            c = fabric.Connection(host=host, user=user)
            c.ssh_user = user
            c.ssh_host = host
            c.run("whoami", hide=True)
            return c
        except AuthenticationException:
            continue
    sys.exit("Could not ssh into '{}'".format(host))


def transfer_file(host, file, users=None, connection=None):
    assert not users or len(users) == 1
    if users:
        host = users[0] + "@" + host
    scp(file=file, remote=host, connection=connection)


def run_command(host, command, users=None, connection=None, sudo=False):
    if not connection:
        with connect(host, users) as c:
            return run_command(host, command, users, c, sudo)

    if sudo:
        return ssh_sudo(connection, command, errors=True)
    return ssh_cmd(connection, command, errors=True)


def get_info(host, users=None, connection=None):
    if not connection:
        with connect(host, users) as c:
            return get_info(host, users, c)

    log.debug("Getting info about '{}'".format(host))

    user, host = connection.ssh_user, connection.ssh_host
    data = OrderedDict()
    data["ssh_user"] = user
    data["ssh_host"] = host
    data["ssh"] = "{}@{}".format(user, host)
    data["whoami"] = ssh_cmd(connection, "whoami")
    data["uname"] = ssh_cmd(connection, "uname")
    data["arch"] = ssh_cmd(connection, "uname -m")
    data["os_release"] = os_release(ssh_cmd(connection, "cat /etc/os-release"))
    data["agent_location"] = ssh_cmd(connection, "which cf-agent")
    data["policy_server"] = ssh_cmd(connection, "cat /var/cfengine/policy_server.dat")
    agent_version = ssh_cmd(connection, "cf-agent --version")
    if agent_version:
        # 'CFEngine Core 3.12.1 \n CFEngine Enterprise 3.12.1'
        #                ^ split and use this part for version number
        agent_version = agent_version.split()[2]
    data["agent_version"] = agent_version
    data["bin"] = {}
    for bin in ["dpkg", "rpm", "yum", "apt", "pkg"]:
        path = ssh_cmd(connection, "which {}".format(bin))
        if path:
            data["bin"][bin] = path

    log.debug("JSON data from host info: \n" + pretty(data))
    return data


def scp(file, remote, connection=None):
    if not connection:
        with connect(remote) as connection:
            scp(file, remote, connection)
    else:
        print("Copying: '{}' to '{}'".format(file, remote))
        connection.put(file)


def install_package(host, pkg, data):
    print("Installing: '{}' on '{}'".format(pkg, host))
    with connect(host) as connection:
        if ".deb" in pkg:
            ssh_sudo(connection, "dpkg -i {}".format(pkg))
        else:
            ssh_sudo(connection, "rpm -i {}".format(pkg))


def boootstrap_host(host, policy_server):
    print("Bootstrapping: '{}' -> '{}'".format(host, policy_server))
    with connect(host) as connection:
        command = "/var/cfengine/bin/cf-agent --bootstrap {}".format(policy_server)
        output = ssh_sudo(connection, command)
        if output and "completed successfully" in output:
            print("Bootstrap succesful: '{}' -> '{}'".format(host, policy_server))
        else:
            user_error("Something went wrong while bootstrapping")


def install_host(host, *, hub=False, package=None, bootstrap=None, version=None, demo=False):
    data = get_info(host)
    print_info(data)

    if not package:
        tags = []
        tags.append("hub" if hub else "agent")
        tags.append("64" if data["arch"] in ["x86_64", "amd64"] else data["arch"])
        extension = None
        if "dpkg" in data["bin"]:
            extension = ".deb"
        elif "rpm" in data["bin"]:
            extension = ".rpm"
        releases = Releases()
        release = releases.default
        if version:
            release = releases.pick_version(version)
        artifacts = release.find(tags, extension)
        if not artifacts:
            user_error(
                "Could not find an appropriate package for host, please use --{}-package".format(
                    "hub" if hub else "client"))
        artifact = artifacts[-1]
        package = download_package(artifact.url)

    scp(package, host)
    package = basename(package)
    install_package(host, package, data)
    data = get_info(host)
    if data["agent_version"] and len(data["agent_version"]) > 0:
        print(
            "CFEngine {} was successfully installed on '{}'".format(data["agent_version"], host))
    else:
        print("Installation failed!")
        sys.exit(1)
    if bootstrap:
        boootstrap_host(host, policy_server=bootstrap)
    if demo:
        if hub:
            demo_lib.install_def_json(host)
            demo_lib.agent_run(host)
            demo_lib.disable_password_dialog(host)
        demo_lib.agent_run(host)
