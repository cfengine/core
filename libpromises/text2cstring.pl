#!/usr/bin/perl

use warnings;
use strict;
use Data::Dumper;

my $source = shift @ARGV;

die "Syntax: $0 SOURCE" unless defined $source;

open my $sf, '<', $source or die "Can't read from file '$source': $!";
my @lines = <$sf>;

chomp @lines;
s/\\/\\\\/g foreach @lines;
s/"/\\"/g foreach @lines;
@lines = map { "            \"$_\\n\"\n" } @lines;

print @lines;
