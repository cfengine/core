import argparse
import os
import sys

from cf_remote import log
from cf_remote import commands, paths
from cf_remote.utils import user_error, exit_success, expand_list_from_file, is_file_string
from cf_remote.utils import strip_user, read_json
from cf_remote.packages import Releases


def print_version_info():
    print("cf-remote version 0.1 (BETA)")
    print("Available CFEngine versions:")
    releases = Releases()
    print(releases)


def get_args():
    ap = argparse.ArgumentParser(
        description="Spooky CFEngine at a distance",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    ap.add_argument("--log-level", help="Specify detail of logging", type=str, default="WARNING")
    ap.add_argument(
        "--version", "-V", help="Print or specify version", nargs="?", type=str, const=True)

    command_help_hint = "Commands (use %s COMMAND --help to get more info)" % os.path.basename(sys.argv[0])
    subp = ap.add_subparsers(dest="command",
                             title=command_help_hint)

    sp = subp.add_parser("info", help="Get info about the given hosts")
    sp.add_argument("--hosts", "-H", help="Which hosts to get info for", type=str, required=True)

    sp = subp.add_parser("install", help="Install CFEngine on the given hosts")
    sp.add_argument("--edition", "-E", help="Enterprise or community packages", type=str)
    sp.add_argument("--package", help="Local path to package for transfer and install", type=str)
    sp.add_argument("--hub-package", help="Local path to package for --hub", type=str)
    sp.add_argument("--client-package", help="Local path to package for --clients", type=str)
    sp.add_argument("--bootstrap", "-B", help="cf-agent --bootstrap argument", type=str)
    sp.add_argument("--clients", "-c", help="Where to install client package", type=str)
    sp.add_argument("--hub", help="Where to install hub package", type=str)
    sp.add_argument(
        "--demo", help="Use defaults to make demos smoother (NOT secure)", action='store_true')
    sp.add_argument(
        "--call-collect", help="Enable call collect in --demo def.json", action='store_true')

    sp = subp.add_parser("uninstall", help="Install CFEngine on the given hosts")
    sp.add_argument("--clients", "-c", help="Where to uninstall", type=str)
    sp.add_argument("--hub", help="Where to uninstall", type=str)
    sp.add_argument("--hosts", "-H", help="Where to uninstall", type=str)

    sp = subp.add_parser("packages", help="Get info about available packages")
    sp.add_argument("--edition", "-E", help="Enterprise or community packages", type=str)
    sp.add_argument("tags", metavar="TAG", nargs="*")

    sp = subp.add_parser("run", help="Run the command given as arguments on the given hosts")
    sp.add_argument("--hosts", "-H", help="Which hosts to run the command on", type=str, required=True)
    sp.add_argument("args", help="Arguments", type=str, nargs='*')

    sp = subp.add_parser("sudo",
                         help="Run the command given as arguments on the given hosts with 'sudo'")
    sp.add_argument("--hosts", "-H", help="Which hosts to run the command on", type=str, required=True)
    sp.add_argument("args", help="Arguments", type=str, nargs='*')

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
    # TODO: --provider, --region (both optional)

    dp = subp.add_parser("destroy", help="Destroy hosts spawned in the clouds")
    dp.add_argument("--all", help="Destroy all hosts spawned in the clouds", action='store_true')
    dp.add_argument("name", help="Name fo the group of hosts to destroy", nargs='?')

    args = ap.parse_args()
    return args


def run_command_with_args(command, args):
    if command == "info":
        commands.info(args.hosts, None)
    elif command == "install":
        commands.install(
            args.hub,
            args.clients,
            package=args.package,
            bootstrap=args.bootstrap,
            hub_package=args.hub_package,
            client_package=args.client_package,
            version=args.version,
            demo=args.demo,
            call_collect=args.call_collect,
            edition=args.edition)
    elif command == "uninstall":
        all_hosts = ((args.hosts or []) + (args.hub or []) + (args.clients or []))
        commands.uninstall(all_hosts)
    elif command == "packages":
        commands.packages(tags=args.tags, version=args.version, edition=args.edition)
    elif command == "run":
        commands.run(hosts=args.hosts, command=" ".join(args.args))
    elif command == "sudo":
        commands.sudo(hosts=args.hosts, command=" ".join(args.args))
    elif command == "scp":
        commands.scp(hosts=args.hosts, files=args.args)
    elif command == "spawn":
        if args.list_platforms:
            commands.list_platforms()
            return
        elif args.init_config:
            commands.init_cloud_config()
            return
        # else
        if args.role.endswith("s"):
            # role should be singular
            args.role = args.role[:-1]
        commands.spawn(args.platform, args.count, args.role, args.name)
    elif command == "destroy":
        group_name = args.name if args.name else None
        commands.destroy(group_name)
    else:
        user_error("Unknown command: '{}'".format(command))


def validate_command(command, args):
    if command in ["install", "packages"]:
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

    if command == "install" and (args.call_collect and not args.demo):
        user_error("--call-collect must be used with --demo")

    if command == "install":
        if not args.clients and not args.hub:
            user_error("Specify hosts using --hub and --clients")
        if args.hub and args.clients and args.package:
            user_error(
                "Use --hub-package / --client-package instead to distinguish between hosts")
        if args.package and (args.hub_package or args.client_package):
            user_error(
                "--package cannot be used in combination with --hub-package / --client-package")
            # TODO: Find this automatically

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
        ret = expand_list_from_file(string)
    elif is_in_cloud_state(string):
        ret = get_cloud_hosts(string, private_ips)
        log.debug("found in cloud, ret='{}'".format(ret))
    else:
        ret = string.split(",")

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

    if args.version and args.command not in ["install", "packages"]:
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

    run_command_with_args(args.command, args)


if __name__ == "__main__":
    main()
