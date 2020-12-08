"""Custom Promise Type module for adding missing gpg keys

Given some GPG user id values (see man page for all possiblities, there are many)
and the ascii value of a key, ensure that the key is present in the given GPG homedir.
Typically the user id will be a key fingerprint or an email address.

Installation:
  cp gpg.py $(sys.masterdir)/modules/promises

typically:
  cp gpg.py /var/cfengine/masterfiles/modules/promises

Usage:

Provide the user id and ascii values to this promise with the `keylist` attribute in
a data container with the format:

similar to this standard: https://datatracker.ietf.org/doc/draft-mccain-keylist/

```json
{
  "keys": [
    {
      "user_id": "86EB84C96B2E62676B47C4919BB29FF9FD3ED09F",
      "ascii": "<large chunk of ascii public key content for importing"
    },
  ]
}
```

The promiser in this case is the GPG homedir. Typically `$HOME/.gnupg`

```cfe3
bundle agent main
{
  gpg:
    "$(sys.user_data[home_dir])$(const.dirsep).gnupg"
      keylist => readjson("$(this.promise_dirname)/keylist.json");
}
```

"""

import json
from subprocess import Popen, PIPE
import sys
from cfengine import PromiseModule, ValidationError, Result

class GpgKeysPromiseTypeModule(PromiseModule):
    def __init__(self):
        super().__init__("gpg_keys_promise_module", "0.0.1")

    def gpg_import_ascii(self, homedir, ascii):
        with Popen(
            ["gpg", "--homedir", f"{homedir}", "--import"],
            stdout=PIPE,
            stdin=PIPE,
            stderr=PIPE,
        ) as proc:
            try:
                sdtout, stderr = proc.communicate(input=ascii.encode())
                if proc.returncode == 0:
                    return True
                else:
                    self.log_error(
                        f"Error importing gpg key return code '{proc.returncode}'"
                    )
                    self.log_verbose(
                        f"Import gpg key failed, stderr was '{stderr.decode()}'"
                    )
                    return False
            except TimeoutExpired:
                proc.kill()
                proc.communicate()
                self.log_error("Timed out importing gpg key")
                return False

    def clean_storejson_output(self, storejson_output):
        # workaround custom promise types not supporting data container or slist attribute values
        # this function cleans up the result of sending a data container or slist with
        # attribute => storejson( @(data) )

        return storejson_output.replace('\\"', '"').replace("\n", "")

    def gpg_key_present(self, homedir, user_id):
        with Popen(
            ["gpg", "--homedir", f"{homedir}", "-k", f"{user_id}"],
            stdout=PIPE,
            stdin=PIPE,
            stderr=PIPE,
        ) as proc:
            try:
                stdout, stderr = proc.communicate()
                if proc.returncode == 0:
                    return True
                else:
                    self.log_verbose(
                        f"Querying gpg key failed, stderr was '{stderr.decode()}'"
                    )
                    return False
            except TimeoutExpired:
                proc.kill()
                proc.communicate()
                self.log_error(f"Timed out querying for gpg key '{user_id}'")

    def validate_promise(self, promiser, attributes):
        if not promiser.startswith("/"):
            raise ValidationError(
                f"Promiser '{promiser}' for 'gpg_keys' promise must be an absolute path"
            )
        if not "keylist" in attributes:
            raise ValidationError(
                f"Required attribute 'keylist' missing for 'gpg_keys' promise"
            )

    def evaluate_promise(self, promiser, attributes):
        keylist_json = self.clean_storejson_output(attributes["keylist"])
        self.log_verbose(f"keylist_json is '{keylist_json}'")

        # strict=False because json.loads() doesn't allow newlines by default
        keylist = json.loads(keylist_json, strict=False)

        result = Result.KEPT

        for key in keylist["keys"]:
            self.log_verbose(f"key is {key}")
            if "fingerprint" in key:
                user_id = key["fingerprint"]
            elif "email" in key:
                user_id = key["email"]
            else:
                self.log_error(
                    "Each keylist entry must specify a user id with either a 'fingerprint' or 'email' property"
                )
                result = Result.NOT_KEPT
                continue

            if not self.gpg_key_present(promiser, user_id):
                self.log_verbose(f"No key found for user id '{user_id}'")
                self.log_info(
                    f"Importing ascii key for user id '{user_id}' into gpg homedir '{promiser}'"
                )
                if self.gpg_import_ascii(promiser, key["ascii"]):
                    if result != Result.NOTKEPT:
                        result = Result.REPAIRED
                else:
                    self.log_error(f"Unable to import key for user id '{user_id}'")
                    result = Result.NOT_KEPT

        return result


if __name__ == "__main__":
    GpgKeysPromiseTypeModule().start()
