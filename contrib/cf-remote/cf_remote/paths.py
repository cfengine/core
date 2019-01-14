import os


def path_append(dir, subdir):
    dir = os.path.abspath(os.path.expanduser(dir))
    return dir if not subdir else os.path.join(dir, subdir)


def cfengine_dir(subdir=None):
    return path_append("~/.cfengine/", subdir)


def cf_remote_dir(subdir=None):
    return path_append(cfengine_dir("cf-remote"), subdir)


def cf_remote_packages_dir(subdir=None):
    return path_append(cf_remote_dir("packages"), subdir)
