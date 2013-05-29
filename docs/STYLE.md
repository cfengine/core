# Style Guide

Please consider these style recommendations to make using CFEngine
more pleasant.

We strongly recommend that core contributions follow this style.

## Indentation settings

### General

* No TAB characters should be used for indentation
* The width of a TAB character is 4 spaces

### C code

See the `contrib/cfengine-code-style.el` library, which defines a CFEngine code style.

Example:

```
static void SourceSearchAndCopy(...)
{
    struct stat sb, dsb;
...
    if (maxrecurse == 0)        /* reached depth limit */
    {
        ...
        return;
    }
```

### CFEngine Policy

#### Style Summary
* one indent = 2 spaces
* avoid letting line length surpass 80 characters.
* vertically align opening and closing curly braces unless on same line
* promise type = 1 indent
* context class expression = 2 indents
* promiser = 3 indents
* promise attributes = 4 indents

#### Whitespace and Line Length

Spaces are preferred to tab characters. Lines should not have trailing whitespace. Generally line length should not surpass 80 characters. 

#### Curly brace alignment

Generally if opening and closing braces are not on a single line they should be aligned vertically.

Example:
```
bundle agent example
{
  vars:
      "people" slist => {
                          "Obi-Wan Kenobi",
                          "Luke Skywalker",
                          "Chewbacca",
                          "Yoda",
                          "Darth Vader",
                        };

      "cuddly" slist => { "Chewbacca", "Yoda" };
}
```

#### Promise types

Promise types should have 1 indent and each promise type after the first listed
should have a blank line before the next promise type.

This example illustrates the blank line before the "classes" type.

```
bundle agent example
{
  vars:
      "policyhost" string => "MyPolicyServerHostname";

  classes:
      "EL5" or => { "centos_5", "redhat_5" };
      "EL6" or => { "centos_6", "redhat_6" };
}
```

#### Context class expressions

Context class expressions should have 2 indents and each context class
expression after the first listed within a given promise type should have a
blank line preceding it.

This example illustrates the blank line before the second context class
expression (solaris) in the files type promise section:
```
bundle agent example
{
  files:
    any::
      "/var/cfengine/inputs/"
        copy_from    => update_policy( "/var/cfengine/masterfiles","$(policyhost)" ),
        classes      => policy_updated( "policy_updated" ),
        depth_search => recurse("inf");

    solaris::
      "/var/cfengine/inputs"
        copy_from => update_policy( "/var/cfengine/masterfiles", "$(policyhost" ),
        classes   => policy_updated( "policy_updated" );
}
```

#### Policy Comments

In-line policy comments are useful for debugging and explaining why something is
done a specific way. We encourage you to document your policy throughly.

Comments about general body and bundle behavior and parameters should be placed
after the body or bundle definition, before the opening curly brace and should
not be indented. Comments about specific promise behavior should be placed
before the promise at the same indention level as the promiser or on the same
line after the attribute.

```
bundle agent example(param1)
# This is an example bundle to illustrate comments
# param1 - string - 
{
  vars:
      "copy_of_param1" string => "$(param1)";

      "jedi" slist => { 
                        "Obi-Wan Kenobi",
                        "Luke Skywalker",
                        "Yoda",
                        "Darth Vader", # He used to be a Jedi, and since he
                                       # tossed the emperor into the Death
                                       # Star's reactor shaft we are including
                                       # him.
                      };
  classes:
      # Most of the time we don't need differentiation of redhat and centos
      "EL5" or => { "centos_5", "redhat_5" };
      "EL6" or => { "centos_6", "redhat_6" };
}
```

#### Hashrockets (=>)

Hash rockets should be aligned within a promise body scope and for grouped single line promises.

Example:
```
bundle agent example
{
  files:
    any::
      "/var/cfengine/inputs/"
        copy_from    => update_policy( "/var/cfengine/masterfiles","$(policyhost)" ),
        classes      => policy_updated( "policy_updated" ),
        depth_search => recurse("inf");

      "/var/cfengine/modules"
        copy_from => update_policy( "/var/cfengine/modules", "$(policyhost" ),
        classes   => policy_updated( "modules_updated" );

  classes:
      "EL5" or => { "centos_5", "redhat_5" };
      "EL6" or => { "centos_6", "redhat_6" };
}
```

### Automatic reindentation

You can run `contrib/reindent.pl FILE1.cf FILE2.c FILE3.h` to reindent
files, if you don't want to set up Emacs.  It will rewrite them with
the new indentation, using Emacs in batch mode.
