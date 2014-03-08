# cf-locate

This is a small perl script to help you locate and display bodies or bundles inside of your masterfiles.  It uses ANSI color sequences.

It takes optional `-f` or `--full`; or `-p` or `--plain` flags first,
then a pattern, then a list of directories.

With `-f` specified, the whole body or bundle will be displayed.

With `-p` specified, no color will be used, and the informational `->
... found` message will be omitted.

With `-h` specified, help will be displayed and then the script will exit.

With no directories specified, `/var/cfengine/masterfiles` will be searched.

The `-c` flag lets you configure colors (it can be specified multiple
times).  The defaults are:

     info => 'green',
     heading => 'red',
     body => 'yellow',

So for example, `-c body=magenta` will print the definition bodies in
magenta.  We're not saying it's the *right* color, mind you.  We like
black on black 'coz it's slimming.

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
