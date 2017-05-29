#!/usr/bin/env python

from __future__ import print_function
import re
import subprocess
import sys
import os
from os.path import basename

try:
    from subprocess import DEVNULL # py3k
except ImportError:
    DEVNULL = open(os.devnull, 'wb')


# Determines which version we should call this build.
#
# Consider the following Git history graph with tag names:
#
#    X          Y          Z            M
#    |          |          |            |
#    * 3.7.2    * 3.8.1    * 3.9.0b1    * master
#    |          |          |            |
#    * 3.7.1    * 3.8.0    |            |
#    |          |          \------------+
#    * 3.7.0    * 3.8.0b1               |
#    |          |                       |
#    |          \-----------------------+
#    |                                  |
#    \----------------------------------+
#                                       |
#                                       * 3.7.0b1
#                                       |
#
# At each of the points, X, Y, Z and M, the script needs to find the correct
# next version based on existing tag names. This is the algorithm to achieve
# that.
#
# 1. Find the most recent version tag reachable from the point, let's call it T.
# 2. Find all tags that contain T in their history.
# 3. If none, then increase patch number of T by one and return that (beta
#    becomes 0).
# 4. If there are tags found in step two, find the latest one (as sorted by
#    version) which does not point at the same commit as T, increase the minor
#    number by one, and return that.
#
# Some notes:
# - Does not take into account changes in major version.
# - If you branch off from a patch series from a point earlier than the last
#   patch release, the script will think you are naming the next minor version.
#   (So in the graph above, finding the next version string at point 3.7.1,
#   would find 3.8.0). This scenario has no well defined version, so it's a
#   compromise.


# Print debug info to stderr
VERBOSE=True


def usage():
    print("Usage: %s [revision]" % sys.argv[0])
    print()
    print("Prints the predicted next version after the given revision, based on")
    print("current tags.")
    sys.exit(1)

def verbose_print(*args, **kwargs):
    if VERBOSE:
        print("%s:" % basename(sys.argv[0]),
              *args, file=sys.stderr, **kwargs)

def final_print(s):
    verbose_print("SUCCESS, version detected is: %s" % s)
    print(s)

def extract_version_components(version):
    match = re.match("^([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)", version)
    if match is None:
        return None
    return (match.group(1), match.group(2), match.group(3), match.group(4))

def version_cmp(a, b):
    a_major = int(a[0])
    b_major = int(b[0])
    if   a_major > b_major:
        return 1
    elif a_major < b_major:
        return -1

    a_minor = int(a[1])
    b_minor = int(b[1])
    if   a_minor > b_minor:
        return 1
    elif a_minor < b_minor:
        return -1

    # A "pure" version with no extra tag info is considered higher than
    # an "impure" one, IOW "3.8.0" > "3.8.0b1".
    if a[3] == "":
        if b[3] != "":
            return 1
    else:
        if b[3] == "":
            return -1

    if a[2] > b[2]:
        return 1
    elif a[2] < b[2]:
        return -1

    # a[2] == b[2]
    if a[3] > b[3]:
        return 1
    elif a[3] < b[3]:
        return -1

    return 0




if len(sys.argv) < 2:
    REV = "HEAD"
else:
    if sys.argv[1] == "-h" or sys.argv[1] == "--help":
        usage()
    REV = sys.argv[1]

# Find the abbreviated version of the commit
git = subprocess.Popen(["git", "rev-list", "-1", "--abbrev-commit", REV],
                       stdout=subprocess.PIPE)
abbrev_rev = git.stdout.readlines()[0].strip()
verbose_print("REV        = %s" % REV)
verbose_print("abbrev_rev = %s" % abbrev_rev)


# If the commit is referenced exactly in a tag, then use that tag as is
git = subprocess.Popen(["git", "describe", "--tags", "--exact-match", "--abbrev=0", REV],
                       stdout=subprocess.PIPE, stderr=DEVNULL)
try:
    exact_tag = git.stdout.readlines()[0].strip()
    verbose_print("exact_tag  = %s" % exact_tag)
    (version_major, version_minor, version_patch, version_extra) = extract_version_components(exact_tag)
    final_print("%s.%s.%s%s" % (version_major, version_minor, version_patch, version_extra))
    sys.exit(0)
except IndexError:                          # command returned no output
    verbose_print("exact_tag  = not found")
    pass

# Find the most recent tag reachable from this commit.
git = subprocess.Popen(["git", "describe", "--tags", "--abbrev=0", REV],
                       stdout=subprocess.PIPE)
recent_tag = git.stdout.readlines()[0].strip()
verbose_print("recent_tag = %s" % recent_tag)

# Find the revision corresponding to the tag
git = subprocess.Popen(["git", "rev-parse", recent_tag + "^{}"],
                       stdout=subprocess.PIPE)
recent_rev = git.stdout.readlines()[0].strip()
verbose_print("recent_rev = %s" % recent_rev)

# Find its version, if any.
recent_version = extract_version_components(recent_tag)
verbose_print("recent_version = ", recent_version)

if recent_version is None:
    (recent_major, recent_minor, recent_patch, recent_extra) = ("0", "0", "0", "")
    tag_finder = ["git", "tag"]
else:
    (recent_major, recent_minor, recent_patch, recent_extra) = recent_version
    tag_finder = ["git", "tag", "--contains", recent_tag]

git_tag_list = subprocess.Popen(tag_finder,
                                stdout=subprocess.PIPE)
all_tags = []
for tag in git_tag_list.stdout.readlines():
    tag = tag.strip()
    git_rev = subprocess.Popen(["git", "rev-parse", tag + "^{}"],
                               stdout=subprocess.PIPE)
    rev = git_rev.stdout.readlines()[0].strip()
    if rev == recent_rev:
        # Ignore tags that point at the same commit.
        continue
    match = extract_version_components(tag)
    if match is None:
        # Ignore non-version tags.
        continue
    all_tags.append(match)

all_tags = sorted(all_tags, cmp=version_cmp, reverse=True)
verbose_print("all_tags   =", all_tags)

if len(all_tags) > 1:
    # This is a new minor version
    final_print("%s.%d.0a.%s" % (all_tags[0][0], int(all_tags[0][1]) + 1, abbrev_rev))
else:
    # This is a new patch version.
    # A "pure" version with no extra tag info is considered higher than
    # an "impure" one, IOW "3.8.0" > "3.8.0b1".
    if recent_extra != "":
        patch_version = int(recent_patch)
    else:
        patch_version = int(recent_patch) + 1
    final_print("%s.%s.%da.%s" % (recent_major, recent_minor, patch_version, abbrev_rev))

sys.exit(0)
