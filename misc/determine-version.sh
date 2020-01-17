# This script takes an input file (.CFVERSION) containing a version number:
# 3.16.0
# And outputs either 3.16.0 or 3.16.0a.c1f3175c5
# It replaces determine-version.py, but is much simpler:
# If you are on the correct tag, just use the version number as is
# otherwise
# The output is usually redirected to the CFVERSION file.
#
# NOTE: 3.16.0 or 3.16.0a.c1f3175c5 are the same version, and should not
#       cause a version mismatch. The SHA is mostly a convenience, to be
#       able to see non-final version numbers on nightly / PR builds, etc.
#
# NOTE: This script does not care about EXPLICIT_VERSION env var.
#       This is by design, since it can be changed at configure time.
#       The configure script will check for EXPLICIT_VERSION variable, and
#       prefer that one, if present.

if [ "$#" -ne 1 ]
then
    echo "Usage: determine-version.sh path/to/.CFVERSION"
    exit 1
fi

# Get the desired version from file (manually commited .CFVERSION):
desired_version=$(cat $1)

# Get the current commit SHA (HEAD):
current_sha=$(git rev-parse HEAD)

print_version_with_sha () {
    sha=$(echo $current_sha | cut -b 1-9)
    echo $desired_version"a."$sha
    exit 0
}

print_version_without_sha () {
    echo $desired_version
    exit 0
}

tag_sha=$(git rev-list -n 1 $desired_version 2>/dev/null || echo "")
if [ "x$current_sha" = "x$tag_sha" ]
then
    # Tag exists and matches our current commit (HEAD):
    print_version_without_sha
else
    # Tag exists, but is on another commit:
    print_version_with_sha
fi
