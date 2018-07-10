How to contribute to CFEngine
=============================

Thanks for considering contributing to the CFEngine! We take pull-requests on
GitHub at http://github.com/cfengine, and we have a public Redmine bug-tracker
at http://bug.cfengine.com

Normally, bug fixes have a higher chance of getting accepted than new
features, but we certainly welcome feature contributions. If you have an idea
for a new feature, it might be a good idea to open up a feature ticket in
Redmine first to get discussion going.



Top reasons pull-requests are rejected or delayed
-------------------------------------------------

* Code does not follow style guidlines. (See section on Coding Style)

* Pull request addresses several disparate issues. In general, smaller
pull-requests are better because they are easier to review and stay mergeable
longer.

* Messy commit log. Tidy up the commit log by squashing commits. Write good
commit messages: One line summary at the top, followed by an optional
detailing paragraphs. Please reference Redmine tickets, e.g. "Close #1234"

* Code is out-of-date, does not compile, or does not pass all tests. Again,
focused and small pull-requests are better.

* No attached test case. Normally, all new code needs test cases. This means a
functional test runnable with 'make check'.


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

The binary *cf-agent* and contains most actuation logic in the *verify_.h*
files. Each file more or less maps to a promise type.

As an example, the file *verify_packages.h* contains
*VerifyPackagesPromise(EvalContext *ctx, Promise *pp)*.

### cf-monitord

Monitoring probes are contained in *mon_.c* files. These all have a common
header file *mon.h*.


Lifecycle of cf-agent
---------------------

The following outlines the normal execution of a *cf-agent* run.

1. Read options and gather these in GenericAgentConfig.
2. Create an EvalContext and call GenericAgentConfigApply(ctx, config).
3. Discover environment and set hard classes, apply to EvalContext.
4. Parse input policy file, get a Policy object.
5. Run static checks on Policy object.
6. Evaluate each *Bundle* in *bundlesequence*.
7. Write reports to disk.


Bootstrapping cf-agent
----------------------

The following outlines the steps taken by agent during a successful bootstrap
to a policy server.

1. Resolve IP of policy server.
2. Remove existing files in outputs.
3. Write built-in failsafe.cf to outputs.
4. Write IP of policy server to policy_server.dat, optionally also
   marker file am_policy_server.
5. Proceed using failsafe.cf as input file.
5a. Evaluating failsafe.cf, fetches policy files from the policy server.
5b. Evaluating failsafe.cf, starts *cf-execd*.
6. Agent finishes.
7. *cf-execd* continues to run *cf-agent* periodically with policy
   from */inputs*.


Coding Style
------------

* Loosely based on Allman-4 and the Google C++ Style Guide
  (http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml).
  Function names are CamelCase (with first letter capital), variable and
  parameters are under_scored.

  * Caution, do-while loops should have the closing brace at the same
    line with while, so that it can't be confused with empty while statement.

    ```
    do
    {
        /* ... */
    } while (condition);
    ```

* Read
  https://git.kernel.org/cgit/linux/kernel/git/kay/libabc.git/plain/README
  It contains many good practices not only suitable for library writers.

* C99 is encouraged in the language, use it.

* As for using C99-specific libc functions, you can mostly use them,
  because we provide replacement functions in libcompat, since many old
  Unix platforms are missing those. If there is no replacement for a
  C99-specific function, then either stick to C89, or write the
  libcompat replacement.

  Current functions known to be missing from libcompat (so stick to
  C89):

  * [s]scanf

* Control statements need to have braces, no matter how simple they are.

* 4 spaces indentation level, no tabs.

* Always use typedefs, no "struct X", or "enum Y" are allowed. Types
  defined with typedef should be in camelcase and no trailing "_t",
  "_f" etc.

* Constify what can be. Don't use global variables.

* Keep tidy header files and document using Doxygen (within reason).

* http://en.wikipedia.org/wiki/Golden_Rule


C Platform Macros
-----------------

It's important to have portability in a consistent way.  Use these platform
macros in C code.

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

Output Message Conventions
--------------------------

CFEngine outputs messages about what its doing using the *Log* function. It
takes a *LogLevel* enum mapping closely to syslog priorities. Please try to do
the following when writing output messages.

* Do not decorate with ornamental symbols or indentation in messages. Leave
  formatting to *Log*.

* When quoting strings, use single quotes, e.g. "Some stuff '%s' happened in
  '%s'.

* Keep in mind context, e.g. write "While copying, insufficient permissions"
  rather than "Insufficient permissions".

* Use output sparingly, and use levels appropriately. Verbose logging tends to
  get very verbose.

* Use platform-independent *GetErrorStr* for strerror(errno).  Write
  e.g. Log(LOG_LEVEL_ERR, "Failed to open ... (fopen: %s)", GetErrorStr())

* Normally, try to keep each message to one line of output, produced
  by one call to *Log*.

* Normally, do not circumvent *Log* by writing to stdout or stderr.

* Use LOG_LEVEL_DEBUG for environmental situations, e.g. don't write "Entering
  function Foo()", but rather "While copying, got reply '%s' from server".


Testing
-------

It is extremely important to have automated tests for all code, and normally
all new code should be covered by tests, though sometimes it can be hard to
mock up the environment.

There are two types of tests in CFEngine. *Unit tests* are generally
preferable to *acceptance tests* because they are more targeted and take less
time to run. Most tests can be run using *make check* (see Unsafe tests
below).

* *Unit tests*. Unit tests are a great way of testing some new module (header
  file). Ideally, the new functionality is written so that the environment can
  be easily injected and results readily verified.

* *Acceptance tests*. These are tests that run *cf-agent* on a policy file
  that contains *test* and *check* bundles, i.e. it uses CFEngine to both make
  a change and check it. See also script tests/acceptance/testall.


Unsafe tests
------------

Note that some acceptance tests are considered to be unsafe because they
modify the system they are running on. One example is the tests for the
"users" promise type, which does real manipulation of the user database on the
system.  Due to their potential to do damage to the host system, these tests
are not run unless explicitly asked for.  Normally, this is something you
would want to do in a VM, so you can restore the OS to a pristine state
afterwards.

To run all tests, including the unsafe ones, you either need to logged in as
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

atexit() and Windows
--------------------

On Windows the atexit function works but the functions registered there are
executed after or concurrently with DLL unloading. If registered functions
rely on DLLs such as pthreads to do locking/unlocking deadlock scenarios can
occur when exit is called. 

In order to make behavior more explicit and predictable we migrated to always
using a homegrown atexit system. RegisterAtExitFunction instead of atexit and
CallAtExitFunctionsAndExit instead of exit.

If _Exit or _exit need to be called that is fine as they don't call atexit or
cleanup functions.
