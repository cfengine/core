import os

from cf_remote.remote import get_info, print_info, install_host
from cf_remote.packages import Releases
from cf_remote.web import download_package
from cf_remote.paths import cf_remote_dir
from cf_remote.utils import save_file
from cf_remote import log


def info(hosts, users=None):
    assert hosts
    for host in hosts:
        data = get_info(host, users)
        print_info(data)


def install(
        hub,
        clients,
        *,
        bootstrap=None,
        package=None,
        hub_package=None,
        client_package=None,
        version=None,
        demo=False):
    assert hub or clients
    assert not (hub and clients and package)
    # These assertions are checked in main.py

    if not hub_package:
        hub_package = package
    if not client_package:
        client_package = package
    if bootstrap:
        save_file(os.path.join(cf_remote_dir(), "policy_server.dat"), bootstrap + "\n")
    if hub:
        log.debug("Installing hub package on '{}'".format(hub))
        install_host(
            hub, hub=True, package=hub_package, bootstrap=bootstrap, version=version, demo=demo)
    for host in (clients or []):
        log.debug("Installing client package on '{}'".format(host))
        install_host(
            host,
            hub=False,
            package=client_package,
            bootstrap=bootstrap,
            version=version,
            demo=demo)
    if demo and hub:
        print(
            "Your demo hub is ready: https://{}/ (Username: admin, Password: password)".format(
                hub))


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
