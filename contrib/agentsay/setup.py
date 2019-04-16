#!/usr/bin/env python3
"""Install agentsay as a python module"""

__copyright__ = "Northern.tech"
__authors__ = ["Ole Herman Schumacher Elgesem"]
__license__ = "MIT"

import sys
from shutil import copy


def success(directory):
    print("agentsay python module successfully installed in {}".format(directory))
    sys.exit(0)


for directory in sys.path:
    if "site-packages" in directory:
        copy("agentsay.py", directory)
        success(directory)

directory = sys.path[-1]
copy("agentsay.py", directory)
success(directory)

sys.exit("Could not find an install directory in python path")
