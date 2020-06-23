====================
Inform Logging Tests
====================

These tests turn the model upside down a bit to make things easier.

Two bits are needed at the top of each .cf test file:

```cf3
body file control
{
  inputs => { "$(sys.policy_entry_dirname)/../common.cf.sub" };
}

bundle common testcase
{
  vars:
    "filename" string => "$(this.promise_filename)";
}

```

This is the boiler-plate to drive the tests.

The framework automatically cleans up `$(G.testfile)` and `$(G.testdir)`.

For the simplest case you can just include a `main` bundle to do the operation you want to perform.

```cf3
bundle agent main
{
  files:
    "$(G.testfile)"
      create => "true";
}
```

And when the test is run it will generate a `<test.cf>.actual` and check it against
a `<test.cf>.expected`.

There are temporary files created during the test involving the results.
If the test fails, these files will be kept, if the test passes, they will be removed.


The little framework also supports a `setup` and `teardown` optional bundle for things you might need in that regard.

Next to common.cf.sub there is a shell script called accept-actual.sh which will mv the .actual files to .expected and add them via `git`.
