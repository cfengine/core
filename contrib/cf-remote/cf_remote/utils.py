import os
import sys
import re
import json
import getpass
from collections import OrderedDict
from cf_remote import log
from datetime import datetime


def is_in_past(date):
    now = datetime.now()
    date = datetime.strptime(date, "%Y-%m-%d")
    return now > date


def canonify(string):
    legal = "abcdefghijklmnopqrstuvwxyz0123456789-_"
    string = string.strip()
    string = string.lower()
    string = string.replace(".", "_")
    string = string.replace(" ", "_")
    results = []
    for c in string:
        if c in legal:
            results.append(c)
    return "".join(results)


def user_error(msg):
    sys.exit("cf-remote: " + msg)


def exit_success():
    sys.exit(0)


def mkdir(path):
    if not os.path.exists(path):
        log.info("Creating directory: '{}'".format(path))
        os.makedirs(path)
    else:
        log.debug("Directory already exists: '{}'".format(path))


def ls(path):
    return os.listdir(path)


def read_file(path):
    try:
        with open(path, "r") as f:
            return f.read()
    except FileNotFoundError:
        return None


def save_file(path, data):
    mkdir("/".join(path.split("/")[0:-1]))
    with open(path, "w") as f:
        f.write(data)


def pretty(data):
    return json.dumps(data, indent=2)


def package_path():
    above_dir = os.path.dirname(__file__)
    return os.path.abspath(above_dir)


def above_package_path():
    path = package_path() + "/../"
    return os.path.abspath(path)


def is_package_url(string):
    return bool(re.match("https?://.+/.+\.(rpm|deb|msi)", string))


def get_package_name(url):
    assert(is_package_url(url))
    return url.rsplit("/", 1)[-1]


def read_json(path):
    try:
        with open(path, "r") as f:
            return json.loads(f.read(), object_pairs_hook=OrderedDict)
    except FileNotFoundError:
        return None


def write_json(path, data):
    data = pretty(data)
    return save_file(path, data)


def os_release(inp):
    if not inp:
        log.debug("Cannot parse os-release file (empty)")
        return None
    d = OrderedDict()
    for line in inp.splitlines():
        line = line.strip()
        if "=" not in line:
            continue
        key, sep, value = line.partition("=")
        assert "=" not in key
        if len(value) > 1 and value[0] == value[-1] and value[0] in ["'", '"']:
            value = value[1:-1]
        d[key] = value
    return d


def parse_version(string):
    if not string:
        return None
    # 'CFEngine Core 3.12.1 \n CFEngine Enterprise 3.12.1'
    #                ^ split and use this part for version number
    words = string.split()
    if len(words) < 3:
        return None
    version_number = words[2]
    edition = words[1]
    if edition == "Core":
        edition = "Community"
    if "Enterprise" in string:
        edition = "Enterprise"
    return "{} ({})".format(version_number, edition)


def parse_systeminfo(data):
    # TODO: This is not great, it misses a lot of the nested data
    lines = [s.strip() for s in data.split("\n") if s.strip()]
    data = OrderedDict()
    for line in lines:
        sections = line.split(":")
        key = sections[0].strip()
        value = ":".join(sections[1:]).strip()
        data[key] = value
    return data


def column_print(data):
    width = 0
    for key in data:
        if len(key) > width:
            width = len(key)

    for key, value in data.items():
        fill = " " * (width - len(key))
        print("{}{} : {}".format(key, fill, value))


def is_file_string(string):
    return string and string.startswith(("./", "~/", "/", "../"))


def expand_list_from_file(string):
    assert is_file_string(string)

    location = os.path.expanduser(string)
    if not os.path.exists(location):
        user_error("Hosts file '{}' does not exist".format(location))
    if not os.path.isfile(location):
        user_error("'{}' is not a file".format(location))
    if not os.access(location, os.R_OK):
        user_error("Cannot read '{}' - Permission denied".format(location))

    with open(location, "r") as f:
        hosts = [line.strip() for line in f if line.strip()]

    return hosts


def strip_user(host):
    """Strips the 'user@' info from a host spec"""
    idx = host.find("@")
    if idx != -1:
        return host[(idx + 1) :]
    return host


def whoami():
    return getpass.getuser()

def print_progress_dot(*args):
    print(".", end="")
    sys.stdout.flush()      # STDOUT is line-buffered
