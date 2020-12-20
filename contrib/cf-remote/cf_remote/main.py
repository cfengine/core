import argparse
import os
import sys

from cf_remote import log
from cf_remote import commands, paths
from cf_remote.utils import user_error, exit_success, expand_list_from_file, is_file_string
from cf_remote.utils import strip_user, read_json, is_package_url
from cf_remote.packages import Releases
from cf_remote.spawn import Providers


def print_version_info():
    print("cf-remote version 0.1 (BETA)")
    print("Available CFEngine versions:")
    releases = Releases()
    print(releases)


def get_args():
    ap = argparse.ArgumentParser(
        description="Spooky CFEngine at a distance",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    ap.add_argument("--log-level", help="Specify level of logging: DEBUG, INFO, WARNING, ERROR, or CRITICAL", type=str, default="WARNING")
    ap.add_argument(
        "--version", "-V", help="Print or specify version", nargs="?", type=str, const=True)

    command_help_hint = "Commands (use %s COMMAND --help to get more info)" % os.path.basename(sys.argv[0])
    subp = ap.add_subparsers(dest="command",
                             title=command_help_hint)

    sp = subp.add_parser("info", help="Get info about the given hosts")
    sp.add_argument("--hosts", "-H", help="Which hosts to get info for", type=str, required=True)

    sp = subp.add_parser("install", help="Install CFEngine on the given hosts")
    sp.add_argument("--edition", "-E", choices=["community", "enterprise"],
                    help="Enterprise or community packages", type=str)
    sp.add_argument("--package", help="Local path to package or URL to download", type=str)
    sp.add_argument("--hub-package", help="Local path to package or URL to download for --hub", type=str)
    sp.add_argument("--client-package", help="Local path to package or URL to download for --clients", type=str)
    sp.add_argument("--bootstrap", "-B", help="cf-agent --bootstrap argument", type=str)
    sp.add_argument("--clients", "-c", help="Where to install client package", type=str)
    sp.add_argument("--hub", help="Where to install hub package", type=str)
    sp.add_argument(
        "--demo", help="Use defaults to make demos smoother (NOT secure)", action='store_true')
    sp.add_argument(
        "--call-collect", help="Enable call collect in --demo def.json", action='store_true')
    sp.add_argument("--remote-download", help="Package will be downloaded directly to the target machine", action="store_true")
    sp.add_argument("--trust-keys", help="Comma-separated list of paths to keys hosts should trust" +
                    " (implies '--trust-server no' when boostraping)", type=str)

    sp = subp.add_parser("uninstall", help="Install CFEngine on the given hosts")
    sp.add_argument("--clients", "-c", help="Where to uninstall", type=str)
    sp.add_argument("--hub", help="Where to uninstall", type=str)
    sp.add_argument("--hosts", "-H", help="Where to uninstall", type=str)

    sp = subp.add_parser("packages", help="Get info about available packages")
    sp.add_argument("--edition", "-E", choices=["community", "enterprise"],
                    help="Enterprise or community packages", type=str)
    sp.add_argument("tags", metavar="TAG", nargs="*")

    sp = subp.add_parser("list", help="List CFEngine packages available for download")
    sp.add_argument("--edition", "-E", choices=["community", "enterprise"],
                    help="Enterprise or community packages", type=str)
    sp.add_argument("tags", metavar="TAG", nargs="*")

    sp = subp.add_parser("download", help="Download CFEngine packages")
    sp.add_argument("--edition", "-E", choices=["community", "enterprise"],
                    help="Enterprise or community packages", type=str)
    sp.add_argument("tags", metavar="TAG", nargs="*")

    sp = subp.add_parser("run", help="Run the command given as arguments on the given hosts")
    sp.add_argument("--hosts", "-H", help="Which hosts to run the command on", type=str, required=True)
    sp.add_argument("--raw", help="Print only output of command itself", action='store_true')
    sp.add_argument("remote_command", help="Command to execute on remote host (including args)", type=str, nargs=1)

    sp = subp.add_parser("sudo",
                         help="Run the command given as arguments on the given hosts with 'sudo'")
    sp.add_argument("--hosts", "-H", help="Which hosts to run the command on", type=str, required=True)
    sp.add_argument("--raw", help="Print only output of command itself", action='store_true')
    sp.add_argument("remote_command", help="Command to execute on remote host (including args)", type=str, nargs=1)

    sp = subp.add_parser("scp", help="Copy the given file to the given hosts")
    sp.add_argument("--hosts", "-H", help="Which hosts to copy the file to", type=str, required=True)
    sp.add_argument("args", help="Arguments", type=str, nargs='*')

    sp = subp.add_parser("spawn", help="Spawn hosts in the clouds")
    sp.add_argument("--list-platforms", help="List supported platforms", action='store_true')
    sp.add_argument("--init-config", help="Initialize configuration file for spawn functionality",
                    action='store_true')
    sp.add_argument("--platform", help="Platform to use", type=str)
    sp.add_argument("--count", help="How many hosts to spawn", type=int)
    sp.add_argument("--role", help="Role of the hosts", choices=["hub", "hubs", "client", "clients"])
    sp.add_argument("--name", help="Name of the group of hosts (can be used in other commands)")
    sp.add_argument("--append", help="Append the new VMs to a pre-existing group", action='store_true')
    sp.add_argument("--aws", help="Spawn VMs in AWS (default)", action='store_true')
    sp.add_argument("--gcp", help="Spawn VMs in GCP", action='store_true')
    sp.add_argument("--size", help="Size/type of the instances", type=str)
    sp.add_argument("--network", help="network/subnet to assign the VMs to (GCP only)", type=str)
    sp.add_argument("--no-public-ip",
                    help="No public IP needed (GCP only; WARNING: The VMs will only be accessible" +
                         " from some other VM in the same cloud/network!)",
                    action="store_true")
    # TODO: --region (optional)

    dp = subp.add_parser("destroy", help="Destroy hosts spawned in the clouds")
    dp.add_argument("--all", help="Destroy all hosts spawned in the clouds", action='store_true')
    dp.add_argument("name", help="Name fo the group of hosts to destroy", nargs='?')

    sp = subp.add_parser("deploy", help="Deploy masterfiles directory to hub")
    sp.add_argument("--hub", help="Hub(s) to deploy to", type=str, required=True)
    sp.add_argument("directory", help="Path to local masterfiles directory", type=str)

    args = ap.parse_args()
    return args


def run_command_with_args(command, args):
    if command == "info":
        return commands.info(args.hosts, None)
    elif command == "install":
        if args.trust_keys:
            trust_keys = args.trust_keys.split(",")
        else:
            trust_keys = None

        return commands.install(
            args.hub,
            args.clients,
            package=args.package,
            bootstrap=args.bootstrap,
            hub_package=args.hub_package,
            client_package=args.client_package,
            version=args.version,
            demo=args.demo,
            call_collect=args.call_collect,
            edition=args.edition,
            remote_download=args.remote_download,
            trust_keys=trust_keys)
    elif command == "uninstall":
        all_hosts = ((args.hosts or []) + (args.hub or []) + (args.clients or []))
        return commands.uninstall(all_hosts)
    elif command == "packages":
        log.warning("packages command is deprecated, please use the new command: download")
        return commands.download(tags=args.tags, version=args.version, edition=args.edition)
    elif command == "list":
        return commands.list_command(tags=args.tags, version=args.version, edition=args.edition)
    elif command == "download":
        return commands.download(tags=args.tags, version=args.version, edition=args.edition)
    elif command == "run":
        return commands.run(hosts=args.hosts, raw=args.raw, command=args.remote_command)
    elif command == "sudo":
        return commands.sudo(hosts=args.hosts, raw=args.raw, command=args.remote_command)
    elif command == "scp":
        return commands.scp(hosts=args.hosts, files=args.args)
    elif command == "spawn":
        if args.list_platforms:
            return commands.list_platforms()
        if args.init_config:
            return commands.init_cloud_config()
        if args.name and "," in args.name:
            user_error("Group --name may not contain commas")
        if args.aws and args.gcp:
            user_error("--aws and --gcp cannot be used at the same time")
        if args.role.endswith("s"):
            # role should be singular
            args.role = args.role[:-1]
        if args.gcp:
            provider = Providers.GCP
        else:
            # AWS is currently also the default
            provider = Providers.AWS
            if args.network:
                user_error("--network not supported for AWS")
            if args.no_public_ip:
                user_error("--no-public-ip not supported for AWS")
        if args.network and (args.network.count("/") != 1):
            user_error("Invalid network specified, needs to be in the network/subnet format")

        return commands.spawn(args.platform, args.count, args.role, args.name,
                              provider=provider, size=args.size, network=args.network,
                              public_ip=not args.no_public_ip, extend_group=args.append)
    elif command == "destroy":
        group_name = args.name if args.name else None
        return commands.destroy(group_name)
    elif command == "deploy":
        return commands.deploy(args.hub, args.directory)
    else:
        user_error("Unknown command: '{}'".format(command))


def validate_command(command, args):
    if command in ["install", "packages", "list", "download"]:
        if args.edition:
            args.edition = args.edition.lower()
            if args.edition == "core":
                args.edition = "community"
            if args.edition not in ["enterprise", "community"]:
                user_error("--edition must be either community or enterprise")
        else:
            args.edition = "enterprise"

    if command in ["uninstall"] and not (args.hosts or args.hub or args.clients):
        user_error("Use --hosts, --hub or --clients to specify remote hosts")

    if command == "install":
        if args.call_collect and not args.demo:
            user_error("--call-collect must be used with --demo")
        if not args.clients and not args.hub:
            user_error("Specify hosts using --hub and --clients")
        if args.hub and args.clients and args.package:
            user_error(
                "Use --hub-package / --client-package instead to distinguish between hosts")
        if args.package and (args.hub_package or args.client_package):
            user_error(
                "--package cannot be used in combination with --hub-package / --client-package")
        if args.package and not is_package_url(args.package):
            if not os.path.exists(os.path.expanduser(args.package)):
                user_error(f"Package/directory '{args.package}' does not exist")
        if args.hub_package and not is_package_url(args.hub_package):
            if not os.path.isfile(args.hub_package):
                user_error(f"Hub package '{args.hub_package}' does not exist")
        if args.client_package and not is_package_url(args.client_package):
            if not os.path.isfile(args.client_package):
                user_error(f"Client package '{args.client_package}' does not exist")

    if command in ["sudo", "run"]:
        if len(args.remote_command) != 1:
            user_error("cf-remote sude/run requires exactly 1 command (use quotes)")
        args.remote_command = args.remote_command[0]

    if command == "spawn" and not args.list_platforms and not args.init_config:
        # --list-platforms doesn't require any other options/arguments (TODO:
        # --provider), but otherwise all have to be given
        if not args.platform:
            user_error("--platform needs to be specified")
        if not args.count:
            user_error("--count needs to be specified")
        if not args.role:
            user_error("--role needs to be specified")
        if not args.name:
            user_error("--name needs to be specified")

    if command == "destroy":
        if not args.all and not args.name:
            user_error("One of --all or NAME required for destroy")

    if command == "deploy":
        if not args.directory:
            user_error("Must specify a masterfiles directory")
        if not args.hub:
            user_error("Must specify at least one hub")
        if not os.path.isdir(args.directory):
            user_error(f"'{args.directory}' is not a directory")


def is_in_cloud_state(name):
    if not os.path.exists(paths.CLOUD_STATE_FPATH):
        return False
    # else
    state = read_json(paths.CLOUD_STATE_FPATH)
    if name in state:
        return True
    if ("@" + name) in state:
        return True

    # search for a host in any of the groups
    for group in [key for key in state.keys() if key.startswith("@")]:
        if name in state[group]:
            return True

    return False


def get_cloud_hosts(name, private_ips=False):
    if not os.path.exists(paths.CLOUD_STATE_FPATH):
        return []

    state = read_json(paths.CLOUD_STATE_FPATH)
    group_name = None
    hosts = []
    if name.startswith("@") and name in state:
        # @some_group given and exists
        group_name = name
    elif ("@" + name) in state:
        # group_name given and @group_name exists
        group_name = "@" + name

    if group_name is not None:
        for name, info in state[group_name].items():
            if name == "meta":
                continue
            log.debug("found name '{}' in state, info='{}'".format(name, info))
            hosts.append(info)
    else:
        if name in state:
            # host_name given and exists at the top level
            hosts.append(state[name])
        else:
            for group_name in [key for key in state.keys() if key.startswith("@")]:
                if name in state[group_name]:
                    hosts.append(state[group_name][name])

    ret = []
    for host in hosts:
        if private_ips:
            key = "private_ips"
        else:
            key = "public_ips"

        ips = host.get(key, [])
        if len(ips) > 0:
            if host.get("user"):
                ret.append('{}@{}'.format(host.get("user"), ips[0]))
            else:
                ret.append(ips[0])
        else:
            ret.append(None)

    return ret


def resolve_hosts(string, single=False, private_ips=False):
    log.debug("resolving hosts from '{}'".format(string))
    if is_file_string(string):
        names = expand_list_from_file(string)
    else:
        names = string.split(",")

    ret = []

    for name in names:
        if is_in_cloud_state(name):
            hosts = get_cloud_hosts(name, private_ips)
            ret.extend(hosts)
            log.debug("found in cloud, adding '{}'".format(hosts))
        else:
            ret.append(name)

    if single:
        if len(ret) != 1:
            user_error("'{}' must contain exactly 1 hostname or IP".format(string))
        return ret[0]
    else:
        return ret


def validate_args(args):
    if args.version is True:  # --version with no second argument
        print_version_info()
        exit_success()

    if args.version and args.command not in ["install", "packages", "list", "download"]:
        user_error("Cannot specify version number in '{}' command".format(args.command))

    if "hosts" in args and args.hosts:
        log.debug("validate_args, hosts in args, args.hosts='{}'".format(args.hosts))
        args.hosts = resolve_hosts(args.hosts)
    if "clients" in args and args.clients:
        args.clients = resolve_hosts(args.clients)
    if "bootstrap" in args and args.bootstrap:
        args.bootstrap = [
            strip_user(host_info) for host_info in resolve_hosts(args.bootstrap, private_ips=True)
        ]
    if "hub" in args and args.hub:
        args.hub = resolve_hosts(args.hub)

    if not args.command:
        user_error("Invalid or missing command")
    args.command = args.command.strip()
    validate_command(args.command, args)


def main():
    args = get_args()
    if args.log_level:
        log.set_level(args.log_level)
    validate_args(args)

    exit_code = run_command_with_args(args.command, args)
    assert(type(exit_code) is int)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
