#!/usr/bin/perl

# Author: Aleksey Tsalolikhin
#
# Detect duplicate CFEngine keys from different hosts.
# You'll ran across different hosts having the same key
# if CFEngine is baked into the image (complete with
# host key). This wreaks havoc with CFEngine reporting
# as CFEngine uses keys to identify the host.

# This script should be called by detect_duplicate_keys.sh

$|++;  # turn off output buffering

my %ip_of; # create an empty hash

while (<STDIN>) {
  ($hostkey, $ipaddress) = split;
  
  
  # if there is more than one IP address per hostkey,
  # print both IP's (so we can re-key those hosts)
  if (
       defined $ip_of{$hostkey}
       &&
       $ipaddress ne $ip_of{$hostkey}
     ) {
         print "$hostkey $ip_of{$hostkey}\n"; # original ipaddress
         print "$hostkey $ipaddress\n"; # new ipaddress

       }
  $ip_of{$hostkey} = $ipaddress;
}
