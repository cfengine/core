#!/usr/bin/python

from __future__ import print_function
import re
import subprocess
import sys

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

def usage():
    print("Usage: %s [revision]" % sys.argv[0])
    print()
    print("Prints the predicted next version after the given revision, based on")
    print("current tags.")
    sys.exit(1)

def extract_version_components(version):
    match = re.match("^([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)", version)
    if match is None:
        return None
    return (match.group(1), match.group(2), match.group(3), match.group(4))

if len(sys.argv) < 2:
    REV = "HEAD"
else:
    if sys.argv[1] == "-h" or sys.argv[1] == "--help":
        usage()
    REV = sys.argv[1]

# Find the most recent tag reachable from this commit.
git = subprocess.Popen(["git",
                        "describe",
                        "--tag",
                        "--abbrev=0",
                        REV],
                       stdout=subprocess.PIPE)
recent_tag = git.stdout.readlines()[0].strip()
git = subprocess.Popen(["git", "rev-parse", recent_tag + "^{}"], stdout=subprocess.PIPE)
recent_rev = git.stdout.readlines()[0].strip()

# Find its version, if any.
recent_version = extract_version_components(recent_tag)
if recent_version is None:
    (recent_major, recent_minor, recent_patch, recent_extra) = ("0", "0", "0", "")
    tag_finder = ["git", "tag"]
else:
    (recent_major, recent_minor, recent_patch, recent_extra) = recent_version
    tag_finder = ["git", "tag", "--contains", recent_tag]

def version_cmp(a, b):
    if a[0] > b[0]:
        return 1
    elif a[0] < b[0]:
        return -1

    # a[0] == b[0]
    if a[1] > b[1]:
        return 1
    elif a[1] < b[1]:
        return -1

    # a[1] == b[1]
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

git_tag_list = subprocess.Popen(tag_finder, stdout=subprocess.PIPE)
all_tags = []
for tag in git_tag_list.stdout.readlines():
    tag = tag.strip()
    git_rev = subprocess.Popen(["git", "rev-parse", tag + "^{}"], stdout=subprocess.PIPE)
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

if len(all_tags) > 1:
    # This is a new minor version
    print("%s.%d.%s" % (all_tags[0][0], int(all_tags[0][1]) + 1, 0))
else:
    # This is a new patch version.
    # A "pure" version with no extra tag info is considered higher than
    # an "impure" one, IOW "3.8.0" > "3.8.0b1".
    if recent_extra != "":
        print("%s.%s.%d" % (recent_major, recent_minor, int(recent_patch)))
    else:
        print("%s.%s.%d" % (recent_major, recent_minor, int(recent_patch) + 1))
sys.exit(0)
