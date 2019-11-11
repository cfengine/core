# BSD building/testing instructions

BSD is not officially supported.
These are some basic instructions for getting CFEngine Community to build on FreeBSD.
Tested on FreeBSD 11 VM in AWS EC2.

## Dependencies

```
$ su root -c 'pkg update -f'
$ su root -c 'pkg install -y gdb gcc gmake lmdb autoconf automake libtool git python3'
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
$ git fetch origin pull/3589/head:bsd-branch
$ git checkout bsd-branch
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
$ ifconfig
lo0: flags=8049<UP,LOOPBACK,RUNNING,MULTICAST> metric 0 mtu 16384
	options=680003<RXCSUM,TXCSUM,LINKSTATE,RXCSUM_IPV6,TXCSUM_IPV6>
	inet6 ::1 prefixlen 128
	inet6 fe80::1%lo0 prefixlen 64 scopeid 0x1
	inet 127.0.0.1 netmask 0xff000000
	nd6 options=21<PERFORMNUD,AUTO_LINKLOCAL>
	groups: lo
xn0: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> metric 0 mtu 9001
	options=503<RXCSUM,TXCSUM,TSO4,LRO>
	ether 0a:60:8e:93:94:90
	hwaddr 0a:60:8e:93:94:90
	inet6 fe80::860:8eff:fe93:9490%xn0 prefixlen 64 scopeid 0x2
	inet 172.31.42.230 netmask 0xfffff000 broadcast 172.31.47.255
	nd6 options=23<PERFORMNUD,ACCEPT_RTADV,AUTO_LINKLOCAL>
	media: Ethernet manual
	status: active
$ su root -c '/var/cfengine/bin/cf-key'
$ su root -c '/var/cfengine/bin/cf-agent -B 172.31.42.230'
R: Bootstrapping from host '172.31.42.230' via built-in policy '/var/cfengine/inputs/failsafe.cf'
R: This host assumes the role of policy server
R: Updated local policy from policy server
R: Triggered an initial run of the policy
R: Started the server
R: Started the scheduler
  notice: Bootstrap to '172.31.42.230' completed successfully!
$
```
