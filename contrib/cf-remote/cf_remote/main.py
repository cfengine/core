import argparse

from cf_remote import log
from cf_remote import commands
from cf_remote.utils import user_error


def get_args():
    ap = argparse.ArgumentParser(
        description="Spooky CFEngine at a distance",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    ap.add_argument("--hosts", "-H", help="Which hosts to connect to (ssh)", type=str)
    ap.add_argument("--log-level", help="Specify detail of logging", type=str, default="WARNING")
    ap.add_argument("command", help="Action to perform", type=str, nargs='?', default="info")
    ap.add_argument("args", help="Arguments", type=str, nargs='*')

    args = ap.parse_args()
    return args


def run_command_with_args(command, args):
    if command == "info":
        return commands.info(args.hosts, None)
    user_error("Unknown command: '{}'".format(command))


def validate_command(command, args):
    if command == "info" and not args.hosts:
        user_error("Use --hosts to specify remote hosts")


def validate_args(args):
    if args.hosts:
        args.hosts = args.hosts.split(",")
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
