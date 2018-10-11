#!/usr/bin/env perl

open (CFELOG, "</var/cfengine/promise_summary.log") or die;
while (<CFELOG>) {
  s/(\d+),(\d+)/localtime($1) . " - " . localtime($2)/e;
  print;
}
close (CFELOG);

# Can also be run as a one-liner:
#perl -pe 's/(\d+),(\d+)/localtime($1) . " - " . localtime($2)/e;' /var/cfengine/promise_summary.log
