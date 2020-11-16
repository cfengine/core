import os
import sys
import time
from multiprocessing.dummy import Pool

from cf_remote.remote import get_info, print_info, HostInstaller, uninstall_host, run_command, transfer_file, deploy_masterfiles
from cf_remote.packages import Releases
from cf_remote.web import download_package
from cf_remote.paths import cf_remote_dir, CLOUD_CONFIG_FPATH, CLOUD_STATE_FPATH, cf_remote_packages_dir
from cf_remote.utils import save_file, strip_user, read_json, write_json, whoami, get_package_name
from cf_remote.utils import user_error, is_package_url, print_progress_dot
from cf_remote.spawn import VM, VMRequest, Providers, AWSCredentials, GCPCredentials
from cf_remote.spawn import spawn_vms, destroy_vms, dump_vms_info, get_cloud_driver
from cf_remote import log
from cf_remote import cloud_data

def info(hosts, users=None):
    assert hosts
    log.debug("hosts='{}'".format(hosts))
    errors = 0
    for host in hosts:
        data = get_info(host, users=users)
        if data:
            print_info(data)
        else:
            errors += 1
    return errors


def run(hosts, command, users=None, sudo=False, raw=False):
    assert hosts
    errors = 0
    for host in hosts:
        lines = run_command(host=host, command=command, users=users, sudo=sudo)
        if lines is None:
            log.error("Command: '{}'\nFailed on host: '{}'".format(command, host))
            errors += 1
            continue
        host_colon = (host + ":").ljust(16)
        if lines == "":
            if not raw:
                print("{} '{}'".format(host_colon, command))
            continue
        cmd = command
        lines = lines.replace("\r", "")
        for line in lines.split("\n"):
            if raw:
                print(line)
            elif cmd:
                print("{} '{}' -> '{}'".format(host_colon, cmd, line))
                fill = " " * (len(cmd) + 7)
                cmd = None
            else:
                print("{}{}'{}'".format(host_colon, fill, line))
    return errors


def sudo(hosts, command, users=None, raw=False):
    return run(hosts, command, users, sudo=True, raw=raw)


def scp(hosts, files, users=None):
    errors = 0
    for host in hosts:
        for file in files:
            errors += transfer_file(host, file, users)
    return errors

def _download_urls(urls):
    """Download packages from URLs, replace URLs with filenames

    Return a new list of packages where URLs are replaced with paths
    to packages which have been downloaded. Other values, like None
    and paths to local packages are preserved.
    """
    urls_dir = cf_remote_packages_dir("url_specified")

    downloaded_urls = []
    downloaded_paths = []
    paths = []
    for package_url in urls:
        # Skip anything that is not a package url:
        if package_url is None or not is_package_url(package_url):
            paths.append(package_url)
            continue

        if not os.path.isdir(urls_dir):
            os.mkdir(urls_dir)

        # separate name from url and construct path for downloaded file
        url, name = package_url, get_package_name(package_url)
        path = os.path.join(urls_dir, name)

        # replace url with local path to package in list which will be returned
        paths.append(path)

        if path in downloaded_paths and url not in downloaded_urls:
            user_error(f"2 packages with the same name '{name}' from different URLs")

        download_package(url, path)
        downloaded_urls.append(url)
        downloaded_paths.append(path)

    return paths


def _verify_package_urls(urls):
    verified_urls = []
    for package_url in urls:
        if package_url is None:
            verified_urls.append(None)
            continue

        # Throw an error if not valid URL
        if is_package_url(package_url):
            verified_urls.append(package_url)
        else:
            user_error("Wrong package URL: {}".format(package_url))

    return verified_urls

def _maybe_packages_in_folder(package):
    if not (package and type(package) is str):
        return package
    folder = os.path.abspath(os.path.expanduser(package))
    if os.path.isdir(folder):
        return [os.path.join(folder, f) for f in os.listdir(folder)]
    return package

def install(
        hubs,
        clients,
        *,
        bootstrap=None,
        package=None,
        hub_package=None,
        client_package=None,
        version=None,
        demo=False,
        call_collect=False,
        edition=None,
        remote_download=False,
        trust_keys=None):
    assert hubs or clients
    assert not (hubs and clients and package)
    assert (trust_keys is None) or hasattr(trust_keys, "__iter__")
    # These assertions are checked/ensured in main.py

    # If there are URLs in any of the package strings and remote_download is FALSE, download and replace with path:
    packages = (package, hub_package, client_package)
    if remote_download:
        package, hub_package, client_package = _verify_package_urls(packages)
    else:
        package, hub_package, client_package = _download_urls(packages)

    # If any of these are folders, transform them to lists of the files inside those folders:
    package = _maybe_packages_in_folder(package)
    hub_package = _maybe_packages_in_folder(hub_package)
    client_package = _maybe_packages_in_folder(client_package)

    # If --hub-package or --client-pacakge are not specified, use --package argument:
    if not hub_package:
        hub_package = package
    if not client_package:
        client_package = package

    if bootstrap:
        if type(bootstrap) is str:
            bootstrap = [bootstrap]
        save_file(os.path.join(cf_remote_dir(), "policy_server.dat"), "\n".join(bootstrap + [""]))

    hub_jobs = []
    if hubs:
        show_host_info = (len(hubs) == 1)
        if type(hubs) is str:
            hubs = [hubs]
        for index, hub in enumerate(hubs):
            log.debug("Installing {} hub package on '{}'".format(edition, hub))
            hub_jobs.append(HostInstaller(
                hub,
                hub=True,
                packages=hub_package,
                bootstrap=bootstrap[index % len(bootstrap)] if bootstrap else None,
                version=version,
                demo=demo,
                call_collect=call_collect,
                edition=edition,
                show_info=show_host_info,
                remote_download=remote_download,
                trust_keys=trust_keys))

    errors = 0
    if hub_jobs:
        with Pool(len(hub_jobs)) as hubs_install_pool:
            hubs_install_pool.map(lambda job: job.run(), hub_jobs)
        errors = sum(job.errors for job in hub_jobs)

    if errors > 0:
        s = "s" if errors > 1 else ""
        log.error(f"{errors} error{s} encountered while installing hub packages, aborting...")
        return errors

    client_jobs = []
    show_host_info = (clients and (len(clients) == 1))
    for index, host in enumerate(clients or []):
        log.debug("Installing {} client package on '{}'".format(edition, host))
        client_jobs.append(HostInstaller(
            host,
            hub=False,
            packages=client_package,
            bootstrap=bootstrap[index % len(bootstrap)] if bootstrap else None,
            version=version,
            demo=demo,
            edition=edition,
            show_info=show_host_info,
            remote_download=remote_download,
            trust_keys=trust_keys))

    if client_jobs:
        with Pool(len(client_jobs)) as clients_install_pool:
            clients_install_pool.map(lambda job: job.run(), client_jobs)
        errors += sum(job.errors for job in client_jobs)

    if demo and hubs:
        for hub in hubs:
            print(
                "Your demo hub is ready: https://{}/ (Username: admin, Password: password)".
                format(strip_user(hub)))

    if errors > 0:
        s = "s" if errors > 1 else ""
        log.error(f"{errors} error{s} encountered while installing client packages")

    return errors

def _iterate_over_packages(tags=None, version=None, edition=None, download=False):
    releases = Releases(edition)
    print("Available releases: {}".format(releases))

    release = releases.default
    if version:
        release = releases.pick_version(version)
    print("Using {}:".format(release))
    artifacts = release.find(tags)

    if len(artifacts) == 0:
        print("No suitable packages found")
    else:
        for artifact in artifacts:
            if download:
                download_package(artifact.url)
            else:
                print(artifact.url)
    return 0

# named list_command to not conflict with list()
def list_command(tags=None, version=None, edition=None):
    return _iterate_over_packages(tags, version, edition, False)

def download(tags=None, version=None, edition=None):
    return _iterate_over_packages(tags, version, edition, True)

def spawn(platform, count, role, group_name, provider=Providers.AWS, region=None,
          size=None, network=None, public_ip=True, extend_group=False):
    if os.path.exists(CLOUD_CONFIG_FPATH):
        creds_data = read_json(CLOUD_CONFIG_FPATH)
    else:
        print("Cloud credentials not found at %s" % CLOUD_CONFIG_FPATH)
        return 1

    if os.path.exists(CLOUD_STATE_FPATH):
        vms_info = read_json(CLOUD_STATE_FPATH)
    else:
        vms_info = dict()

    group_key = "@%s" % group_name
    group_exists = group_key in vms_info
    if not extend_group and group_exists:
        print("Group '%s' already exists!" % group_key)
        return 1

    creds = None
    sec_groups = None
    key_pair = None
    if provider == Providers.AWS:
        try:
            creds = AWSCredentials(creds_data["aws"]["key"], creds_data["aws"]["secret"])
            sec_groups = creds_data["aws"]["security_groups"]
            key_pair = creds_data["aws"]["key_pair"]
        except KeyError:
            print("Incomplete AWS credential info") # TODO: report missing keys
            return 1

        region = region or creds_data["aws"].get("region", "eu-west-1")
    elif provider == Providers.GCP:
        try:
            creds = GCPCredentials(creds_data["gcp"]["project_id"],
                                   creds_data["gcp"]["service_account_id"],
                                   creds_data["gcp"]["key_path"])
        except KeyError:
            print("Incomplete GCP credential info") # TODO: report missing keys
            return 1

        region = region or creds_data["gcp"].get("region", "europe-west1-b")

    # TODO: Do we have to complicate this instead of just assuming existing VMs
    # were created by this code and thus follow the naming pattern from this
    # code?
    if group_exists:
        range_start = len([key for key in vms_info[group_key].keys() if key != "meta"])
    else:
        range_start = 0

    requests = []
    for i in range(range_start, range_start + count):
        vm_name = whoami()[0:2] + group_name + "-" + platform + role + str(i)
        requests.append(VMRequest(platform=platform,
                                  name=vm_name,
                                  size=size,
                                  public_ip=public_ip))
    print("Spawning VMs...", end="")
    sys.stdout.flush()
    vms = spawn_vms(requests, creds, region, key_pair,
                    security_groups=sec_groups,
                    provider=provider,
                    network=network,
                    role=role,
                    spawned_cb=print_progress_dot)
    print("DONE")

    if public_ip and (not all(vm.public_ips for vm in vms)):
        print("Waiting for VMs to get IP addresses...", end="")
        sys.stdout.flush()      # STDOUT is line-buffered
        while not all(vm.public_ips for vm in vms):
            time.sleep(1)
            print_progress_dot()
        print("DONE")

    if group_exists:
        vms_info[group_key].update(dump_vms_info(vms))
    else:
        vms_info[group_key] = dump_vms_info(vms)

    write_json(CLOUD_STATE_FPATH, vms_info)
    print("Details about the spawned VMs can be found in %s" % CLOUD_STATE_FPATH)

    return 0

def destroy(group_name=None):
    if os.path.exists(CLOUD_CONFIG_FPATH):
        creds_data = read_json(CLOUD_CONFIG_FPATH)
    else:
        print("Cloud credentials not found at %s" % CLOUD_CONFIG_FPATH)
        return 1

    aws_creds = None
    try:
        aws_creds = AWSCredentials(creds_data["aws"]["key"], creds_data["aws"]["secret"])
    except KeyError:
        # missing/incomplete AWS credentials, may not be needed, though
        pass

    gcp_creds = None
    try:
        gcp_creds = GCPCredentials(creds_data["gcp"]["project_id"],
                                   creds_data["gcp"]["service_account_id"],
                                   creds_data["gcp"]["key_path"])
    except KeyError:
        # missing/incomplete GCP credentials, may not be needed, though
        pass

    if not os.path.exists(CLOUD_STATE_FPATH):
        print("No saved cloud state info")
        return 1

    vms_info = read_json(CLOUD_STATE_FPATH)

    to_destroy = []
    if group_name:
        print("Destroying hosts in the '%s' group" % group_name)
        if not group_name.startswith("@"):
            group_name = "@" + group_name
        if group_name not in vms_info:
            print("Group '%s' not found" % group_name)
            return 1

        region = vms_info[group_name]["meta"]["region"]
        provider = vms_info[group_name]["meta"]["provider"]
        if provider == "aws":
            if aws_creds is None:
                user_error("Missing/incomplete AWS credentials")
                return 1
            driver = get_cloud_driver(Providers.AWS, aws_creds, region)
        if provider == "gcp":
            if gcp_creds is None:
                user_error("Missing/incomplete GCP credentials")
                return 1
            driver = get_cloud_driver(Providers.GCP, gcp_creds, region)

        nodes = driver.list_nodes()
        for name, vm_info in vms_info[group_name].items():
            if name == "meta":
                continue
            vm_uuid = vm_info["uuid"]
            vm = VM.get_by_uuid(vm_uuid, nodes=nodes)
            if vm is not None:
                to_destroy.append(vm)
            else:
                print("VM '%s' not found in the clouds" % vm_uuid)
        del vms_info[group_name]
    else:
        print("Destroying all hosts")
        for group_name in [key for key in vms_info.keys() if key.startswith("@")]:
            region = vms_info[group_name]["meta"]["region"]
            provider = vms_info[group_name]["meta"]["provider"]
            if provider == "aws":
                if aws_creds is None:
                    user_error("Missing/incomplete AWS credentials")
                    return 1
                driver = get_cloud_driver(Providers.AWS, aws_creds, region)
            if provider == "gcp":
                if gcp_creds is None:
                    user_error("Missing/incomplete GCP credentials")
                    return 1
                driver = get_cloud_driver(Providers.GCP, gcp_creds, region)

            nodes = driver.list_nodes()
            for name, vm_info in vms_info[group_name].items():
                if name == "meta":
                    continue
                vm_uuid = vm_info["uuid"]
                vm = VM.get_by_uuid(vm_uuid, nodes=nodes)
                if vm is not None:
                    to_destroy.append(vm)
                else:
                    print("VM '%s' not found in the clouds" % vm_uuid)
            del vms_info[group_name]

    destroy_vms(to_destroy)
    write_json(CLOUD_STATE_FPATH, vms_info)
    return 0

def list_platforms():
    print("Available platforms:")
    for key in sorted(cloud_data.aws_platforms.keys()):
        print(key)
    return 0

def init_cloud_config():
    if os.path.exists(CLOUD_CONFIG_FPATH):
        print("File %s already exists" % CLOUD_CONFIG_FPATH)
        return 1
    empty_config = {
        "aws": {
            "key": "TBD",
            "secret": "TBD",
            "key_pair": "TBD",
            "security_groups": ["TBD"],
            "region": "OPTIONAL (DEFAULT: eu-west-1)",
        },
        "gcp": {
            "project_id": "TBD",
            "service_account_id": "TBD",
            "key_path": "TBD",
            "region": "OPTIONAL (DEFAULT: europe-west1-b)",
        },
    }
    write_json(CLOUD_CONFIG_FPATH, empty_config)
    print("Config file %s created, please complete the configuration in it." % CLOUD_CONFIG_FPATH)
    return 0

def uninstall(hosts):
    errors = 0
    for host in hosts:
        errors += uninstall_host(host)
    return errors

def deploy(hubs, directory):
    assert(directory.endswith("/masterfiles"))
    assert(os.path.isfile(directory + "/autogen.sh"))
    os.system(f"bash -c 'cd {directory} && ./autogen.sh 1>/dev/null 2>&1'")
    assert(os.path.isfile(directory + "/promises.cf"))

    assert(not cf_remote_dir().endswith("/"))
    tarball = cf_remote_dir() + "/masterfiles.tgz"
    above = directory[0:-len("/masterfiles")]
    os.system(f"rm -rf {tarball}")
    os.system(f"tar -czf {tarball} -C {above} masterfiles")
    errors = 0
    for hub in hubs:
        errors += deploy_masterfiles(hub, tarball)
    return errors
