import argparse

from cf_remote import log
from cf_remote import commands
from cf_remote.utils import user_error, exit_success, expand_list_from_file, is_file_string, strip_user
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

    ap.add_argument("--hosts", "-H", help="Which hosts to connect to (ssh)", type=str)
    ap.add_argument("--clients", "-c", help="Where to install client package", type=str)
    ap.add_argument("--hub", help="Where to install hub package", type=str)
    ap.add_argument("--bootstrap", "-B", help="cf-agent --bootstrap argument", type=str)
    ap.add_argument("--package", help="Local path to package for transfer and install", type=str)
    ap.add_argument("--hub-package", help="Local path to package for --hub", type=str)
    ap.add_argument("--client-package", help="Local path to package for --clients", type=str)
    ap.add_argument("--log-level", help="Specify detail of logging", type=str, default="WARNING")
    ap.add_argument(
        "--demo", help="Use defaults to make demos smoother (NOT secure)", action='store_true')
    ap.add_argument(
        "--call-collect", help="Enable call collect in --demo def.json", action='store_true')
    ap.add_argument(
        "--version", "-V", help="Print or specify version", nargs="?", type=str, const=True)
    ap.add_argument("command", help="Action to perform", type=str, nargs='?')
    ap.add_argument("args", help="Arguments", type=str, nargs='*')

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
            call_collect=args.call_collect)
    elif command == "packages":
        commands.packages(tags=args.args, version=args.version)
    elif command == "run":
        commands.run(hosts=args.hosts, command=" ".join(args.args))
    elif command == "sudo":
        commands.sudo(hosts=args.hosts, command=" ".join(args.args))
    elif command == "scp":
        commands.scp(hosts=args.hosts, files=args.args)
    else:
        user_error("Unknown command: '{}'".format(command))


def validate_command(command, args):
    if command in ["info", "sudo", "run"] and not args.hosts:
        user_error("Use --hosts to specify remote hosts")

    if args.bootstrap and command != "install":
        user_error("--bootstrap can only be used with install command")

    if args.call_collect and not args.demo:
        user_error("--call-collect must be used with --demo")

    if command == "install":
        if args.hosts:
            user_error("Use --clients and --hub instead of --hosts")
        if not args.clients and not args.hub:
            user_error("Specify hosts using --hub and --clients")
        if args.hub and args.clients and args.package:
            user_error(
                "Use --hub-package / --client-package instead to distinguish between hosts")
        if args.package and (args.hub_package or args.client_package):
            user_error(
                "--package cannot be used in combination with --hub-package / --client-package")
            # TODO: Find this automatically


def file_or_comma_list(string):
    if is_file_string(string):
        return expand_list_from_file(string)
    return string.split(",")


def file_or_single_host(string):
    assert "," not in string
    one_list = file_or_comma_list(string)
    if len(one_list) != 1:
        user_error("File '{}' must contain exactly 1 hostname or IP".format(string))
    return one_list[0]


def validate_args(args):
    if args.version is True:  # --version with no second argument
        print_version_info()
        exit_success()

    if args.version and args.command not in ["install", "packages"]:
        user_error("Cannot specify version number in '{}' command".format(args.command))

    if args.hosts:
        args.hosts = file_or_comma_list(args.hosts)
    if args.clients:
        args.clients = file_or_comma_list(args.clients)
    if args.bootstrap:
        args.bootstrap = [
            strip_user(host_info) for host_info in file_or_comma_list(args.bootstrap)
        ]
    if args.hub:
        args.hub = file_or_comma_list(args.hub)

    args.command = args.command.strip()
    if not args.command:
        user_error("Invalid or missing command")
    validate_command(args.command, args)


def main():
    args = get_args()
    validate_args(args)
    if args.log_level:
        log.set_level(args.log_level)

    run_command_with_args(args.command, args)


if __name__ == "__main__":
    main()
