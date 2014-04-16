#!/usr/bin/perl

use warnings;
use strict;
use Data::Dumper;
use Getopt::Long;
use File::Basename;
use Sys::Hostname;

$|=1;                                   # autoflush

my %options = (
               help => 0,
               bug => 0,
               fixup => 0,
               good => 0,
               bad => 0,
               repo_root => "../../",
              );

GetOptions(\%options,
           "help|h!",
           "bug|b!",
           "fixup|f!",
           "good=s",
           "bad=s",
          );

usage() if ($options{help});

if ($options{bug} != 0 && $options{fixup} != 0) {
  die "Please specify either --bug or --fixup, they are mutally exclusive options...";
}

usage() unless ($options{bug} || $options{fixup});

my $rc = 0;
my $test = "";
my $file = shift @ARGV;

chdir $options{repo_root};

print qx/git bisect start/;

if ($options{bug}) {
  $test = <<"TESTSTR";
(cd ./tests/acceptance/; ./testall ${file} | grep -q "^.*\.cf Pass\$" && exit \$? || exit 1)
TESTSTR

  print qx/git bisect good $options{good}/;
  print qx/git bisect bad $options{bad}/;
}

if ($options{fixup}) {
  $test = <<"TESTSTR";
(cd ./tests/acceptance/; ./testall ${file} | grep -q "^.*\.cf Pass\$" && (test ! \$? && exit \$?) || exit 1)
TESTSTR

  # invert; we're trying to find a commit that fixed a bug.
  print qx/git bisect good $options{bad}/;
  print qx/git bisect bad $options{good}/;
}

my $script = "make -j3 clean && ./autogen.sh && ./configure --enable-debug && " .
             "make -j3 && " . $test; 

system("git bisect run sh -c '$script'");

exit $rc;

############

sub usage {
  print <<EOHIPPUS;
Syntax: $0 [-b|--bug]|[-f|--fixup] [--good=<commit-sha>] [--bad=<commit-sh>] TEST.cf

Automatically git bisect against the specified CFEngine acceptance test;
cleaning artifacts, regenerating autotools files, and rebuilding binaries
along the way.


REQUIRED: specify --bug or --fixup to indicate whether you are hunting for
          the commit that introduced a bug or the commit that fixed a bug.

          This forces the program to respect git's good and bad more like bzr's
          notion of yes -vs- no... much less confusing when hunting for a commit
          that _fixed_ an issue rather than introducing one.

[-g|--good]:  the commit-sha whose build produces known good results.
[-b|--bad]:   the commit-sha whose build produces known bad results.


This script expects to be run from inside tests/acceptance/.

---

For the final argument, supply a CFEngine acceptance test to check against.

EOHIPPUS

  exit;
}
