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

### CFEngine code

Follow the defaults of the `cfengine.el` Emacs mode:

* one indent = 2 spaces
* comments inside bundles and bodies get 3 indents
* comments, promises and their attributes get 3 indents
* contexts/classes get 2 indents
* promise types get 1 indent
* everything else (bundle and body declarations and their braces, and top-level comments) gets 0 indents

Example:

```
body common control
{
      bundlesequence => { run };
}

bundle agent run
{
  vars:
      "time" int => now(),
      ifvarclass => "maybe";

  reports:
    cfengine::
      "time now $(now)";
}
```

### Automatic reindentation

You can run `contrib/reindent.pl FILE1.cf FILE2.c FILE3.h` to reindent
files, if you don't want to set up Emacs.  It will rewrite them with
the new indentation, using Emacs in batch mode.
