# BSD building/testing instructions

BSD is not officially supported.
These are some basic instructions for getting CFEngine Community to build on FreeBSD.
Tested on FreeBSD 12.1 VM in AWS EC2.

## Dependencies

```
$ su root -c 'pkg update -f && pkg install -y gdb gcc gmake lmdb autoconf automake libtool git python3 emacs-nox'
```

## Download

```
$ git clone --recursive https://github.com/cfengine/core.git
$ git clone --recursive https://github.com/cfengine/masterfiles.git
```

## Install masterfiles

```
$ (cd masterfiles && ./autogen.sh && su root -c "gmake install")
```

## Check out PR (Optional)

To check out PR `#3589` to branch `bsd-branch`, do:

```
$ cd core
$ git fetch origin pull/3589/head:bsd-branch && git checkout bsd-branch
```

### Rebase PR branch (Optional)

```
$ git config --global user.email "A" && git config --global user.name "A"
$ git rebase master
```

## Build

Within `core` directory:

```
$ ./autogen.sh --enable-debug -C --with-lmdb=/usr/local/ --with-pcre=/usr/local/
$ gmake -j8
```

## Install

```
$ su root -c 'gmake install'
```

## Bootstrap

Example bootstrap, using IP from `ifconfig`:

```
$ su root -c '/var/cfengine/bin/cf-key'
$ ifconfig | grep inet
	inet6 ::1 prefixlen 128
	inet6 fe80::1%lo0 prefixlen 64 scopeid 0x1
	inet 127.0.0.1 netmask 0xff000000
	inet6 fe80::8f7:3cff:fe7d:5e7c%xn0 prefixlen 64 scopeid 0x2
	inet 172.31.45.172 netmask 0xfffff000 broadcast 172.31.47.255
$ su root -c '/var/cfengine/bin/cf-agent -B 172.31.45.172'
R: Bootstrapping from host '172.31.42.230' via built-in policy '/var/cfengine/inputs/failsafe.cf'
R: This host assumes the role of policy server
R: Updated local policy from policy server
R: Triggered an initial run of the policy
R: Started the server
R: Started the scheduler
  notice: Bootstrap to '172.31.42.230' completed successfully!
$
```
