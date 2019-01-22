import os
import requests
from cf_remote.utils import write_json, mkdir
from cf_remote import log
from cf_remote.paths import cf_remote_dir, cf_remote_packages_dir


def get_json(url):
    r = requests.get(url)
    assert r.status_code >= 200 and r.status_code < 300
    data = r.json()

    filename = os.path.basename(url)
    dir = cf_remote_dir("json")
    path = os.path.join(dir, filename)
    log.debug("Saving '{}' to '{}'".format(url, path))
    write_json(path, data)

    return data


def download_package(url):
    filename = os.path.basename(url)
    dir = cf_remote_packages_dir()
    mkdir(dir)
    location = os.path.join(dir, filename)
    if os.path.exists(location):
        print("Package already downloaded: '{}'".format(location))
        return location
    print("Downloading package: '{}'".format(location))
    os.system("curl --silent -L '{}' -o '{}'".format(url, location))
    return location
