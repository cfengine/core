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
               fixup => 0,
               make => "make -j3",
               repo_root => "../../",
              );

GetOptions(\%options,
           "help|h!",
           "fixup|f!",
           "good=s",
           "bad=s",
           "make=s",
          );

usage() if ($options{help});

unless (exists $options{good} && exists $options{bad})
{
    usage();
}

my $rc = 0;
my $test = "";
my $file = shift @ARGV;

chdir $options{repo_root};

print qx/git bisect start/;

if ($options{fixup})
{
  $test = <<"TESTSTR";
(cd ./tests/acceptance/; ./testall ${file} | grep -q "^.*\.cf Pass\$" && (test ! \$? && exit \$?) || exit 1)
TESTSTR

  # invert; we're trying to find a commit that fixed a bug.
  print qx/git bisect good $options{bad}/;
  print qx/git bisect bad $options{good}/;
}
else
{
  $test = <<"TESTSTR";
(cd ./tests/acceptance/; ./testall ${file} | grep -q "^.*\.cf Pass\$" && exit \$? || exit 1)
TESTSTR

  print qx/git bisect good $options{good}/;
  print qx/git bisect bad $options{bad}/;
}

my $m = $options{make};
my $script = "($m clean && ./autogen.sh && ./configure --enable-debug && $m || exit 125) && " . $test;

system("git bisect run sh -c '$script'");

exit $rc;

############

sub usage {
  print <<EOHIPPUS;
Syntax: $0 [-f|--fixup] [--make="make command"] --good=<commit-sha> --bad=<commit-sha> TEST.cf

Automatically git bisect against the specified CFEngine acceptance test;
cleaning artifacts, regenerating autotools files, and rebuilding binaries
along the way.  You need to provide a good and a bad commit SHA.

By default hunts for the commit that introduced a bug. If you want the commit
that fixed a bug, use `--fixup`

          This forces the program to respect git's good and bad more like bzr's
          notion of yes -vs- no... much less confusing when hunting for a commit
          that _fixed_ an issue rather than introducing one.

[-g|--good]:  the commit-sha whose build produces known good results.
[-b|--bad]:   the commit-sha whose build produces known bad results.
[-m|--make]:  a make command (default $options{make})

This script expects to be run from inside tests/acceptance/.

---

For the final argument, supply a CFEngine acceptance test to check against.

Example invocation:

  ./bisect-acceptance.pl --fixup --good=HEAD --bad=ff4dcdc ./01_vars/01_basic/009.cf

EOHIPPUS

  exit;
}
