#!/usr/bin/perl -w

##############################################################################
#
#   cf-profile.pl
#
#   Copyright (C) cfengineers.net
#
#   Written and maintained by Jon Henrik Bjornstad <jonhenrik@cfengineers.net>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; version 3.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
##############################################################################

use strict;
use POSIX;
use Time::HiRes;
use Data::Dumper;
use Getopt::Std;

my %opts = ();
my %data = ();
my $debug = 0;
my $line = "";
my $cur_bundle = "";
my $cur_bundle_key = "";
my $promise_type = "";
my $iter = "";
my $parent_bundle = "";
my @parent = ();
my @parent_keys = ();
my $is_edit_bundle = 0;
my @b_log = ();
my $tabber = " " x 4;
my $branch = "-" x 4;
$line = <STDIN>;

getopts("T:dhstva", \%opts);

if(defined($opts{h})){
	usage();
}

if(defined($opts{d})){
	$debug = 1;
}
if(defined($opts{T})){
	$tabber = " " x $opts{T};
	$branch = "-" x $opts{T};
}

if(!defined($opts{a}) and !(defined($opts{t}) or defined($opts{c}) or defined($opts{s}))) {
	print "ERROR: you need to specify at least one of -a, -t, -s or -c\n";
	usage();
}

$data{start} = Time::HiRes::gettimeofday();

if($line =~ /^.*> /) {
	debug("Found output version < 3.5.0");
	prelude_v1();
	bundles_v1();
}else{
	die("This program is currently working with legacy output of cfengine < 3.5.0. If you are on
cfengine >= 3.5.0, try using the -l or --legacy-output switches for cf-agent.");
}

if(defined($opts{t}) or defined($opts{a})){
	print "===============================================================================\n";
	print "Execution tree\n";
	print "===============================================================================\n";
	print "Start: $data{start} s\n";
	print "|\n";

	foreach my $b(@b_log){
		my $elapsed = sprintf("%.5f", $data{bundles}{$b}{stop} - $data{bundles}{$b}{start});
		my $rel_start = sprintf("%.5f", $data{bundles}{$b}{start} - $data{start});
		my $rel_stop = sprintf("%.5f", $data{bundles}{$b}{stop} - $data{start});
		my $tab = "$tabber"x$data{bundles}{$b}{level};
	#	my $tab = "      "x$data{bundles}{$b}{level};
		my $header = "$branch"x$data{bundles}{$b}{level};
	#	my $header = "-----"x$data{bundles}{$b}{level};
		print "|$header> Bundle $b\n";
		print "|$tab"."$tab"."Start: $data{bundles}{$b}{start} s\n" if defined($opts{v});
		print "|$tab"."$tab"."Stop: $data{bundles}{$b}{stop} s\n" if defined($opts{v});
		print "|$tab"."$tab"."Elapsed: $elapsed s\n";
		print "|$tab"."$tab"."Relative start: $rel_start s\n" if defined($opts{v});
		print "|$tab"."$tab"."Relative stop: $rel_stop s\n" if defined($opts{v});
		
		if(defined($opts{v})){
			foreach my $p(@{$data{bundles}{$b}{prtype}}) {
				my $t = ($data{bundles}{$b}{promise_types}{$p}{start})? $data{bundles}{$b}{promise_types}{$p}{start} : "NAN";
				my ($pt,$pass) = split(/:/,$p);
				print "|$tab"."$tab"."$tab"."Promise type $pt: pass $pass\n";
				print "|$tab"."$tab"."$tab"."$tab"."Start: $t s\n";
				if(defined($data{bundles}{$b}{promise_types}{$p}{classes})) {
					print "|$tab"."$tab"."$tab"."$tab"."$tab".join("\n|$tab"."$tab"."$tab"."$tab"."$tab", @{$data{bundles}{$b}{promise_types}{$p}{classes}});
					print "\n";
				}
			}
		}
		print "|\n";
	}

	$data{stop} = Time::HiRes::gettimeofday();
	print "|\n";
	print "Stop: $data{stop} s\n";
	print "===============================================================================\n";
	print "\n";
}

if(defined($opts{s}) or defined($opts{a})){
	print "===============================================================================\n";
	print "Summary\n";
	print "===============================================================================\n";
	print "Top 10 worst, bundles:\n";
	print "Top 10 worst, promise types:\n";
}

exit(0);

sub usage {
	print<<EOF;

Usage: $0 [-T N] [-s|-t|-c|-a|-d|-v|-h]

   -s         : Print only summary info
   -t         : Print only eecution tree
   -c         : Print only classes info
   -a         : Print all (synonym to -s -t -c)
   -T N       : Set the tabulator to N chars in execution tree
   -d         : Set debug mode
   -v         : Set verbose output
   -h         : Print this help text

EOF
	exit(1);
}

sub debug {
	my $msg = shift;
	print "DEBUG: $msg\n" if $debug;
}

sub prelude_v1{
	$line = <STDIN>;
	do {
		if($line =~ /(Defined|Hard)\s+classes\s+=\s+\{\s+(.*)\s*\}/){
			$data{all_classes} = $2;
			debug("Found classes: \"$2\"");
			return 0;
		}
	} while($line = <STDIN>)
}

sub bundles_v1 {
	do {
		if($line =~ /Handling\s+file\s+edits\s+in\s+edit_line\s+bundle/){
			$is_edit_bundle = 1;  
		}
		if($line =~ /\s+BUNDLE\s+(\w+)(\(?\s*\{?[^\}]+\}?\s?\)?)?/){
			if(!$is_edit_bundle) {
				my $bundle = $1;
				my $args = (defined($2))? $2 : "";
				chomp $args;
				if($args) {
					while($line !~ /\'\}\s+\)$/){
						$line = <STDIN>;
						$args .= $line;
					}
				}
				chomp($args);
				debug("Found bundle $bundle $args");
				$cur_bundle = $bundle;
				$cur_bundle_key = $bundle.":".$args;
				$data{bundles}{$cur_bundle_key}{start} = Time::HiRes::gettimeofday();
				$b_log[$#b_log + 1] = $cur_bundle_key;
			}
			$is_edit_bundle = 0;
		} elsif ($line =~ /(\S+)\s+in\s+bundle\s+$cur_bundle\s+\((\d+)\)/){
			$promise_type = $1;
			$iter = $2;
			$data{bundles}{$cur_bundle_key}{prtype}[$#{$data{bundles}{$cur_bundle_key}{prtype}} + 1] = $promise_type.":".$iter;
			$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{start} = Time::HiRes::gettimeofday();
			if($promise_type =~ /^methods$/ && ! grep(/$cur_bundle/,@parent)){
				debug("Registering parent $cur_bundle");
				push(@parent,$cur_bundle);
				push(@parent_keys,$cur_bundle_key);
			}
			debug("Found $promise_type in bundle $cur_bundle iter $iter");
		}elsif($line =~ /(Bundle\s+Accounting\s+Summary\s+for|Zero\s+promises\s+executed\s+for\s+bundle)\s+\"*\'*(\w+)\"*\'*/) {
			my $b = $2;
			debug("End $b");
			if($#parent >= 0 && $parent[$#parent] =~ /$b/) {
				pop(@parent);
				my $p = pop(@parent_keys);
				$data{bundles}{$p}{level} = $#parent + 2;
				$data{bundles}{$p}{stop} = Time::HiRes::gettimeofday();
			}else{
				$data{bundles}{$cur_bundle_key}{stop} = Time::HiRes::gettimeofday();
				$data{bundles}{$cur_bundle_key}{level} = $#parent + 2;
			}
#		}elsif ($line =~ /\s+(\+|\-)\s+(\S+)$/){
#			$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{classes}[$#{$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{classes}} + 1] = "$1$2";
		}elsif($line =~ /(defining\s+explicit\s+local\s+bundle\s+class\s+|defining\s+promise\s+result\s+class\s+)(\S+)/){
			$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{classes}[$#{$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{classes}} + 1] = "+$2";
		}
	} while($line = <STDIN>)
}
