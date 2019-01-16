# cf-remote

Simple example:
```
$ cf-remote info -H 192.168.100.90
```

## Ideas

Useful example:

```
$ cf-remote install --hub 192.168.100.90 --client 192.168.100.91 --bootstrap 192.168.100.90
```

`--bootstrap` has an optional argument in case you want to bootstrap to a different IP than `hub`.
For example, in AWS you should bootstrap to the internal IP.

Ideas for commands:
```
$ cf-remote install
$ cf-remote status
$ cf-remote info
$ cf-remote vars
$ cf-remote classes
$ cf-remote execute
$ cf-remote restart
$ cf-remote uninstall
$ cf-remote help
$ cf-remote version
$ cf-remote --version
$ cf-remote --help
```
