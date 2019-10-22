import string
import random
import os
from collections import namedtuple
from enum import Enum

from libcloud.compute.types import Provider
from libcloud.compute.providers import get_driver
from libcloud.compute.base import NodeSize, NodeImage

from cf_remote.cloud_data import aws_platforms
from cf_remote import log


_NAME_RANDOM_PART_LENGTH = 4

AWSCredentials = namedtuple("AWSCredentials", ["key", "secret"])

VMRequest = namedtuple("VMRequest", ["platform", "name", "size"])

_DriverSpec = namedtuple("_DriverSpec", ["provider", "creds", "region"])

# _DriverSpec -> libcloud.providers.get_driver()
_DRIVERS = dict()


class Providers(Enum):
    AWS = 1


class VM:
    def __init__(self, name, driver, node, role=None,
                 platform=None, size=None, key_pair=None, security_groups=None):
        self._name = name
        self._driver = driver
        self._node = node
        self._platform = platform
        self._size = size
        self._key_pair = key_pair
        self._sec_groups = security_groups
        self.role = role

    @classmethod
    def get_by_ip(cls, ip, driver=None):
        if driver is None:
            if len(_DRIVERS.keys()) == 1:
                driver = _DRIVERS.items()[0]
            else:
                print("Don't know which driver to use: %s" % _DRIVERS.keys())
                return None

        for node in driver.list_nodes():
            if node.state in (0, 'running') and (ip in node.public_ips or ip in node.private_ips):
                return cls(node.name, driver, node)
        return None

    @classmethod
    def get_by_name(cls, name, driver=None):
        if driver is None:
            if len(_DRIVERS.keys()) == 1:
                driver = _DRIVERS.items()[0]
            else:
                print("Don't know which driver to use: %s" % _DRIVERS.keys())
                return None

        for node in driver.list_nodes():
            if node.state in (0, 'running') and node.name == name:
                return cls(node.name, driver, node)
        return None

    @classmethod
    def get_by_uuid(cls, uuid, driver=None):
        if driver is None:
            if len(_DRIVERS.keys()) == 1:
                driver = _DRIVERS.items()[0]
            else:
                print("Don't know which driver to use: %s" % _DRIVERS.keys())
                return None

        for node in driver.list_nodes():
            if node.uuid == uuid:
                return cls(node.name, driver, node)
        return None


    @classmethod
    def get_by_info(cls, driver, vm_info):
        for node in driver.list_nodes():
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
        return self._driver.region

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
    def _data(self):
        # we need to refresh this every time because libcloud's drivers seem to
        # be returning just snapshots of info (IOW, things are not updated)
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
        if self.role:
            ret["role"] = self.role
        if self.key_pair:
            ret["key_pair"] = self.key_pair
        if self.security_groups:
            ret["security_groups"] = self.security_groups
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
        driver = _DRIVERS[driver_spec]
    else:
        cls = get_driver(Provider.EC2)
        driver = cls(creds.key, creds.secret, region=region)

        # somehow driver.region is always None unless we set it explicitly
        driver.region = region

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
    size = size or aws_platform["size"]
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
            "owner": os.getlogin(),
        }
    )

    return VM(name, driver, node, role, platform, size, key_pair, security_groups)


def spawn_vms(vm_requests, creds, region, key_pair=None, security_groups=None, provider=Providers.AWS, role=None):
    # TODO: support other providers
    if provider != Providers.AWS:
        raise ValueError("Unsupported provider %s" % provider)
    if key_pair is None:
        raise ValueError("key pair ID required for AWS")
    if security_groups is None:
        raise ValueError("security groups required for AWS")

    ret = []
    for req in vm_requests:
        vm = spawn_vm_in_aws(req.platform, creds, key_pair, security_groups,
                             region, req.name, req.size, role)
        ret.append(vm)
    return ret


def destroy_vms(vms):
    for vm in vms:
        vm.destroy()

def dump_vms_info(vms):
    ret = {
        "meta": {
            "provider": "aws",
        }
    }
    regions = {vm.region for vm in vms}
    if len(regions) == 1:
        ret["meta"]["region"] = next(iter(regions))
        for vm in vms:
            info = vm.info
            del info["region"]
            ret[vm.name] = info
    else:
        for vm in vms:
            ret[vm.name] = vm.info
    return ret
