#!/usr/bin/perl

use warnings;
use strict;

use FindBin;
use lib map { "$_/ext" } ("$FindBin::Bin/../../../../");

use plugin;
# autoflush
$| = 1;

$CFEngine::Plugin::DEBUG = 1;
print STDERR "$0: starting plugin\n";

CFEngine::Plugin::Promise::promise_loop({
                                         meta =>
                                         {
                                          authors => [ "Joe", "Frank" ],
                                          signature => "long hex string"
                                         },
                                         response =>
                                         {
                                          type => "promise",
                                          name => "simple_promise",
                                          attributes =>
                                          [
                                           {
                                            name => "x",
                                            type => "string",
                                            required => CFEngine::Plugin::json_boolean(1)
                                           },
                                           {
                                            name => "y",
                                            type => "slist",
                                            required => CFEngine::Plugin::json_boolean(0)
                                           }
                                          ]
                                         }
                                        },
                                        # verify/repair handler
                                        sub
                                        {
                                            my $repair = shift @_;
                                            my $attributes = shift @_;
                                            my $state = shift @_;

                                            # always not kept when verified
                                            return 0 unless $repair;

                                            # always repair
                                            return 1;
                                        },
                                        # shutdown handler
                                        sub
                                        {
                                            print STDERR "$0: shutting plugin down\n";
                                        });

exit 0;
