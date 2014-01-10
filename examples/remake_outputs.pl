#!/usr/bin/perl

use warnings;
use strict;
use Data::Dumper;
use Getopt::Long;
use File::Basename;

$|=1;                                   # autoflush

my %options = (
               check => 0,
               verbose => 0,
               help => 0,
               cfagent => "../cf-agent/cf-agent",
               workdir => "/tmp",
              );

GetOptions(\%options,
           "help|h!",
           "check|c!",
           "cfagent=s",
           "workdir=s",
           "verbose!",
    );

if ($options{help})
{
 print <<EOHIPPUS;
Syntax: $0 [-c|--check] [-v|--verbose] [--cfagent=PATH] [--workdir=WORKDIR] FILE1.cf FILE2.cf ...

Generate the output section of CFEngine code example.

With -c or --check, the script reports if the output is different but doesn't
write it.

With -v or --verbose, the script shows the full output of each test.

The --workdir path, defaulting to /tmp, is used for storing the example to be
run.

The --cfagent path, defaulting to ../cf-agent/cf-agent, is the cf-agent path.

Each input .cf file is scanned for three markers:

1) required: a cfengine3 code block to be run

#+begin_src cfengine3
... CFEngine code to run here ...
#+end_src

2) optionally, a prep block
(each command will be run before the cfengine3 code block)

#+begin_src prep
#@ ```
#@ touch -d '2001-02-03 12:34:56' /tmp/earlier
#@ touch -d '2002-02-03 12:34:56' /tmp/later
#@ ```
#+end_src

3) required: an output code block

#+begin_src example_output
#@ ```
#@ 2013-12-16T20:48:24+0200   notice: /default/example: R: The secret changes have been accessed after the reference time
#@ ```
#+end_src

This block is rewritten if it's different, otherwise it's left alone.

The "#@ " part is optional but needed if you want the file to be valid CFEngine
policy. The "#@ ```" parts make the prep and output steps render as code in markdown.

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

    my $base = basename($file);
    my $tempfile = "$options{workdir}/$base";
    mkdir $options{workdir};
    open my $fh, '>', $tempfile or die "Could not write to $tempfile: $!";
    print $fh $example;
    close $fh;

    foreach (@$prep)
    {
        s/^#@ //;
        s/FILE/$tempfile/g;
        print "processing $file: Running prep '$_'"
         if $options{verbose};
        system($_);
    }

    my $cmd = "$options{cfagent} -nKf $tempfile 2>&1";
    $ENV{EXAMPLE} = $base;
    open my $ofh, '-|', $cmd;
    my $output = join '', <$ofh>;
    close $ofh;

    print "Test file: $file\nCommand: $cmd\nOutput: $output\n\n\n"
     if $options{verbose};
    return $output;
}
