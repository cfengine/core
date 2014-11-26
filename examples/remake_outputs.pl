#!/usr/bin/perl

use warnings;
use strict;
use Data::Dumper;
use Getopt::Long;
use File::Basename;
use Sys::Hostname;

$|=1;                                   # autoflush

my %options = (
               check => 0,
               verbose => 0,
               veryverbose => 0,
               help => 0,
               cfagent => "../cf-agent/cf-agent",
               workdir => "/tmp",
              );

die ("Unknown options") unless GetOptions(\%options,
           "help|h!",
           "check|c!",
           "cfagent=s",
           "workdir=s",
           "verbose|v!",
           "veryverbose!",
    );

if ($options{help})
{
 print <<EOHIPPUS;
Syntax: $0 [-c|--check] [-v|--verbose] [--cfagent=PATH] [--workdir=WORKDIR] FILE1.cf FILE2.cf ...

Generate the output section of CFEngine code example.

With -c or --check, the script reports if the output is different but does not
write it.

With -v or --verbose, the script shows the full output of each test.  Use
--veryverbose if you REALLY want a lot of output.

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
#@ touch -t '200102031234.56' /tmp/earlier
#@ touch -t '200202031234.56' /tmp/later
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

If the output is unpredictable due to, for example, random input or network
dependencies, make sure the unpredictable line has the string RANDOM in capital
letters somewhere. That will skip the output check for that line.

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

        $data =~ s/(#\+begin_src example_output( no_check)?\n)(.*?)(#\+end_src)/$1 . rewrite_output($file, $prep, $example, $3) . $4/es;
        if (!defined($2) && $data ne $copy)
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

    if (equal_outputs($old_output, $new_output, $file))
    {
        return $old_output;
    }

    if (defined $new_output && length $new_output > 0)
    {
        $new_output =~ s/^/#@ /mg;
        $new_output = "#@ ```\n$new_output#@ ```\n";
    }

    return $new_output;
}

sub equal_outputs
{
    # strip out date, e.g. '2013-12-16T20:48:24+0200'
    my $x = shift @_;
    my $y = shift @_;
    my $file = shift @_;

    my ($tempfile, $base) = get_tempfile($file);

    $x =~ s/^#@ ```\s+//mg;
    $y =~ s/^#@ ```\s+//mg;

    $x =~ s/^(#@ )//mg;
    $x =~ s/^[-0-9T:+]+\s+//mg;
    $y =~ s/^(#@ )//mg;
    $y =~ s/^[-0-9T:+]+\s+//mg;

    $x =~ s/.*RANDOM.*//mg;
    $y =~ s/.*RANDOM.*//mg;

    if ($x ne $y)
    {
        open my $fha, '>', "$tempfile.a" or die "Could not write to diff output $tempfile.a: $!";
        print $fha $x;
        close $fha;

        open my $fhb, '>', "$tempfile.b" or die "Could not write to diff output $tempfile.b: $!";
        print $fhb $y;
        close $fhb;

        system("diff -u $tempfile.a $tempfile.b") if $options{verbose};
        return 0;
    }

    return 1;
}

sub get_tempfile
{
    my $file = shift @_;

    my $base = basename($file);
    my $tempfile = "$options{workdir}/$base";
    mkdir $options{workdir} unless -e $options{workdir};

    return ($tempfile, $base);
}

sub run_example
{
    my $file = shift @_;
    my $prep = shift @_ || [];
    my $example = shift @_;

    my ($tempfile, $base) = get_tempfile($file);
    open my $fh, '>', $tempfile or die "Could not write to $tempfile: $!";
    print $fh $example;
    close $fh;
    chmod 0600, $tempfile;

    foreach (@$prep)
    {
        s/^#@ //;
        # skip Markdown markup like ```
        next unless m/^\w/;
        s/FILE/$tempfile/g;
        s/\$HOSTNAME/hostname()/ge;
        print "processing $file: Running prep '$_'"
         if $options{verbose};
        system($_);
    }

    $ENV{EXAMPLE} = $base;
    $ENV{CFENGINE_COLOR} = 0;
    my $cmd = "$options{cfagent} -D_cfe_output_testing -nKf $tempfile 2>&1";
    open my $ofh, '-|', $cmd;
    my $output = join '', <$ofh>;
    close $ofh;

    print "Test file: $file\nCommand: $cmd\n\nNEW OUTPUT: [[[$output]]]\n\n\n"
     if $options{verbose};

    return $output;
}
