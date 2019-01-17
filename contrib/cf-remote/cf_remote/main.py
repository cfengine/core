import argparse

from cf_remote import log
from cf_remote import commands
from cf_remote.utils import user_error


def get_args():
    ap = argparse.ArgumentParser(
        description="Spooky CFEngine at a distance",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    ap.add_argument("--hosts", "-H", help="Which hosts to connect to (ssh)", type=str)
    ap.add_argument("--clients", help="Where to install client package", type=str)
    ap.add_argument("--hub", help="Where to install hub package", type=str)
    ap.add_argument("--bootstrap", help="cf-agent --bootstrap argument", type=str)
    ap.add_argument("--package", help="Local path to package for transfer and install", type=str)
    ap.add_argument("--hub-package", help="Local path to package for --hub", type=str)
    ap.add_argument("--client-package", help="Local path to package for --clients", type=str)
    ap.add_argument("--log-level", help="Specify detail of logging", type=str, default="WARNING")
    ap.add_argument("command", help="Action to perform", type=str, nargs='?', default="info")
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
            client_package=args.client_package)
    elif command == "packages":
        commands.packages(tags=args.args)
    else:
        user_error("Unknown command: '{}'".format(command))


def validate_command(command, args):
    if command == "info" and not args.hosts:
        user_error("Use --hosts to specify remote hosts")
    if command == "install":
        if args.hosts:
            user_error("Use --clients and --hub instead of --hosts")
        if not args.clients and not args.hub:
            user_error("Specify hosts using --hub and --clients")
        if args.hub and args.clients and args.package:
            user_error(
                "Use --hub-package / --client-package instead to distinguish between hosts")
        if not (args.package or args.client_package or args.hub_package):
            user_error(
                "Specify local package file(s) with --package / --hub-package / --client-package")
        if args.package and (args.hub_package or args.client_package):
            user_error(
                "--package cannot be used in combination with --hub-package / --client-package")
        if args.clients and not (args.package or args.client_package):
            user_error("Specify client package using --client-package")
        if args.hub and not (args.package or args.hub_package):
            user_error("Specify hub package using --hub-package")
            # TODO: Find this automatically


def validate_args(args):
    if args.hosts:
        args.hosts = args.hosts.split(",")
    if args.clients:
        args.clients = args.clients.split(",")
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
