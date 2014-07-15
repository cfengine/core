#!/usr/bin/perl

use File::Basename;
use warnings;
use strict;

my %handlers = (
                c => 'c-mode',
                h => 'c-mode',
                cf => 'cfengine3-mode',
                sub => 'cfengine3-mode',
                srv => 'cfengine3-mode',
                pl => 'cperl-mode',
                pm => 'cperl-mode',
                sh => 'shell-mode',
               );

my %extra = (
             'c-mode' => [
                          '--eval' => "(c-set-style \"cfengine\")"
                         ],
            );

my %modes = map { $_ => 1 } values %handlers;
my $homedir = dirname($0);
my $mode_override;

foreach my $f (@ARGV)
{
    if (exists $modes{$f})
    {
        $mode_override = $f;
    }
    else
    {
        next unless -f $f;
    }

    my ($name,$path,$suffix) = fileparse($f, keys %handlers);

    my $mode = $handlers{$suffix} || $mode_override;

    if ($mode)
    {
        my @extra = exists $extra{$handlers{$suffix}} ? @{$extra{$handlers{$suffix}}} : ();
        my $locals = "$homedir/dir-locals.el";
        print "Reindenting $f\n";
        system('emacs', '-q',
               '--batch',
               '--eval' => "(cd \"$homedir\")",
               '-l' => 'cfengine-code-style.el',
               '-l' => 'cfengine.el',
               '--visit' => $f,
               '--eval' => "($mode)",
               @extra,
               '--eval' => "(set-variable 'indent-tabs-mode nil)",
               # '--eval' => '(untabify (point-min) (point-max))',
               '--eval' => '(indent-region (point-min) (point-max) nil)',
               '--eval' => '(delete-trailing-whitespace)',
               '--eval' => '(save-some-buffers t)');
    }
    else
    {
        warn "Sorry, can't handle file extension of $f";
    }
}
