import os
import sys

from cf_remote.remote import get_info, print_info, install_host, run_command, transfer_file
from cf_remote.packages import Releases
from cf_remote.web import download_package
from cf_remote.paths import cf_remote_dir
from cf_remote.utils import save_file, strip_user
from cf_remote import log


def info(hosts, users=None):
    assert hosts
    for host in hosts:
        data = get_info(host, users=users)
        print_info(data)


def run(hosts, command, users=None, sudo=False):
    assert hosts
    for host in hosts:
        lines = run_command(host=host, command=command, users=users, sudo=sudo)
        if lines is None:
            sys.exit("Command: '{}'\nFailed on host: '{}'".format(command, host))
        host_colon = (host + ":").ljust(16)
        if lines == "":
            print("{} '{}'".format(host_colon, command))
            continue
        cmd = command
        lines = lines.replace("\r", "")
        for line in lines.split("\n"):
            if cmd:
                print("{} '{}' -> '{}'".format(host_colon, cmd, line))
                fill = " " * (len(cmd) + 7)
                cmd = None
            else:
                print("{}{}'{}'".format(host_colon, fill, line))


def sudo(hosts, command, users=None):
    run(hosts, command, users, sudo=True)


def scp(hosts, files, users=None):
    for host in hosts:
        for file in files:
            transfer_file(host, file, users)


def install(
        hubs,
        clients,
        *,
        bootstrap=None,
        package=None,
        hub_package=None,
        client_package=None,
        version=None,
        demo=False,
        call_collect=False):
    assert hubs or clients
    assert not (hubs and clients and package)
    # These assertions are checked in main.py

    if not hub_package:
        hub_package = package
    if not client_package:
        client_package = package
    if bootstrap:
        if type(bootstrap) is str:
            bootstrap = [bootstrap]
        save_file(os.path.join(cf_remote_dir(), "policy_server.dat"), "\n".join(bootstrap + [""]))
    if hubs:
        if type(hubs) is str:
            hubs = [hubs]
        for index, hub in enumerate(hubs):
            log.debug("Installing hub package on '{}'".format(hub))
            install_host(
                hub,
                hub=True,
                package=hub_package,
                bootstrap=bootstrap[index % len(bootstrap)] if bootstrap else None,
                version=version,
                demo=demo,
                call_collect=call_collect)
    for index, host in enumerate(clients or []):
        log.debug("Installing client package on '{}'".format(host))
        install_host(
            host,
            hub=False,
            package=client_package,
            bootstrap=bootstrap[index % len(bootstrap)] if bootstrap else None,
            version=version,
            demo=demo)
    if demo and hubs:
        for hub in hubs:
            print(
                "Your demo hub is ready: https://{}/ (Username: admin, Password: password)".
                format(strip_user(hub)))


def packages(tags=None, version=None):
    releases = Releases()
    print("Available releases: {}".format(releases))

    release = releases.default
    if version:
        release = releases.pick_version(version)
    print("Using {}:".format(release))
    artifacts = release.find(tags)

    if len(artifacts) == 0:
        print("No suitable packages found")
    else:
        for artifact in artifacts:
            download_package(artifact.url)
