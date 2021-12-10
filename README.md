[![Gitter chat](https://badges.gitter.im/cfengine/core.png)](https://gitter.im/cfengine/core)

[![Language grade: C](https://img.shields.io/lgtm/grade/cpp/g/cfengine/core.svg?logo=lgtm&logoWidth=18&label=code%20quality)](https://lgtm.com/projects/g/cfengine/core/)

# CFEngine 3

CFEngine 3 is a popular open source configuration management system. Its primary
function is to provide automated configuration and maintenance of large-scale
computer systems.

## Source code repositories

CFEngine is comprised of several source code repositories.
As you might be looking for another part of the open source code base, here is a list to ease navigation:

* [core](https://github.com/cfengine/core) (This repo) - The C source code for core components, like cf-agent and cf-serverd.
  * [libntech](https://github.com/cfengine/libntech) (submodule in core) - Library of reusable C code, such as data structures, string manipulation, JSON parsing, file handling, etc.
  * [core/contrib](https://github.com/cfengine/core/tree/master/contrib) (subdirectory in core) - User-contributed tools and scripts
* [masterfiles](https://github.com/cfengine/masterfiles) - The Masterfiles Policy Framework (MPF) contains the default policy (.cf) files
* [documentation](https://github.com/cfengine/documentation) - Documentation on how CFEngine components work, the policy language, the enterprise features, etc.
* [cf-remote](https://github.com/cfengine/cf-remote) - Tooling to make deploying / testing CFEngine across many remote instances easy
* [buildscripts](https://github.com/cfengine/buildscripts) - Scripts and files needed to build installer packages across a wide variety of supported platforms

(Each repo also contains some supporting code/files, such as tests, scripts, documentation, etc.).

## Installation

Pre-built installers are available from our website:

* [Download CFEngine Enterprise Installers](https://cfengine.com/product/cfengine-enterprise-free-25/)
* [Download CFEngine Community Installers](https://cfengine.com/product/community/)

To install from source please see
the [INSTALL](https://github.com/cfengine/core/blob/master/INSTALL) file for
prerequisites and build instructions.

## License

As per the [LICENSE](https://github.com/cfengine/core/blob/master/LICENSE) file,
CFEngine Community is licensed under the GNU General Public License, version 3.

All the files in this repository are licensed under GNU GPL version 3, unless
stated otherwise in the copyright notice inside the particular file.

## Example Usage

### Hello World

The following code demonstrates simple CFEngine output through a reports promise.

    body common control
    {
      bundlesequence => { "run" };
    }

    bundle agent run
    {
      reports:
        cfengine::
          "Hello, world";
    }

The following policy code may be executed with cf-agent (the main CFEngine binary) as follows.

    $ cf-agent/cf-agent hello.cf
    R: Hello, world

## Contributing

Please see the [CONTRIBUTING.md](https://github.com/cfengine/core/blob/master/CONTRIBUTING.md) file.

## Relationship to CFEngine 2

CFEngine 3 is *not* a drop-in upgrade for CFEngine 2 installations.  It is a
significantly more powerful version, but it is incompatible with the CFEngine 2
policy language.

The server part of CFEngine 3 supports the network protocol of CFEngine 2, so you may
upgrade your installation gradually.

# Authors

CFEngine was originally created by Mark Burgess with many contributions from
around the world. Thanks [everyone](https://github.com/cfengine/core/blob/master/AUTHORS)!

[CFEngine](https://cfengine.com) is sponsored by [Northern.tech AS](https://northern.tech)
