#!/usr/bin/perl

use File::Basename;
use warnings;
use strict;

my %handlers = (
                c => 'c-mode',
                h => 'c-mode',
                cf => 'cfengine3-mode',
                pl => 'cperl-mode',
                pm => 'cperl-mode',
                sh => 'shell-mode',
               );

my %extra = (
             'c-mode' => [
                          '--eval' => "(c-set-style \"cfengine\")"
                         ],
            );

foreach my $f (@ARGV)
{
    next unless -f $f;
    my ($name,$path,$suffix) = fileparse($f, keys %handlers);

    if (exists $handlers{$suffix})
    {
        my @extra = exists $extra{$handlers{$suffix}} ? @{$extra{$handlers{$suffix}}} : ();
        my $locals = dirname($0) . "/dir-locals.el";
        print "Reindenting $f\n";
        system('emacs', '-q',
               '--batch',
               '-chdir' => dirname($0), # ensure we're in the right directory
               '-l' => 'cfengine-code-style.el',
               '-l' => 'cfengine.el',
               '--visit' => $f,
               '--eval' => "($handlers{$suffix})",
               @extra,
               '--eval' => '(indent-region (point-min) (point-max) nil)',
               '--eval' => '(save-some-buffers t)');
    }
    else
    {
        warn "Sorry, can't handle file extension of $f";
    }
}
