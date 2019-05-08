#!/usr/bin/env python

from __future__ import print_function
import re
import subprocess
import sys
import os
from os.path import basename
try:
    from functools import cmp_to_key
except ImportError:
    cmp_to_key = None

try:
    DEVNULL = subprocess.DEVNULL
except AttributeError:
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
# 2a. Fuzzy-detect whether we are on master branch.
# 2b. Find all tags that contain T in their history. If we are not on
#     the master branch, exclude tags that point to the same commit as T
#     (for example T itself and T-build1).
# 3. If no tags were found, then increase *patch number* of T by one and
#    return that (beta becomes 0). Because having no subsequent tags
#    indicates that we are on the same branch as T.
# 4. If there are tags found in step two, find the latest one (as sorted
#    by version) which does not point at the same commit as T, increase
#    the minor number by one, and return that. Because this indicates
#    that we have branched-off after T was issued, and the new tags that
#    contain T were issued from branches not reachable to us now. In the
#    above graph, we are on a new ZZ branch with no tags, and the most
#    recent reachable tag T is 3.7.0b1, so we search for the most recent
#    tag containing 3.7.0b1, which is 3.9.0b1, and we increase the
#    *minor* number, i.e. we set the version to 3.10.0. Magic!
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

# Find the full and abbreviated SHAs of the commit
git = subprocess.Popen(["git", "rev-list", "-1", REV],
                       stdout=subprocess.PIPE)
full_rev = git.stdout.readlines()[0].decode().strip()
git = subprocess.Popen(["git", "rev-list", "-1", "--abbrev-commit", REV],
                       stdout=subprocess.PIPE)
abbrev_rev = git.stdout.readlines()[0].decode().strip()
verbose_print("REV        = %s" % REV)
verbose_print("full_rev   = %s" % full_rev)
verbose_print("abbrev_rev = %s" % abbrev_rev)


# If the commit is referenced exactly in a tag, then use that tag as is
git = subprocess.Popen(["git", "describe", "--tags", "--exact-match", "--abbrev=0", REV],
                       stdout=subprocess.PIPE, stderr=DEVNULL)
try:
    exact_tag = git.stdout.readlines()[0].decode().strip()
    verbose_print("exact_tag  = %s" % exact_tag)
    (version_major, version_minor, version_patch, version_extra) = extract_version_components(exact_tag)
    final_print("%s.%s.%s%s" % (version_major, version_minor, version_patch, version_extra))
    sys.exit(0)
except IndexError:                          # command returned no output
    verbose_print("exact_tag  = not found")

# Find the most recent tag reachable from this commit.
git = subprocess.Popen(["git", "describe", "--tags", "--abbrev=0", REV],
                       stdout=subprocess.PIPE)
recent_tag = git.stdout.readlines()[0].decode().strip()
verbose_print("recent_tag = %s" % recent_tag)

# Find the revision corresponding to the tag
git = subprocess.Popen(["git", "rev-parse", recent_tag + "^{}"],
                       stdout=subprocess.PIPE)
recent_rev = git.stdout.readlines()[0].decode().strip()
verbose_print("recent_rev = %s" % recent_rev)

# Find its version, if any.
recent_version = extract_version_components(recent_tag)
verbose_print("recent_version = ", recent_version)


in_master_branch = True
git_all_merged_branches = subprocess.Popen(["git", "branch", "-r", "--merged"],
                                 stdout=subprocess.PIPE)
for line in git_all_merged_branches.stdout:
    line = line.decode().strip()
    match = re.match('(origin|upstream)/\\d+\\.\\d+\\.x$', line)
    if match:
        in_master_branch = False
        verbose_print("Detected that we are NOT in master branch, but in [%s]" % match.group(0))
        break


# List all tags that contain the most recent tag found earlier, unless
# no such tag was found in which case we list all tags.
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
    tag = tag.decode().strip()
    git_rev = subprocess.Popen(["git", "rev-parse", tag + "^{}"],
                               stdout=subprocess.PIPE)
    rev = git_rev.stdout.readlines()[0].decode().strip()
    if not in_master_branch and rev == recent_rev:
        # Ignore tags that point at the same commit as the most recent
        # tag, i.e. ignore the tag itself and same-release tags (like
        # 3.7.5-build1 which points to the same commit as 3.7.5),
        # because we want the len(all_tags) comparison later to
        # work. But if we are on master branch, keep it, in order to
        # increase the minor version of the most recent tag then.
        continue
    match = extract_version_components(tag)
    if match is None:
        # Ignore non-version tags.
        continue
    all_tags.append(match)

if cmp_to_key is None:
    all_tags = sorted(all_tags, cmp=version_cmp, reverse=True)
else:
    all_tags = sorted(all_tags, key=cmp_to_key(version_cmp), reverse=True)
verbose_print("all_tags   =", all_tags)


if len(all_tags) == 0:
    # No tags besides the most recent one were found, so this is a new
    # patch version. So "increase" the patch version:

    # A "pure" version with no extra tag info is considered higher than
    # an "impure" one, IOW "3.8.0" > "3.8.0b1".
    if recent_extra != "":
        patch_version = int(recent_patch)
    else:
        patch_version = int(recent_patch) + 1
    final_print("%s.%s.%da.%s" % (recent_major, recent_minor, patch_version, abbrev_rev))
else:
    # The most recent tag reachable from here is contained in many more
    # tags. This means that branches were created *after* that tag, and
    # then further tags were issued, and they all represent new minor
    # (3.x) versions. But since those are not reachable from here, we
    # have branched off again, i.e. we need a new *minor* (3.x)
    # version. So find the newest minor version and increase it.
    final_print("%s.%d.0a.%s" % (all_tags[0][0], int(all_tags[0][1]) + 1, abbrev_rev))


sys.exit(0)
