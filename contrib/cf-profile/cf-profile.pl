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
my $cur_bundle_name = "";
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
my $version = 0;
my $namespace = "";
my $b_log_ref = undef;

$line = <STDIN>;

getopts("T:dhstvac", \%opts);

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
	$version = 1;
	prelude_v1();
	bundles_v1();
}else{
	debug("Trying to parse new format output");
	$version = 2;
	prelude_v2();
	bundles_v2();
}
$data{stop} = Time::HiRes::gettimeofday();

if(defined($opts{c}) or defined($opts{a})){
	print "===============================================================================\n";
	print "Classes defined:\n";
	print "$data{all_classes}\n";
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
		my $header = "$branch"x$data{bundles}{$b}{level};
		my $fq_b = $data{bundles}{$b}{name};
		$fq_b = (defined($data{bundles}{$b}{namespace}))? $data{bundles}{$b}{namespace}.":".$fq_b : $fq_b;
		print "|$header> Bundle $fq_b\n";
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

	print "|\n";
	print "Stop: $data{stop} s\n";
}

if(defined($opts{s}) or defined($opts{a})){
	print "===============================================================================\n";
	print "Summary\n";
	print "===============================================================================\n";
	print "Total elapsed: ".sprintf("%.5f", $data{stop} - $data{start})." s\n";
	my %times = ();
	foreach my $b(@b_log){
		my $elapsed = sprintf("%.5f", $data{bundles}{$b}{stop} - $data{bundles}{$b}{start});
		$times{$b} = $elapsed;
	}
	my $no_bundles  = scalar keys %{$data{bundles}};
	print "Total number of bundles: $no_bundles\n";
	print "Top 10 worst, bundles:\n";
	my $iter = 0;
	foreach my $b(sort {$times{$b} <=> $times{$a}} keys %times){
		last if $iter == 10;
		my $t = $times{$b};
		$iter++;
		$b =~ s/:l\d+$//g;	
		print "$tabber$iter.\t${t}s  : $b\n";
	}
}
print "===============================================================================\n";

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
			classes($2);
			return 0;
		}
	} while($line = <STDIN>)
}

sub prelude_v2 {
	$line = <STDIN>;
	my @classes = ();
	do {
		if($line =~ /^\s*verbose:\s+C:\s+(discovered\s+hard|added\s+soft)\s+class\s+(.*)$/){
			$classes[$#classes + 1] = $2;
		}elsif($line =~ /^\s*verbose:\s+Preliminary\s+variable\/class-context\s+convergence\s*$/) {
			classes(join(' ',@classes));
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
				bundle_start($bundle,$args);
			}
			$is_edit_bundle = 0;
		} elsif ($line =~ /(\S+)\s+in\s+bundle\s+$cur_bundle_name\s+\((\d+)\)/){
			$promise_type = $1;
			$iter = $2;
			bundle_promise($promise_type,$iter);
		}elsif($line =~ /(Bundle\s+Accounting\s+Summary\s+for|Zero\s+promises\s+executed\s+for\s+bundle)\s+\"*\'*(\w+)\"*\'*/) {
			my $b = $2;
			bundle_end($b);
#		}elsif ($line =~ /\s+(\+|\-)\s+(\S+)$/){
#			$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{classes}[$#{$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{classes}} + 1] = "$1$2";
		}elsif($line =~ /(defining\s+explicit\s+local\s+bundle\s+class\s+|defining\s+promise\s+result\s+class\s+)(\S+)/){
			$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{classes}[$#{$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{classes}} + 1] = "+$2";
		}
	} while($line = <STDIN>)
}

sub bundles_v2 {
	$line = <STDIN>;
	
	do {
		if($line =~ /BEGIN\s+bundle\s+(\w+)(\(?\s*\{?[^\}]+\}?\s?\)?)?/){
			if(!$is_edit_bundle) {
				my $bundle = $1;
				#my $args = $2;
				my $args = (defined($2))? $2 : "";
				chomp $args;
				#if($args) {
				#	while($line !~ /\'\}\s+\)$/){
				#		$line = <STDIN>;
				#		$args .= $line;
				#	}
				#}
				chomp($args);
				bundle_start($bundle,$args);
			}
			$is_edit_bundle = 0;
		} elsif ($line =~ /^\s+verbose:\s+P:\s+BEGIN\s+promise\s+'\w+'\s+of\s+type\s+"([^"]+)"\s+\(pass\s+(\d)\)\s*$/){
			$promise_type = $1;
			$iter = $2;
			bundle_promise($promise_type,$iter);
		} elsif ($line =~ /^\s+verbose:\s+P:\s+BEGIN\s+un-named\s+promise\s+of\s+type\s+"([^"]+)"\s+\(pass\s+(\d)\)\s*$/){
			$promise_type = $1;
			$iter = $2;
			bundle_promise($promise_type,$iter);
		} elsif($line =~ /^\s+verbose:\s+A:\s+Bundle\s+Accounting\s+Summary\s+for\s+'(\S+)'\s+in\s+namespace\s+(\S+).*$/){
			$data{bundles}{$cur_bundle_key}{namespace} = $2;
		} elsif($line =~ /^\s+verbose:\s+B:\s+END\s+bundle\s+(\w+)/) {
			my $b = $1;
			bundle_end($b);
		}
	} while($line = <STDIN>)
}

sub classes {
	my $classes = shift;
	$data{all_classes} = $classes;
	debug("Found classes: \"$classes\"");
	return 0;
}

sub bundle_start {
	my ($bundle,$args) = @_;
	debug("bundle_start: Found bundle $bundle $args");
	$cur_bundle = "$bundle:l$.";
	$cur_bundle_name = $bundle;
	$cur_bundle_key = "${bundle}${args}:l$.";
	$data{bundles}{$cur_bundle_key}{name} = "${bundle}${args}";
	$data{bundles}{$cur_bundle_key}{start} = Time::HiRes::gettimeofday();
	$b_log[$#b_log + 1] = $cur_bundle_key;
}

sub bundle_promise {
	my ($promise_type,$iter) = @_;
	if(!grep(/^$promise_type:$iter$/, @{$data{bundles}{$cur_bundle_key}{prtype}})){
		$data{bundles}{$cur_bundle_key}{prtype}[$#{$data{bundles}{$cur_bundle_key}{prtype}} + 1] = $promise_type.":".$iter;
		$data{bundles}{$cur_bundle_key}{promise_types}{$promise_type.":".$iter}{start} = Time::HiRes::gettimeofday();
	}
	if($promise_type =~ /^methods$/ && ! grep(/^$cur_bundle$/, @parent) && !defined($data{bundles}{$cur_bundle_key}{stop})){
		debug("Registering parent $cur_bundle");
		push(@parent,$cur_bundle);
		push(@parent_keys,$cur_bundle_key);
	}
	debug("Found $promise_type in bundle $cur_bundle iter $iter");
}

sub bundle_end {
	my $b = shift;
	debug("End $b");
	my $b_key = $cur_bundle_key;
	if($#parent >= 0 && $parent[$#parent] =~ /^${b}:l\d+$/) {
		debug("Found parent end $b");
		pop(@parent);
		$b_key = pop(@parent_keys);
	}
	debug("Registering end data for $b_key");
	$data{bundles}{$b_key}{level} = $#parent + 2;
	$data{bundles}{$b_key}{stop} = Time::HiRes::gettimeofday();
	$cur_bundle_key = (@parent_keys)? $parent_keys[$#parent_keys] : $cur_bundle_key;
	$cur_bundle = (@parent)? $parent[$#parent] : $cur_bundle;
	$cur_bundle_name = (split/:l\d+/,$cur_bundle)[0];
}
