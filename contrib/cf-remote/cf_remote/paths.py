import os


def path_append(dir, subdir):
    dir = os.path.abspath(os.path.expanduser(dir))
    return dir if not subdir else os.path.join(dir, subdir)


def cfengine_dir(subdir=None):
    return path_append("~/.cfengine/", subdir)


def cf_remote_dir(subdir=None):
    return path_append(cfengine_dir("cf-remote"), subdir)


def cf_remote_file(fname=None):
    return path_append(cfengine_dir("cf-remote"), fname)


def cf_remote_packages_dir(subdir=None):
    return path_append(cf_remote_dir("packages"), subdir)

CLOUD_CONFIG_FNAME = "cloud_config.json"
CLOUD_CONFIG_FPATH = cf_remote_file(CLOUD_CONFIG_FNAME)
CLOUD_STATE_FNAME = "cloud_state.json"
CLOUD_STATE_FPATH = cf_remote_file(CLOUD_STATE_FNAME)
