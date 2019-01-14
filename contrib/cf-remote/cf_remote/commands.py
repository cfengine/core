from cf_remote.remote import get_info, print_info, install_host


def info(hosts, users=None):
    assert hosts
    for host in hosts:
        data = get_info(host, users)
        print_info(data)


def install(hub, clients, bootstrap=None, package=None):
    assert hub or clients
    if hub:
        install_host(hub, hub=True, package=package)
    for host in (clients or []):
        install_host(host, hub=False, package=package)
