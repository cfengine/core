#!/usr/bin/python3
#
# Sample custom promise type, uses cfengine.py library located in same dir.
#
# Use it in the policy like this:
# promise agent copy
# {
#     interpreter => "/usr/bin/python3";
#     path => "$(sys.inputdir)/copy_promises.py";
# }
# bundle agent main
# {
#   copy:
#     "/home/vagrant/dst"
#       from => "/home/vagrant/src";
# }

import os.path
import filecmp
import shutil

from cfengine import PromiseModule, Result


class CopyPromiseTypeModule(PromiseModule):
    def __init__(self):
        super().__init__("copy_promise_module", "0.0.1")

    def validate_promise(self, promiser, attributes):
        pass

    def evaluate_promise(self, promiser, attributes):
        dst = promiser
        src = attributes["from"]
        if os.path.exists(dst) and filecmp.cmp(src, dst):
            return Result.KEPT
        try:
            shutil.copy(src, dst)
        except:
            return Result.NOT_KEPT
        if os.path.exists(dst) and filecmp.cmp(src, dst):
            self.log_info("Copied '%s' to '%s'" % (src, dst))
            return Result.REPAIRED
        else:
            return Result.NOT_KEPT


if __name__ == "__main__":
    CopyPromiseTypeModule().start()
