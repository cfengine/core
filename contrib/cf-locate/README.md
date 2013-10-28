# cf-locate

This is a small perl script to help you locate and display bodies or bundles inside of your masterfiles.  It uses ANSI color sequences.

It takes an optional `-f` or `--full` flag first, then a pattern, then a list of directories.

With `-f` specified, the whole body or bundle will be displayed.

## Example

```
cf-locate always /var/cfengine/masterfiles
```

```
-> body or bundle matching 'always' found in /var/cfengine/masterfiles/lib/3.6/common.cf:260
body classes always(x)
```

```
cf-locate -f always /var/cfengine/masterfiles
```

```
-> body or bundle matching 'always' found in /var/cfengine/masterfiles/lib/3.6/common.cf:260
body classes always(x)
# Define a class no matter what the outcome of the promise is

{
      promise_repaired => { "$(x)" };
      promise_kept => { "$(x)" };
      repair_failed => { "$(x)" };
      repair_denied => { "$(x)" };
      repair_timeout => { "$(x)" };
}
```
