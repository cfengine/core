import string
import random
from collections import namedtuple
from enum import Enum
from multiprocessing.dummy import Pool

from libcloud.compute.types import Provider
from libcloud.compute.providers import get_driver
from libcloud.compute.base import NodeSize, NodeImage

from cf_remote.cloud_data import aws_platforms
from cf_remote.utils import whoami
from cf_remote import log


_NAME_RANDOM_PART_LENGTH = 4

AWSCredentials = namedtuple("AWSCredentials", ["key", "secret"])
GCPCredentials = namedtuple("GCPCredentials", ["project_ID", "SA_ID", "key_path"])

VMRequest = namedtuple("VMRequest", ["platform", "name", "size", "public_ip"])

_DriverSpec = namedtuple("_DriverSpec", ["provider", "creds", "region"])

# _DriverSpec -> libcloud.providers.get_driver()
_DRIVERS = dict()


class Providers(Enum):
    AWS = 1
    GCP = 2

    def __str__(self):
        return self.name.lower()


class VM:
    def __init__(self, name, driver, node, role=None,
                 platform=None, size=None, key_pair=None, security_groups=None, user=None,
                 provider=None):
        self._name = name
        self._driver = driver
        self._node = node
        self._platform = platform
        self._size = size
        self._key_pair = key_pair
        self._sec_groups = security_groups
        self._user = user
        self.role = role
        self._provider = provider

    @classmethod
    def get_by_ip(cls, ip, driver=None, nodes=None):
        if nodes is None and driver is None:
            if len(_DRIVERS.keys()) == 1:
                driver = _DRIVERS.items()[0]
            else:
                print("Don't know which driver to use: %s" % _DRIVERS.keys())
                return None

        nodes = nodes or driver.list_nodes()
        for node in nodes:
            if node.state in (0, 'running') and (ip in node.public_ips or ip in node.private_ips):
                return cls(node.name, driver, node)
        return None

    @classmethod
    def get_by_name(cls, name, driver=None, nodes=None):
        if nodes is None and driver is None:
            if len(_DRIVERS.keys()) == 1:
                driver = _DRIVERS.items()[0]
            else:
                print("Don't know which driver to use: %s" % _DRIVERS.keys())
                return None

        nodes = nodes or driver.list_nodes()
        for node in nodes:
            if node.state in (0, 'running') and node.name == name:
                return cls(node.name, driver, node)
        return None

    @classmethod
    def get_by_uuid(cls, uuid, driver=None, nodes=None):
        if nodes is None and driver is None:
            if len(_DRIVERS.keys()) == 1:
                driver = _DRIVERS.items()[0]
            else:
                print("Don't know which driver to use: %s" % _DRIVERS.keys())
                return None

        nodes = nodes or driver.list_nodes()
        for node in nodes:
            if node.uuid == uuid:
                return cls(node.name, driver, node)
        return None


    @classmethod
    def get_by_info(cls, driver, vm_info, nodes=None):
        nodes = nodes or driver.list_nodes()
        for node in nodes:
            if (("name" in vm_info and vm_info["name"] == node.name) or
                ("public_ips" in vm_info and set(vm_info["public_ips"]).intersection(set(node.public_ips))) or
                ("private_ips" in vm_info and set(vm_info["private_ips"]).intersection(set(node.private_ips)))):
                return cls(node.name, driver, node, role=vm_info.get("role"),
                           platform=vm_info.get("platform"), size=vm_info.get("size"),
                           key_pair=vm_info.get("key_pair"), security_groups=vm_info.get("security_groups"))
        return None

    @property
    def name(self):
        return self._name

    @property
    def uuid(self):
        return self._node.uuid

    @property
    def driver(self):
        return self._driver

    @property
    def platform(self):
        return self._platform

    @property
    def region(self):
        data = self._node or self._data
        if "zone" in data.extra:
            return data.extra["zone"].name

        region = self._driver.region
        if (type(region) != str) and hasattr(region, "name"):
            return region.name
        else:
            return str(region)

    @property
    def size(self):
        return self._size

    @property
    def key_pair(self):
        return self._key_pair

    @property
    def security_groups(self):
        return self._sec_groups

    @property
    def user(self):
        return self._user

    @property
    def provider(self):
        return self._provider

    @property
    def _data(self):
        # We need to refresh this every time to get fresh data because
        # libcloud's drivers seem to be returning just snapshots of info (IOW,
        # things are not updated).

        # GCP waits for VMs to fully initialize in create_node() so self._node
        # is as fresh as we need it to be for a running VM.
        if (self._provider == Providers.GCP) and self._node:
            return self._node

        for node in self._driver.list_nodes():
            if node is self._node or node.uuid == self._node.uuid:
                return node
        raise RuntimeError("Cannot find data for '%s' in its driver" % self._name)

    @property
    def state(self):
        return self._data.state

    @property
    def public_ips(self):
        return self._data.public_ips or []

    @property
    def private_ips(self):
        return self._data.private_ips or []

    @property
    def info(self):
        ret = {
            "platform": self.platform,
            "region": self.region,
            "size": self.size,
            "private_ips": self.private_ips,
            "public_ips": self.public_ips,
            "uuid": self.uuid,
        }
        if self.user:
            ret["user"] = self.user
        if self.role:
            ret["role"] = self.role
        if self.key_pair:
            ret["key_pair"] = self.key_pair
        if self.security_groups:
            ret["security_groups"] = self.security_groups
        if self.provider:
            ret["provider"] = str(self.provider)
        return ret

    def __str__(self):
        return "%s: %s" % (self.name, self.info)

    def destroy(self):
        log.info("Destroying VM '%s'" % self._name)
        self._node.destroy()
        self._node = None
        self._driver = None


def _get_unused_name(used_names, prefix, random_suffix_length):
    random_part = ''.join(random.sample(string.ascii_lowercase, random_suffix_length))
    name = "%s-%s" % (prefix, random_part)
    while name in used_names:
        random_part = ''.join(random.sample(string.ascii_lowercase, random_suffix_length))
        name = "%s-%s" % (prefix, random_part)

    return name


def get_cloud_driver(provider, creds, region):
    driver_spec = _DriverSpec(provider, creds, region)
    if driver_spec in _DRIVERS:
        return _DRIVERS[driver_spec]

    if provider == Providers.AWS:
        EC2 = get_driver(Provider.EC2)
        driver = EC2(creds.key, creds.secret, region=region)

        # somehow driver.region is always None unless we set it explicitly
        driver.region = region
    elif provider == Providers.GCP:
        GCP = get_driver(Provider.GCE)
        driver = GCP(creds.SA_ID, creds.key_path, project=creds.project_ID, datacenter=region)
    else:
        raise ValueError("Unknown provider: %s" % provider)

    _DRIVERS[driver_spec] = driver

    return driver


def spawn_vm_in_aws(platform, aws_creds, key_pair, security_groups, region, name=None, size=None, role=None):
    driver = get_cloud_driver(Providers.AWS, aws_creds, region)
    existing_vms = driver.list_nodes()

    if name is None:
        name = _get_unused_name([vm.name for vm in existing_vms], platform, _NAME_RANDOM_PART_LENGTH)
    else:
        if any(vm.state in (0, 'running') and vm.name == name for vm in existing_vms):
            raise ValueError("VM with the name '%s' already exists" % name)

    aws_platform = aws_platforms[platform]
    size = size or aws_platform.get("xlsize") or aws_platform["size"]
    user = aws_platform.get("user")
    ami = aws_platform["ami"]

    log.info("Spawning new '%s' VM in AWS (AMI: %s, size=%s)" % (platform, ami, size))
    node = driver.create_node(
        name=name,
        image=NodeImage(id=ami, name=None, driver=driver),
        size=NodeSize(id=size, name=None, ram=None,
		      disk=None, bandwidth=None, price=None, driver=driver),
        ex_keyname=key_pair,
        ex_security_groups=security_groups,
        ex_metadata={
            "created-by": "cf-remote",
            "owner": whoami(),
        },
    )

    return VM(name, driver, node, role, platform, size, key_pair, security_groups, user, Providers.AWS)


def spawn_vm_in_gcp(platform, gcp_creds, region, name=None, size="n1-standard-1", network=None, public_ip=True, role=None):
    driver = get_cloud_driver(Providers.GCP, gcp_creds, region)
    existing_vms = driver.list_nodes()

    if name is None:
        name = _get_unused_name([vm.name for vm in existing_vms], platform, _NAME_RANDOM_PART_LENGTH)
    else:
        if any(vm.state in (0, 'running') and vm.name == name for vm in existing_vms):
            raise ValueError("VM with the name '%s' already exists" % name)

    # TODO: Should we have a list of GCP platforms/images? No weird IDs needed,
    #       they are straightforward like "centos-7" or "debian-9".
    kwargs = dict()
    if network is not None:
        net, subnet = network.split("/")
        kwargs["ex_network"] = net
        kwargs["ex_subnetwork"] = subnet
    if not public_ip:
        kwargs["external_ip"] = None
    kwargs["ex_metadata"] = {
        "created-by": "cf-remote",
        "owner": whoami(),
    }
    if not size:
        size = "n1-standard-1"

    node = driver.create_node(name, size, platform, **kwargs)
    return VM(name, driver, node, role, platform, size, None, None, None, Providers.GCP)


class GCPSpawnTask:
    def __init__(self, spawned_cb, *args, **kwargs):
        self._spawned_cb = spawned_cb
        self._args = args
        self._kwargs = kwargs
        self._vm = None
        self._errors = []

    def run(self):
        try:
            self._vm = spawn_vm_in_gcp(*self._args, **self._kwargs)
        except Exception as e:
            self._errors.append(e)
        else:
            self._spawned_cb(self._vm)

    @property
    def vm(self):
        return self._vm

    @property
    def errors(self):
        return self._errors


def spawn_vms(vm_requests, creds, region, key_pair=None, security_groups=None,
              provider=Providers.AWS, size=None, network=None,
              role=None, spawned_cb=None):
    if provider not in (Providers.AWS, Providers.GCP):
        raise ValueError("Unsupported provider %s" % provider)

    if (provider == Providers.AWS) and (key_pair is None):
        raise ValueError("key pair ID required for AWS")
    if (provider == Providers.AWS) and (security_groups is None):
        raise ValueError("security groups required for AWS")

    ret = []
    if provider == Providers.AWS:
        for req in vm_requests:
            vm = spawn_vm_in_aws(req.platform, creds, key_pair, security_groups,
                                 region, req.name, req.size, role)
            if spawned_cb is not None:
                spawned_cb(vm)
            ret.append(vm)
    else:
        tasks = [GCPSpawnTask(spawned_cb, req.platform, creds, region,
                              req.name, req.size, network, req.public_ip,
                              role)
                 for req in vm_requests]
        with Pool(len(vm_requests)) as pool:
            pool.map(lambda x: x.run(), tasks)
        for task in tasks:
            if task.vm is None:
                for error in task.errors:
                    log.error(str(error))
            else:
                ret.append(task.vm)

    return ret


def destroy_vms(vms):
    if not vms:
        return
    with Pool(len(vms)) as pool:
        pool.map(lambda vm: vm.destroy(), vms)


def dump_vms_info(vms):
    ret = {
        "meta": {
        }
    }
    duplicate_info_keys = []
    providers = {vm.provider for vm in vms}
    if (len(providers) == 1):
        ret["meta"]["provider"] = str(next(iter(providers)))
        duplicate_info_keys.append("provider")

    regions = {vm.region for vm in vms}
    if len(regions) == 1:
        ret["meta"]["region"] = next(iter(regions))
        duplicate_info_keys.append("region")

    for vm in vms:
        info = vm.info
        for key in duplicate_info_keys:
            del info[key]
        ret[vm.name] = info
    return ret
