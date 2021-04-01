#!/usr/bin/python3
#
# Sample custom promise type, uses cfengine.py library located in same dir.
#
# Use it in the policy like this:
# promise agent multiline_insert
# {
#     interpreter => "/usr/bin/python3";
#     path => "$(sys.inputdir)/multiline_insert.py";
# }
# bundle agent main
# {
#   multiline_insert:
#     "/home/vagrant/output"
#       lines => { "line1", "line2", "line3" };
# }

from cfengine import PromiseModule, ValidationError, Result


def _file_content_is(fpath, contents):
    try:
        with open(fpath, "r") as f:
            return f.read() == contents
    except:
        return False


class MultilineInsertPromiseTypeModule(PromiseModule):
    def __init__(self):
        super().__init__("multiline_insert_promise_module", "0.0.1")

    def validate_promise(self, promiser, attributes):
        if ("lines" not in attributes or
            type(attributes["lines"]) not in (list, tuple) or
            any(type(item) != str for item in attributes["lines"])):
            raise ValidationError("'lines' attribute of type slist required")

    def evaluate_promise(self, promiser, attributes):
        dst = promiser
        lines = attributes["lines"]
        lines_str = "\n".join(lines)
        if _file_content_is(dst, lines_str):
            return Result.KEPT

        try:
            with open(dst, "w") as f:
                f.write(lines_str)
        except:
            return Result.NOT_KEPT

        if _file_content_is(dst, lines_str):
            return Result.REPAIRED
        else:
            return Result.NOT_KEPT


if __name__ == "__main__":
    MultilineInsertPromiseTypeModule().start()
