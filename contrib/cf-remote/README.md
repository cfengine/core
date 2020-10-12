# cf-remote

## Installation

cf-remote requires python 3 with fabric and requests.
(Install using brew/apt/yum/pip).
It's a _normal_ python package, but doesn't have a setup script (yet).

Clone `cfengine/core` repo where you want, and then symlink, like this:

```
$ git clone https://github.com/cfengine/core.git
$ ln -s `pwd`/core/contrib/cf-remote/cf_remote/__main__.py /usr/local/bin/cf-remote
```

Install dependencies:

```
$ cd core/contrib/cf-remote/
$ pip3 install -r requirements.txt
```

Check that it worked:

```
$ ls -al `which cf-remote`
lrwxr-xr-x  1 olehermanse  admin  68 Jan 10 15:20 /usr/local/bin/cf-remote -> /northern.tech/cfengine/core/contrib/cf-remote/cf_remote/__main__.py
$ cf-remote --version
cf-remote version 0.1 (BETA)
Available CFEngine versions:
3.13.0, 3.12.1, 3.12.0, 3.10.5, 3.10.4, 3.10.3, 3.10.2, 3.10.1, 3.10.0
```

Note that the version/branch of core you have checked out is important!
You can also move it out of your core folder if you prefer this (but then making changes to cf-remote becomes more tedious).

## Examples

### See information about remote host

The `info` command can be used to check basic information about a system.

```
$ cf-remote info -H 34.241.203.218

ubuntu@34.241.203.218
OS            : ubuntu (debian)
Architecture  : x86_64
CFEngine      : 3.12.1
Policy server : 172.31.42.192
Binaries      : dpkg, apt
```

(You must have ssh access).

### Installing and bootstrapping CFEngine Enterprise Hub

The `install` command can automatically download and install packages as well as bootstrap both hubs and clients.

```
cf-remote install --hub 34.247.181.100 --bootstrap 172.31.44.146 --demo

ubuntu@34.247.181.100
OS            : ubuntu (debian)
Architecture  : x86_64
CFEngine      : Not installed
Policy server : None
Binaries      : dpkg, apt

Package already downloaded: '/Users/olehermanse/.cfengine/cf-remote/packages/cfengine-nova-hub_3.12.1-1_amd64.deb'
Copying: '/Users/olehermanse/.cfengine/cf-remote/packages/cfengine-nova-hub_3.12.1-1_amd64.deb' to '34.247.181.100'
Installing: 'cfengine-nova-hub_3.12.1-1_amd64.deb' on '34.247.181.100'
CFEngine 3.12.1 was successfully installed on '34.247.181.100'
Bootstrapping: '34.247.181.100' -> '172.31.44.146'
Bootstrap successful: '34.247.181.100' -> '172.31.44.146'
Transferring def.json to hub: '34.247.181.100'
Copying: '/Users/olehermanse/.cfengine/cf-remote/json/def.json' to '34.247.181.100'
Triggering an agent run on: '34.247.181.100'
Disabling password change on hub: '34.247.181.100'
Triggering an agent run on: '34.247.181.100'
Your demo hub is ready: https://34.247.181.100/ (Username: admin, Password: password)
```

Note that this demo setup (`--demo`) is notoriously insecure.
It has default passwords and open access controls.
Don't use it in a production environment.

### Specify an SSH key

If you have more than one key in `~/.ssh` you may need to specify which key `cf-remote` is to use.

```
$ export CF_REMOTE_SSH_KEY="~/.ssh/id_rsa.pub"
```

## Contribute

Feel free to open pull requests to expand this documentation, add features or fix problems.
You can also pick up an existing task or file an issue in [our bug tracker](https://tracker.mender.io/issues/?filter=11711).
`cf-remote` is a part of the [cfengine community repo](https://github.com/cfengine/core) and has the same license (GPLv3).
