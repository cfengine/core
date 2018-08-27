testing 1 2 3 -craig

| Version    | [Core](https://github.com/cfengine/core)                                                                           | [MPF](https://github.com/cfengine/masterfiles)                                                                                  |
|------------|--------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------|
| master     | [![Core Build Status](https://travis-ci.org/cfengine/core.svg?branch=master)](https://travis-ci.org/cfengine/core) | [![MPF Build Status](https://travis-ci.org/cfengine/masterfiles.svg?branch=master)](https://travis-ci.org/cfengine/masterfiles) |
| 3.12.x LTS | [![Core Build Status](https://travis-ci.org/cfengine/core.svg?branch=3.12.x)](https://travis-ci.org/cfengine/core) | [![MPF Build Status](https://travis-ci.org/cfengine/masterfiles.svg?branch=3.12.x)](https://travis-ci.org/cfengine/masterfiles) |
| 3.10.x LTS | [![Core Build Status](https://travis-ci.org/cfengine/core.svg?branch=3.10.x)](https://travis-ci.org/cfengine/core) | [![MPF Build Status](https://travis-ci.org/cfengine/masterfiles.svg?branch=3.10.x)](https://travis-ci.org/cfengine/masterfiles) |
| 3.7.x LTS  | [![Core Build Status](https://travis-ci.org/cfengine/core.svg?branch=3.7.x)](https://travis-ci.org/cfengine/core)  | [![MPF Build Status](https://travis-ci.org/cfengine/masterfiles.svg?branch=3.7.x)](https://travis-ci.org/cfengine/masterfiles)  |

[![codecov](https://codecov.io/gh/cfengine/core/branch/master/graph/badge.svg)](https://codecov.io/gh/cfengine/core)
[![Language grade: C](https://img.shields.io/lgtm/grade/cpp/g/cfengine/core.svg?logo=lgtm&logoWidth=18&label=code%20quality)](https://lgtm.com/projects/g/cfengine/core/)

# CFEngine 3

CFEngine 3 is a popular open source configuration management system. Its primary
function is to provide automated configuration and maintenance of large-scale
computer systems.

## Installation

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

Please see the [HACKING.md](https://github.com/cfengine/core/blob/master/HACKING.md) file.

## Relationship to CFEngine 2

CFEngine 3 is *not* a drop-in upgrade for CFEngine 2 installations.  It is a
significantly more powerful version, but it is incompatible with the CFEngine 2
policy language.

The server part of CFEngine 3 supports the network protocol of CFEngine 2, so you may
upgrade your installation gradually.
