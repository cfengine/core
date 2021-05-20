#!/usr/bin/python3
#
# Sample custom promise type, uses cfengine.py library located in same dir.
#
# Use it in the policy like this:
# promise agent json_insert
# {
#     interpreter => "/usr/bin/python3";
#     path => "$(sys.inputdir)/json_insert.py";
# }
# bundle agent main
# {
#   json_insert:
#     "/home/vagrant/output"
#       json_data => parsejson('{"key": "value"}');
# }

import json
from cfengine import PromiseModule, ValidationError, Result


def _file_content_is(fpath, contents):
    try:
        with open(fpath, "r") as f:
            return f.read() == contents
    except:
        return False


class JsonInsertPromiseTypeModule(PromiseModule):
    def __init__(self):
        super().__init__("json_insert_promise_module", "0.0.1")

    def validate_promise(self, promiser, attributes):
        if ("json_data" not in attributes or
            type(attributes["json_data"]) != dict):
            raise ValidationError("'json_data' attribute of type data required")

    def evaluate_promise(self, promiser, attributes):
        dst = promiser
        data = attributes["json_data"]
        data_str = json.dumps(data)
        if _file_content_is(dst, data_str):
            return Result.KEPT

        try:
            with open(dst, "w") as f:
                f.write(data_str)
        except:
            return Result.NOT_KEPT

        if _file_content_is(dst, data_str):
            return Result.REPAIRED
        else:
            return Result.NOT_KEPT


if __name__ == "__main__":
    JsonInsertPromiseTypeModule().start()
