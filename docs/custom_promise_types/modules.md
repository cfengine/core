# CFEngine custom promise types

Custom promise types can be added as _Promise modules_.
These enable users to implement their own promise types for managing new resources.
They can be added without a binary upgrade, and shared between users.

## Using custom promise types

A new top level `promise` block (similar to `body`) is used to add new promise types.
After adding a promise type, it is used in the same way as built-in promise types, there is no special syntax.

### Example of using a promise module

```cf3
promise agent git
{
  interpreter => "/usr/bin/python3";
  path => "/var/cfengine/inputs/modules/promises/git.py";
}

bundle agent main
{
  git:
    "/opt/cfengine/masterfiles"
      repo => "https://github.com/cfengine/masterfiles";
}
```

## Evaluation details

Common attributes and class guards are handled by the agent, and not sent to the promise module.

```cf3
bundle agent main
{
  git:
    linux::
      "/opt/cfengine/masterfiles"
        if => "redhat",
        repo => "https://github.com/cfengine/masterfiles";
}
```

In this example, the promise module does not receive information about the `if` attribute, or the `linux::` class guard.
The agent evaluates these, and decides whether to request evaluation from the module.

These attributes are handled by the agent, and cannot be used inside promise modules:

* `if` / `ifvarclass`
* `unless`
* `action` (body for `ifelapsed`, `expireafter`, etc.)
  * `action_policy`, if not default, will be sent to the module, for dry-run/no-changes functionality
* `comment`
* `depends_on`
* `handle`
* `meta`
* `with`

In an early iteration, the agent will emit errors for any of these which are not implemented yet, if you try to use them.

## Creating custom promise types

The agent spawns the promise module as a subprocess and communicates with it using it's standard input and output (stdin, stdout).
It does not use command line arguments, or standard error output (stderr), but these may be used for testing / debugging promise modules.
Everything written to stdin and stdout should follow the module protocol described below.

### Custom promise types using provided libraries

We provide libraries in some programming languages, which abstract away the protocol, letting you only implement the business logic for the promise type.
Here is an example promise type, written in Python, using our library:

```python
import sys
import os
from cfengine import PromiseModule, ValidationError


class GitPromiseTypeModule(PromiseModule):
    def validate_promise(self, promiser, attributes):
        if not promiser.startswith("/"):
            raise ValidationError(f"File path '{promiser}' must be absolute")
        for name, value in attributes.items():
            if name != "repo":
                raise ValidationError(f"Unknown attribute '{name}' for git promises")
            if name == "repo" and type(value) is not str:
                raise ValidationError(f"'repo' must be string for git promise types")

    def evaluate_promise(self, promiser, attributes):
        if not promiser.startswith("/"):
            raise ValidationError("File path must be absolute")

        folder = promiser
        url = attributes["repo"]

        if os.path.exists(folder):
            self.promise_kept()
            return

        self.log_info(f"Cloning '{url}' -> '{folder}'...")
        os.system(f"git clone {url} {folder} 1>/dev/null 2>/dev/null")

        if os.path.exists(folder):
            self.log_info(f"Successfully cloned '{url}' -> '{folder}'")
            self.promise_repaired()
        else:
            self.log_error(f"Failed to clone '{url}' -> '{folder}'")
            self.promise_not_kept()


if __name__ == "__main__":
    GitPromiseTypeModule().start()
```

### Logging

Log levels in CFEngine are explained here:
https://github.com/cfengine/core/blob/master/CONTRIBUTING.md#log-levels

In short, when writing a promise module, these log levels should be used:

* `critical` - Serious errors in protocol or module itself (not in policy)
* `error` - Errors when validating / evaluating a promise, including syntax errors and promise not kept
* `warning` - The promise did not fail, but there is something the user (policy writer) should probably fix. Some examples:
  * Policy relies on deprecated behavior/syntax which will change
  * Policy uses demo / unsafe options which should be avoided in a production environment
* `notice` - Unusual events which you want to notify the user about
  * Most promise types won't need this - usually `info` or `warning` is more appropriate
  * Useful for events which happen rarely and are not the result of a promise, for example:
    * New credentials detected
    * New host bootstrapped
    * The module made a change to the system for itself to work (database initialized, user created)
* `info` - Changes made to the system (usually 1 per repaired promise, more if the promise made multiple different changes to the system)
* `verbose` - Human understandable detailed information about promise evaluation
* `debug` - Programmer-level information that is only useful for CFEngine developers or module developers

Note that all log levels, except for `debug`, should be friendly to non-developers, and not include programmer's details (such as protocol messages, source code references, function names, etc.).

### Results

Each operation performed by the module, sends a result back to the agent.
The possible results are as follows:

* Shared between operations:
  * `error` - an unexpected error occured in the module or protocol, indicating a bug in CFEngine or the promise module
    * Should be explained by a `critical` level log message
* Promise validation:
  * `valid` - No problems with the data or data types in promise
  * `invalid` - There are problems with the promise data or data types
    * Should be explained by an `error` level log message
* Promise evaluation:
  * The module should assume the promise has already been validated.
    * It does not need to validate the promise again, and should **not** return `valid` / `invalid`.
  * `kept` - promise satisfied already, no change made
  * `repaired` - promise not satisfied before, but fixed now
    * The change should be explained in a `info` level log message
  * `not_kept` - promise not satisfied before, and could not be fixed
    * Should be explained by an `error` level log message
* Teminate:
  * `success` - Module succesfully terminated without errors
  * `failure` - There were problems when trying to clean up / terminate
    * Should be explained by a `critical` level log message

Built-in CFEngine promises may have multiple outcomes when evaluated.
For the sake of simplicity, custom promises may only have 1 outcome, logic should be as follows:

1. If the promise was already satisfied and no changes were necessary, the promise was `kept`
2. Otherwise, if there were issues and the module could not fix/satisfy all parts of the promise, it was `not_kept`.
3. Otherwise, if some changes were made (successfully), the promise was `repaired`.

### Protocol

To implement a promise module from scratch, you will have to implement the promise module protocol.
2 variants of the protocol are provided.
1 JSON based and 1 line based.

**Note: if you are using the library, as shown above, you don't have to care / learn about the following internals of the protocol.**

### Protocol header

When a promise module starts, the agent sends a protocol header (single line), followed by 2 newline characters.

The header sent by cf-agent consists of 3 space-separated parts:

* Name of program - Example: `cf-agent`
* CFEngine version - Example: `3.16.0`
* Highest supported protocol version - Example: `v1`

The header response sent by the module consists of 3 or more space separated parts:

* Module name - Example: `git_promise_module`
* Module version - Example: `0.0.1`
* Requested protocol version - Example: `v1`
* Rest of line, optional: Feature requests/flags - Example: `line_based`

The header has the same syntax regardless of protocol (It is used to determine protocol).
The module should respond with the same protocol version, or lower.
Lowest protocol version wins.
The promise module should not request features which are unavailable in the requested protocol version.

There is no `success` message after the headers are exchanged.
The module can assume that the version and requested features were accepted.
The agent will terminate the module and error, if it requests something invalid.

#### Example headers:

Request sent by cf-agent to promise module:

```
cf-agent 3.16.0 v1
```

Response, requesting line based protocol:

```
git_promise_module 0.0.1 v1 line_based
```

Response, requesting JSON based protocol:

```
git_promise_module 0.0.1 v1 json_based
```

After the initial exchange of protocol headers, the module responds to requests from the agent, using the agreed upon protocol.
Requests and responses follow the same format and rules, and are commonly called messages.
For each request, there should be exactly one response.

### Protocol request sequence

A promise module will receive 3 types of requests (operations):

1. `validate_promise` - Validate the names and types of promise attributes
2. `evaluate_promise` - Perform changes to the system
3. `terminate` - Shut down the module

The module should not make any assumptions about the order of validation or evaluation.
Each promise might be validated and evaluated, 0, 1, or more times, in any order.
In general, the promise module should not require that state is maintained between the individual requests.

The agent _might_ start multiple processes of the same module in parallel, for example for a module implementing multiple promise types.
The module should not rely on this.
Additionally, the same module may be started and stopped multiple times during an agent run, so don't make any assumptions that it will only run once per agent run.

The promise module's responses to cf-agent requests must include `operation` and `result`.
Promiser string and attributes are typically added for convenience.

##### Result classes

After promise evaluation the module can send back a list of classes to set, to give more information about what happened.
Result classes should only be included in the response to `evaluate` requests - validation, or other operations should not define classes.
Exactly what classes to set and what information they should give the policy writer is up to the module author.
They are not required, and for simple promises, they might not be necessary.
Some examples could be: `masterfiles_cloned`, `ran_git_clone`, `masterfiles_cloned_from_github` etc.
Both how many classes are set, and the class names can be dynamic, depending on the promiser, attribute, module, etc.
In the JSON protocol, they are a list of strings, in the line based protocol, they are a comma separated list.
In both cases, the key is `result_classes`.
See examples for both protocols in their sections below.

#### JSON protocol

The JSON based protocol supports all aspects of CFEngine promises, including `slist`, `data` containers, and strings with newlines, tabs, etc.
It is designed to be easy to implement in a language with JSON support.

Protocol messages are separated by empty lines (double newline).
The headers (request and response) are not JSON, but a sequence of space-separated values.
All messages sent by cf-agent and the promise module are single line JSON-data, except:

* Headers (both from cf-agent and promise module) are not JSON.
* JSON responses sent from promise module may optionally be preceeded by log messages, as explained below.

Within strings in the JSON data, newline characters must be escaped (`\n`).
This is not strictly required by the JSON spec, but most implementations do this anyway.

##### Example requests in JSON based protocol

```
cf-agent 3.16.0 v1

{"operation": "validate_promise", "log_level": "info", "promise_type": "git", "promiser": "/opt/cfengine/masterfiles", "attributes": {"repo": "https://github.com/cfengine/masterfiles"}}

{"operation": "evaluate_promise", "log_level": "info", "promise_type": "git", "promiser": "/opt/cfengine/masterfiles", "attributes": {"repo": "https://github.com/cfengine/masterfiles"}}

{"operation": "terminate", "log_level": "info"}
```

##### Example response in JSON based protocol

```
git_promise_module 0.0.1 v1 json_based

{"operation": "validate_promise", "promiser": "/opt/cfengine/masterfiles", "attributes": {"repo": "https://github.com/cfengine/masterfiles"}, "result": "valid"}

log_info=Cloning 'https://github.com/cfengine/masterfiles' -> '/opt/cfengine/masterfiles'...
log_info=Successfully cloned 'https://github.com/cfengine/masterfiles' -> '/opt/cfengine/masterfiles'
{"operation": "evaluate_promise", "promiser": "/opt/cfengine/masterfiles", "attributes": {"repo": "https://github.com/cfengine/masterfiles"}, "result_classes": ["masterfiles_cloned"], "result": "repaired"}

{"operation": "terminate", "result": "success"}
```

In the example above, log messages are sent to the agent on separate lines.
The syntax is the same as for the line based protocol, described below.
This is done so the agent can print the messages while the promise is evaluating.
(In the example, cloning the repo takes a few seconds, and it is nice to get some feedback).

Log messages formatted like this must be before the JSON message, and it is optional.
You can also include log messages in the JSON data:

```
{
  "operation": "evaluate_promise",
  "promiser": "/opt/cfengine/masterfiles",
  "attributes": {
    "repo": "https://github.com/cfengine/masterfiles"}
  },
  "log": [
    {
      "level": "info",
      "message": "Cloning 'https://github.com/cfengine/masterfiles' -> '/opt/cfengine/masterfiles'..."
    }
  ],
  "result_classes": [
    "masterfiles_cloned"
  ],
  "result": "repaired"
}
```

(Formatted here for readability, JSON data must still be on a single line in protocol).

#### Line based protocol

The line based protocol supports a limited subset of features of CFEngine promises, which should be enough for most use cases.
In particular, data passed to the module (through promise attributes) must be strings, without newlines.
`slist`, `data` containers, and strings with newlines are not supported.
It is designed to be easy to implement in a language which doesn't have JSON support.

A protocol message is a sequence of lines, separated by newline characters, and terminated by an extra newline character (empty line).
Each line is a key-value pair, separated by `=`.
Anything before the first `=` of a line is the key, anything after is the value.

The key may not be empty, but the value may.
The key must consist of lowercase letters and underscores, not numbers, spaces, quotes, or other symbols (`[a-z_]+`).
The value may contain anything (including `=` signs) except for newlines and zero-bytes.

##### Example requests in line based protocol

```conf
cf-agent 3.16.0 v1

operation=validate_promise
log_level=info
promise_type=git
promiser=/opt/cfengine/masterfiles
attribute_repo=https://github.com/cfengine/masterfiles

operation=evaluate_promise
log_level=info
promise_type=git
promiser=/opt/cfengine/masterfiles
attribute_repo=https://github.com/cfengine/masterfiles

operation=terminate
log_level=info
```

##### Example response in line based protocol

```conf
git_promise_module 0.0.1 v1 line_based

operation=validate_promise
promiser=/opt/cfengine/masterfiles
attribute_repo=https://github.com/cfengine/masterfiles
result=valid

operation=evaluate_promise
promiser=/opt/cfengine/masterfiles
attribute_repo=https://github.com/cfengine/masterfiles
log_info=Cloning 'https://github.com/cfengine/masterfiles' -> '/opt/cfengine/masterfiles'...
log_info=Successfully cloned 'https://github.com/cfengine/masterfiles' -> '/opt/cfengine/masterfiles'
result_classes=masterfiles_cloned,ran_git_clone
result=repaired

operation=terminate
result=success
```

##### Repeated keys

In some cases, the left side keys within the protocol message are not unique.
Most notably, this is useful for logging in response messages:

```
log_verbose=Users left in list: 'alice', 'bob', 'charlie'
log_info=User 'alice' created
log_info=User 'alice' added to group 'testers'
log_verbose=Users left in list: 'bob', 'charlie'
log_info=User 'bob' created
log_info=User 'bob' added to group 'testers'
log_verbose=Users left in list: 'charlie'
log_info=User 'charlie' created
log_info=User 'charlie' added to group 'testers'
log_verbose=No users left in list
```

This enables the agent to filter the log messages based on log level, and also print them in the correct order.
