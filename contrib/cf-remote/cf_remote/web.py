import os
import urllib.request
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


def download_package(url, path=None):
    if not path:
        filename = os.path.basename(url)
        directory = cf_remote_packages_dir()
        mkdir(directory)
        path = os.path.join(directory, filename)
    if os.path.exists(path):
        print("Package already downloaded: '{}'".format(path))
        return path
    print("Downloading package: '{}'".format(path))
    with open(path, "wb") as f:
        f.write(urllib.request.urlopen(url).read())
    return path
