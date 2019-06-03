import sys
import fabric
from paramiko.ssh_exception import AuthenticationException
from invoke.exceptions import UnexpectedExit

from cf_remote import log


def connect(host, users=None):
    log.debug("Connecting to '{}'".format(host))
    if "@" in host:
        parts = host.split("@")
        assert len(parts) == 2
        host = parts[1]
        if not users:
            users = [parts[0]]
    if not users:
        users = ["Administrator", "ubuntu", "ec2-user", "centos", "vagrant", "root"]
    for user in users:
        try:
            log.debug("Attempting ssh: {}@{}".format(user, host))
            c = fabric.Connection(host=host, user=user)
            c.ssh_user = user
            c.ssh_host = host
            c.run("whoami", hide=True)
            return c
        except AuthenticationException:
            continue
    sys.exit("Could not ssh into '{}'".format(host))


# Decorator to make a function automatically connect
# Requires that first positional argument is host
# and connection should be a keyword argument with default None
# Uses a context manager (with) to ensure connections are closed
def auto_connect(func):
    def connect_wrapper(host, *args, **kwargs):
        if not kwargs.get("connection"):
            with connect(host, users=kwargs.get("users")) as connection:
                assert connection
                kwargs["connection"] = connection
                return func(host, *args, **kwargs)
        return func(host, *args, **kwargs)

    return connect_wrapper


def scp(file, remote, connection=None):
    if not connection:
        with connect(remote) as connection:
            scp(file, remote, connection)
    else:
        print("Copying: '{}' to '{}'".format(file, remote))
        connection.put(file)


def ssh_cmd(connection, cmd, errors=False):
    assert connection
    try:
        log.debug("Running over SSH: '{}'".format(cmd))
        result = connection.run(cmd, hide=True)
        output = result.stdout.replace("\r\n", "\n").strip("\n")
        log.debug("'{}' -> '{}'".format(cmd, output))
        return output
    except UnexpectedExit as e:
        msg = "Non-sudo command unexpectedly exited: '{}'".format(cmd)
        if errors:
            print(e)
            log.error(msg)
        else:
            log.debug(str(e))
            log.debug(msg)
        return None


def ssh_sudo(connection, cmd, errors=False):
    assert connection
    try:
        log.debug("Running(sudo) over SSH: '{}'".format(cmd))
        sudo_cmd = 'sudo bash -c "{}"'.format(cmd.replace('"', r'\"'))
        result = connection.run(sudo_cmd, hide=True, pty=True)
        output = result.stdout.strip("\n")
        log.debug("'{}' -> '{}'".format(cmd, output))
        return output
    except UnexpectedExit as e:
        msg = "Sudo command unexpectedly exited: '{}'".format(cmd)
        if errors:
            print(e)
            log.error(msg)
        else:
            log.debug(str(e))
            log.debug(msg)
        return None
