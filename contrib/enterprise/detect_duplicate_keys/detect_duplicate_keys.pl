#!/usr/bin/perl -w

# Author: Aleksey Tsalolikhin
# Date: 27 Nov 2018
#
# Purpose: Detect duplicate CFEngine keys from different hosts.

# You'll run across different hosts having the same key
# if CFEngine is baked into the image (complete with
# host key), or if a VM is copied.
#
# Multiple hosts with the same key wreak havoc with
# CFEngine reporting as CFEngine uses keys to identify
# hosts. It appears that the host's hostname and IP address
# keeps changing!

# I run it for 30 minutes (1800 seconds) to catch
# different hosts checking in with the same hostkey.

use strict;
use warnings;
$|++;  # turn off output buffering

use Getopt::Long;
my $module; # whether to use CFEngine module protocol
            # when generating our output (we'll check
            # if we were called with a --module flag

GetOptions ( "module"  => \$module)   # flag
or die("Error in command line arguments\n");

my %ip_of; # create an empty hash
my %duplicates;
my $DEBUG = 0;  # toggle debug reporting

my $original_ipaddress;

for (1..360) {
  sleep(5);

  open DATA, "cf-key -n -s |"   or die "Couldn't execute program cf-key: $!";

# comment out the cf-key call above and uncomment the following to feed test data from file
#  open DATA, "cat test-data.txt |"   or die "Couldn't execute program: $!";

  while ( defined( my $line = <DATA> )  ) {
    chomp($line);
    if ($line =~ /^Incoming/) {
      my ($direction, $ipaddress, $hostname, $lastconnection, $hostkey) = split(/   +/, $line);
      undef $direction;      #YAGNI
      undef $hostname;       #YAGNI
      undef $lastconnection; #YAGNI

      # if there is more than one IP address per hostkey,
      # report both IP's (so we can re-key those hosts)
      if (
           defined $ip_of{$hostkey}
           &&
           $ipaddress ne $ip_of{$hostkey}
         ) {
             print "$hostkey $ip_of{$hostkey}\n" if $DEBUG == 1; # original ipaddress
             print "$hostkey $ipaddress\n" if $DEBUG == 1; # new ipaddress
             $original_ipaddress = $ip_of{$hostkey};
             $duplicates{$hostkey}{$original_ipaddress} = 1;
             $duplicates{$hostkey}{$ipaddress} = 1;
             # using a two-dimensional hash deduplicates the data,
             # so even though we might see a duplicate hostkey-IP
             # combination more than once, we'll only _report_ it
             # once.

           }
       $ip_of{$hostkey} = $ipaddress;  # note IP address belonging to hostkey
    }
  }
  close DATA;
}

# Display any hostkeys that have more than one IP address


# Use CFEngine module protocol if needed
if (defined $module) {
  print "^context=inventory_duplicate_hostkeys\n";
  print "^meta=report\n";
}

my $i=0 if defined $module;  # array index

my @keys = keys %duplicates;

foreach my $hostkey (sort keys %duplicates) {
    foreach my $ipaddress (keys %{ $duplicates{$hostkey} }) {
      if (defined $module) {
        printf("=duplicate_key[%d][ipaddress]=%s\n", $i, $ipaddress);
        printf("=duplicate_key[%d][hostkey]=%s\n", $i, $hostkey);
        $i++;
      } else {
        print "$hostkey $ipaddress\n";
      }
    }
    print "\n" unless defined $module;
}
