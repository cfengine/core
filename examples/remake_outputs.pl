#!/usr/bin/perl

use warnings;
use strict;
use Data::Dumper;
use Getopt::Long;

$|=1;                                   # autoflush

my %options = (
               check => 0,
               help => 0,
              );

GetOptions(\%options,
           "help|h!",
           "check!",
    );

if ($options{help})
{
 print <<EOHIPPUS;
Syntax: $0 [-c|--check] FILE1.cf FILE2.cf ...

Generate the output section of CFEngine code example.

With -c or --check, the script reports if the output is different but doesn't
write it.

Each input .cf file is scanned for three markers:

1) required: a cfengine3 code block to be run

#+begin_src cfengine3
... CFEngine code to run here ...
#+end_src

2) optionally, a prep block
(each command will be run before the cfengine3 code block)

#+begin_src prep
#@ touch -a -d 2001 /tmp/earlier
#@ touch -a -d 2002 /tmp/later
#+end_src

3) required: an output code block

#+begin_src example_output
#@ 2013-12-16T20:48:24+0200   notice: /default/example: R: The secret changes have been accessed after the reference time
#+end_src

This block is rewritten if it's different, otherwise it's left alone.

The "#@ " part is optional but needed if you want the file to be valid CFEngine
policy.

EOHIPPUS

  exit;
}

my $rc = 0;
my @todo = @ARGV;

foreach my $file (@todo)
{
    open my $fh, '<', $file or warn "Could not open $file: $!";
    my $data = join '', <$fh>;
    close $fh;
    my $copy = $data;
    if ($data =~ m/#\+begin_src cfengine3\n(.+?)\n#\+end_src/s)
    {
        my $example = $1;

        my $prep;
        if ($data =~ m/#\+begin_src prep\n(.+?)\n#\+end_src/s)
        {
            $prep = [split "\n", $1];
        }

        $data =~ s/(#\+begin_src example_output\n)(.*?)(#\+end_src)/$1 . rewrite_output($file, $prep, $example, $2) . $3/es;
        if ($data ne $copy)
        {
            print "$file: output differs from original...";
            if ($options{check})
            {
                $rc = 1;
                print "\n";
                next;
            }

            open my $fh, '>', $file or warn "Could not open $file: $!";
            print $fh $data;
            close $fh;
            print "new output written!\n";
        }
    }
    else
    {
        warn "No example to run was found in $file, skipping";
    }
}

exit $rc;

sub rewrite_output
{
    my $file = shift @_;
    my $prep = shift @_;
    my $example = shift @_;
    my $old_output = shift @_;
    my $new_output = run_example($file, $prep, $example);

    if (equal_outputs($old_output, $new_output))
    {
        return $old_output;
    }

    if (defined $new_output && length $new_output > 0)
    {
        $new_output =~ s/^/#@ /mg;
    }

    return $new_output;
}

sub equal_outputs
{
    # strip out date, e.g. '2013-12-16T20:48:24+0200'
    my $x = shift @_;
    my $y = shift @_;

    $x =~ s/^(#@ )//mg;
    $x =~ s/^[-0-9T:+]+\s+//mg;
    $y =~ s/^(#@ )//mg;
    $y =~ s/^[-0-9T:+]+\s+//mg;

    return $x eq $y;
}

sub run_example
{
    my $file = shift @_;
    my $prep = shift @_ || [];
    my $example = shift @_;

    foreach (@$prep)
    {
        s/^#@ //;
        warn "processing $file: Running prep '$_'";
        system($_);
    }

    my $tempfile = '/tmp/example.cf';
    open my $fh, '>', $tempfile or die "Could not write to $tempfile: $!";
    print $fh $example;
    close $fh;

    $ENV{EXAMPLE} = $tempfile;
    open my $ofh, '-|', "../cf-agent/cf-agent -nKf $tempfile 2>&1";
    my $output = join '', <$ofh>;
    close $ofh;
    return $output;
}
