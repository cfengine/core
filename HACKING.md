How to contribute to CFEngine
=============================

Thanks for considering contributing to CFEngine! We take pull-requests
[on GitHub](https://github.com/cfengine/core/pulls) and we have a public
[bug-tracker](https://tracker.mender.io/projects/CFE/issues/). Discussion is taking place
on the [dev-cfengine](https://groups.google.com/forum/#!forum/dev-cfengine)
and [help-cfengine](https://groups.google.com/forum/#!forum/help-cfengine)
mailing lists. You'll find us chatting on Freenode's IRC channels
[#cfengine](https://webchat.freenode.net/?channels=cfengine&nick=) and
[#cfengine-dev](https://webchat.freenode.net/?channels=cfengine-dev&nick=).

Normally, bug fixes have a higher chance of getting accepted than new
features, but we certainly welcome feature contributions. If you have an idea
for a new feature, it might be a good idea to open up a feature ticket in
our [bug-tracker](https://tracker.mender.io/projects/CFE/issues/) and send a
message to dev-cfengine mailing list, before actually contributing the code,
in order to get discussion going.

Merged features and larger changes will be released in the first minor release
(i.e. x.y.0). Please note that such pull requests should be ready for merging
(i.e. adjusted for any feedback) at least two months before
[the scheduled release date](https://cfengine.com/product/supported-versions/)
in order to make it to the first minor release.


Top reasons pull-requests are rejected or delayed
-------------------------------------------------

* Code does not follow style guidlines. See [Coding Style](#coding-style).

* Pull request addresses several disparate issues. In general, smaller
pull-requests are better because they are easier to review and stay mergeable
longer.

* Big feature is added, but it is not configurable in compile-time.
We are striving to keep CFEngine lightweight and fast, so big new
features should be possible to disable with
`./configure --disable-feature` and linking to new libraries
should be optional with `./configure --without-libfoo`.

* Messy commit log. Tidy up the commit log by squashing commits.

* Missing ChangeLog description in commit message, which is mandatory
for new features or bugfixes to be accepted.
See [ChangeLog Entries](#changelog-entries) for details.

* Code is out-of-date, does not compile, or does not pass all tests. Again,
focused and small pull-requests are better.

* No attached test case. Normally, all new code needs test cases. This means a
functional test runnable with `make check`.


Code Overview
-------------

The CFEngine codebase can be usefully thought of as a few separate components:
utilities (libutils), parsing (libpromises), evaluation (libpromises),
actuation (mostly in cf-agent), network (libcfnet).

Over the past year, the structure of the codebase has undergone some
change. The goal of the restructuring is to isolate separate components with
explicit dependencies, and provide better unit test coverage.

For a general introduction to the tools, please read the man pages.

### libcompat

These are replacement functions in cases where autoconf cannot find a function
it expected to find on the platform. CFEngine takes an approach of relying on
the platform (typically POSIX) as much as possible, rather than creating its
own system abstraction layer.

### libutils

Contains generally useful datastructures or utilities. The key point about
*libutils* is that it is free of dependencies (except *libcompat*), so it does
not know about any CFEngine structures or global state found in *libpromises*.

- *sequence.h*: Loosely based on glib GSequence, essentially an array list.
- *map.h*: General purpose map (hash table).
- *set.h*: General purpose set, a wrapper of *Map*.
- *writer.h*: Stream writer abstraction over strings and FILEs.
- *xml_writer.h*: Utility for writing XML using a *Writer*.
- *csv_writer.h*: Utility for writing CSV using a *Writer*.
- *buffer.h*: Dynamic byte-array buffer.
- *json.h*: JSON document model, supports de/serialization.
- *string_lib.h*: General purpose string utilities.
- *logging.h*: Log functions, use Log() instead of printf.
- *man.h*: Utility for generating the man pages.
- *list.h*: General purpose linked list.
- *ip_address.h*: IP address parsing.
- *hashes.h*: Hashing functions.
- *file_lib.h*: General purpose file utilities.
- *misc_lib.h*: Really general utilities.

### libcfnet

Contains the networking layer for CFEngine. (At the time of writing, a bit of
a moving target).

### libpromises

This is the remainder of the old src directory, that which has not been
categorized. The roadmap for the project remains to leave *libpromises* as a
component for evaluation.

- *cf3.defs.h*: Contains structure definitions used widely.
- *eval_context.h*: Header for EvalContext, keeper of evaluation state.
- *dbm_api.h*: Local database for agents.
- *mod_.c*: Syntax definitions for all promise types (actuation modules).
- *syntax.h*: Syntax utilities and type checking.
- *files_.h": File utilities we haven't been able to decouple from evaluation.
- *locks.h*: Manages various persistent locks, kept in a local database.
- *rlist.h*: List for Rvals (of attributes).
- *expand.c*: Evaluates promises.
- *parser.h*: Parse a policy file.
- *policy.h*: Policy document object model, essentially the AST output of the
              parsing stage.
- *sysinfo.c*: Detects hard classes from the environment.
- *evalfunction.c*: Where all the built-in functions are implemented.
- *crypto.h*: Crypto utilities for some reason still tied to evaluation state.
- *generic_agent.h*: Common code for all agent binaries.

Things you should not use in *libpromises*

- *cf3.extern.h*: Remaining global variables.
- *prototypes3.h*: The original singular header file.
- *item_lib.h*: Item is a special purpose list that has been abused for
                unintended purposes.
- *assoc.h*: An lval-rval pair, deprecated in favor of *EvalContext*
             symbol table.
- *scope.h*: Old symbol table, this will move into *EvalContext*.

### cf-agent

The binary *cf-agent* contains most actuation logic in the `verify_*.h`
files. Each file more or less maps to a promise type.

As an example, the file `verify_packages.h` contains
`VerifyPackagesPromise(EvalContext *ctx, Promise *pp)`.

#### Lifecycle of cf-agent

The following outlines the normal execution of a *cf-agent* run.

1. Read options and gather these in GenericAgentConfig.
2. Create an EvalContext and call GenericAgentConfigApply(ctx, config).
3. Discover environment and set hard classes, apply to EvalContext.
4. Parse input policy file, get a Policy object.
5. Run static checks on Policy object.
6. Evaluate each *Bundle* in *bundlesequence*.
7. Write reports to disk.


#### Bootstrapping cf-agent

The following outlines the steps taken by agent during a successful bootstrap
to a policy server.

1. Remove all files in `inputs` directory
2. Write built-in `inputs/failsafe.cf`
3. Write policy server address or hostname, as was the argument
   to `--bootstrap` option, to `policy_server.dat`.
4. If the host was bootstrapped to the machine's own IP address, then it
   is a policy server, and the file `state/am_policy_hub` is touched as
   marker.
5. cf-agent runs using `failsafe.cf` as input file:
5a. Runs `cf-key` to generate `localhost.{priv,pub}` keys inside
    `ppkeys` directory.
5b. Fetches policy files from the policy server.
5c. Starts `cf-execd`
5d. Runs `cf-agent -f update.cf`
6. Agent finishes.
7. `cf-execd` continues to run `cf-agent` periodically with policy
   from `inputs` directory.

### cf-monitord

Monitoring probes are contained in `mon_*.c` files. These all have a common
header file `mon.h`.



Coding Style
------------

* Loosely based on Allman-4 and the
  [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
* Keep in mind that code should be readable by non C experts.
  If you are a Guru, try to restrain yourself, only do magic when
  absolutely necessary.
* 4 spaces indentation level, no tabs.
* Function names are `CamelCase` (with first letter capital), variables and
  parameters are `under_scored`.
  * If you introduce a new namespace, you can use underscore as
    namespace-identifier separator, for example
    `StrList_BinarySearch()`.
  * Avoid introducing extra long identifiers, like
    ~~`GenericAgentConfigParseWarningOptions()`~~.
* Try not to include assignments inside if/while expressions
  *unless they avoid great repetition*. On the average case,
  just put the assignment on the previous line. So try NOT to do
  the following:
  ~~`if ((ret = open(...)) == -1)`~~
* Explicit comparisons are better than implicit, i.e. prefer writing
  `if (number == 0)` or `if (pointer == NULL)`
  instead of ~~`if (!number)`~~ or ~~`if (!pointer)`~~. It only makes
  sense to test booleans directly, for example `if (is_valid)` is good.
  Furthermore have the literal last in the comparison, not first, i.e.
  prefer writing `if (open(...) == -1)` instead of
  ~~`if (-1 == open(...))`~~.
* Control statements need to have braces on separate line,
  no matter how simple they are.
  * Caution, do-while loops should have the closing brace at the same
    line with while, so that it can't be confused with empty while statement.
    ```c
    do
    {
        // ...

    } while (condition);
    ```
* In functions which can fail, error code (int) or success/failure (bool) should be returned.
    * `true`(bool) and `0`(int) should always signify success.
    * Only return an error code (int) when there are multiple different return values for different errors. If a function can only return `0` (success) or `-1` (error) use `bool` instead.
    * Compiler can enforce checking of return value, output of function can be in an output parameter (pointer).
* *C99 is encouraged in the language, use it.*

  As for using C99-specific libc functions, you can mostly use them,
  because we provide replacement functions in libcompat, since many old
  Unix platforms are missing those. If there is no replacement for a
  C99-specific function, then either stick to C89, or write the
  libcompat replacement.

  Current functions known to be missing from libcompat
  (so stick to C89):
  * `[s]scanf()`
* Fold new code at 78 columns.
* Do not break string literals. Prefer having strings on a single line
  In order to improve grep-ability. If they do not fit on a single line,
  try breaking on punctuation. In worst case scenario, you are allowed
  to surpass the 78 columns limit.

  Bad:
  ```c
  Log(LOG_LEVEL_INFO, "Some error occurred while reading installed "
      "packages cache.");
  ```
  Good:
  ```c
  Log(LOG_LEVEL_INFO,
      "Some error occurred while reading installed packages cache");
  ```
* Always use typedefs, no `struct X`, or `enum Y` are allowed. Types
  defined with typedef should be in camelcase and no trailing `_t`,
  `_f` etc.
* Constify what can be `const`. Minimize use of global variables.
  Never declare a global variable in a library (e.g. libpromises) and
  change it in the programs.
* Don't use `static` variables that change, since they are not thread-safe.
* Sizes of stack-allocated buffers should be deduced using `sizeof()`.
  Never hard-code the size (like `CF_BUFSIZE`).
* Avoid using type casts, unless absolutely necessary. Usually a compiler
  warning is better satisfied with correct code rather than using a type cast.
  * Type casts should be separated with one space from the variable,
    for example ```(struct sockaddr *) &myaddr```.
* Avoid pointless initialisation of variables, because they
  silence important compiler warnings. Only initialise variables
  when there is a reason to do so.
* Document using Doxygen (within reason), preferably in the `.c` files,
  not the header files.
* Read
  [Linux Kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html) and
  [libabc coding style](https://git.kernel.org/cgit/linux/kernel/git/kay/libabc.git/plain/README).
  They contain many good practices.
* http://en.wikipedia.org/wiki/Golden_Rule


C Platform Macros
-----------------

It's important to have portability in a consistent way. In general we
use *autoconf* to test for features (like system headers, defines,
specific functions). So try to use the autoconf macros `HAVE_DECL_X`,
`HAVE_STRUCT_Y`, `HAVE_MYFUNCTION` etc.  See the
[autoconf manual existing tests section](https://www.gnu.org/software/autoconf/manual/html_node/Existing-Tests.html).

It is preferable to write feature-specific ifdefs, instead of
OS-specific, but it's not always easy. If necessary use these
platform-specific macros in C code:

* Any Windows system: Use `_WIN32`.  Don't use `NT`.
* mingw-based Win32 build: Use `__MINGW32__`.  Don't use `MINGW`.
* Cygwin-based Win32 build: Use `__CYGWIN__`.  Don't use `CFCYG`.
* OS X: Use `__APPLE__`.  Don't use `DARWIN`.
* FreeBSD: Use `__FreeBSD__`.  Don't use `FREEBSD`.
* NetBSD: Use `__NetBSD__`.  Don't use `NETBSD`.
* OpenBSD: Use `__OpenBSD__`.  Don't use `OPENBSD`.
* AIX: Use `_AIX`.  Don't use `AIX`.
* Solaris: Use `__sun`. Don't use `SOLARIS`.
* Linux: Use `__linux__`.  Don't use `LINUX`.
* HP/UX: Use `__hpux` (two underscores!).  Don't use `hpux`.

Finally, it's best to avoid polluting the code logic with many ifdefs.
Try restricting ifdefs in the header files, or in the beginning of
the C files.


Output Message, Logging Conventions
-----------------------------------

CFEngine outputs messages about what its doing using the `Log()` function. It
takes a `LogLevel` enum mapping closely to syslog priorities. Please try to do
the following when writing output messages.

* Log levels
  * `LOG_LEVEL_CRIT` For critical errors, process exits immediately.
  * `LOG_LEVEL_ERR`: For cf-agent, promise failed. For cf-serverd,
    some system error occurred that is worth logging to syslog.
  * `LOG_LEVEL_NOTICE`: Important information (not errors) that must not
    be missed by the user. For example cf-agent uses it in files promises
    when change tracking is enabled and the file changes.
  * `LOG_LEVEL_INFO`: For cf-agent, changes that the agent performs
    to the system, for example when a promise has been repaired. For
    cf-serverd, `access_rules` denials for connected clients.
  * `LOG_LEVEL_VERBOSE` :: Log *human readable* progress info useful to
    users (i.e. sysadmins). Also errors that are unimportant or expected
    in certain cases.
  * `LOG_LEVEL_DEBUG`: Log anything else (for example various progress info).
    Try to avoid "Entering function Foo()", but rather use for
    "While copying, got reply '%s' from server".

* Do not decorate with symbols or indentation in messages and do not
  terminate the message with punctuation. Let `Log()` enforce the common
  formatting rules.

* When quoting strings, use single quotes, e.g. "Some stuff '%s' happened in
  '%s'.

* Keep in mind context, e.g. write "While copying, insufficient permissions"
  rather than "Insufficient permissions".

* Use output sparingly, and use levels appropriately. Verbose logging tends to
  get very verbose.

* Use platform-independent `GetErrorStr()` for `strerror(errno)`.  Write
  for example
  `Log(LOG_LEVEL_ERR, "Failed to open ... (fopen: %s)", GetErrorStr());`

* Normally, try to keep each message to one line of output, produced
  by one call to `Log()`.

* Normally, do not circumvent `Log()` by writing to stdout or stderr.


Testing
-------

It is extremely important to have automated tests for all code, and normally
all new code should be covered by tests, though sometimes it can be hard to
mock up the environment.

There are two types of tests in CFEngine. *Unit tests* are generally
preferable to *acceptance tests* because they are more targeted and take less
time to run. Most tests can be run using `make check`.
See [Unsafe Tests](#unsafe-tests) below.

* *Unit tests*. Unit tests are a great way of testing some new module (header
  file). Ideally, the new functionality is written so that the environment can
  be easily injected and results readily verified.

* *Acceptance tests*. These are tests that run *cf-agent* on a policy file
  that contains *test* and *check* bundles, i.e. it uses CFEngine to both make
  a change and check it. See also script tests/acceptance/testall.

Tip: In order to trigger assert() calls in the code, build with
`--enable-debug` (passed to either `./autogen.sh` or `./configure`). If you get
very large binary sizes you can also pass `CFLAGS='-g -O0'` to reduce that.


Unsafe Tests
------------

Note that some acceptance tests are considered to be unsafe because they
modify the system they are running on. One example is the tests for the
"users" promise type, which does real manipulation of the user database on the
system.  Due to their potential to do damage to the host system, these tests
are not run unless explicitly asked for.  Normally, this is something you
would want to do in a VM, so you can restore the OS to a pristine state
afterwards.

To run all tests, including the unsafe ones, you either need to be logged in as
root or have "sudo" configured to not ask for a password. Then run the
following:

    $ UNSAFE_TESTS=1 GAINROOT=sudo make check

Again: DO NOT do this on your main computer! Always use a test machine,
preferable in a VM.


Emacs users
-----------

There is Elisp snippet in contrib/cfengine-code-style.el which defines the
project's coding style. Please use it when working with source code. The
easiest way to do so is to add

    (add-to-list 'load-path "<core checkout directory>/contrib")
    (require 'cfengine-code-style)

and run

    ln -s contrib/dir-locals.el .dir-locals.el

in the top directory of the source code checkout.


ChangeLog Entries
-----------------

When a new feature or a bugfix is being merged, it is often necessary to be
accompanied by a proper entry in the ChangeLog file. Besides manually editing
the file, we have an automatic way of generating them before the release,
by properly formatting *commit messages*
(see [git-commit-template](misc/githooks/git-commit-template)). Keep in mind
that changelog entries should be written in a way that is understandable by non-
programmers. This means that references to implementation details are not
appropriate, leave this for the non-changelog part of the commit message. It is
the behavior change which is important. This implies that refactorings that have
no visible effect on behavior don't need a changelog entry.

If a changelog entry is needed, your pull request should have at least one
commit either with a "Changelog:" line in it (anywhere after the title), or
title should start with ticket number from our bug tracker ("CFE-1234").
"Changelog:" line may be one of the following:

* To write arbitrary message in the ChangeLog:
`Changelog: <message>`
* To use the commit title line in the ChangeLog:
`Changelog: Title`
* To use the entire commit message in the ChangeLog:
`Changelog: Commit`

It's worth noting that we strive to have bugtracker tickets for most
changes, and they should be mentioned in the ChangeLog entries. In fact
if anywhere in the commit message the string ```CFE-1234``` is found
(referring to a ticket from https://tracker.mender.io ), it will be
automatically added to the ChangeLog.
