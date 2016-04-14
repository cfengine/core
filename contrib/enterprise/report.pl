#!/usr/bin/perl

# originally by Shane McEwan <shane@mcewan.id.au>
# maintained by Ted Zlatanov <tzz@lifelogs.com>

use warnings;
use strict;
use LWP::UserAgent;
use JSON;
use Data::Dumper;
use Getopt::Long;

my %outputs = (
               csv => sub { my $data = decode_json( shift ); print_csv($data, shift) },
               html => sub { my $data = decode_json( shift ); print_html($data, shift) },
               list => sub { my $data = decode_json( shift ); print_hostlist($data, shift) },
               host => sub { my $data = decode_json( shift ); print_hostinfo($data, shift) },
               ansible_emptyhostvars => sub { my $data = decode_json( shift ); print_ansible_emptyhostvars($data, shift) },
               json => sub { print @_ },
              );

my %options = (
               url => $ENV{CFENGINE_MP_URL} || 'http://admin:admin@localhost:80',
               output => 'list',
               limit => 100000,
               verbose => 0,
               list => 0,
              );

GetOptions(\%options,
           "verbose|v!",
           "output:s",
           "limit:i",           # note that you can say "LIMIT N" in the query!
           "url:s",
           "host:s",
           "list|list-hosts!"
          );

# Create a user agent object
my $ua = LWP::UserAgent->new;

my $query = shift;

if ($options{list})
{
    $query = "SELECT Hosts.HostName, Contexts.ContextName FROM Hosts JOIN Contexts ON Hosts.Hostkey = Contexts.HostKey";
}

elsif (exists $options{host})
{
    $query = "SELECT Variables.Bundle, Variables.VariableName, Variables.VariableValue FROM Variables WHERE Variables.HostKey = (SELECT Hosts.Hostkey FROM Hosts WHERE Hosts.HostName = '$options{host}')";
    $options{output} = 'host';
}

die "Syntax: $0 [--url BASEURL] [--limit N] [--output @{[ join('|', sort keys %outputs) ]}] [QUERY|--list|--host HOSTNAME]"
 unless ($query && exists $outputs{$options{output}});

$query =~ s/\v+/ /g;

# Create a request
my $url = "$options{url}/api/query";
my $req = HTTP::Request->new(POST => $url);
$req->content_type('application/x-www-form-urlencoded');
$req->content(sprintf('{ "limit": %d, "query" : "%s" }',
                      $options{limit},
                      $query));

print "Opening $url...\n" if $options{verbose};
print "Query is [$query]\n" if $options{verbose};

# Pass request to the user agent and get a response back
my $res = $ua->request($req);

unless ($res->is_success)
{
 die $res->status_line;
}

print "Got data ", $res->as_string(), "\n" if $options{verbose};

$outputs{$options{output}}->($res->content,
                             $res->header("Content-Type"));

exit 0;

sub print_csv
{
 my $data = shift;
 my $ctype = shift || 'unknown';

 print "Processing content type $ctype\n" if $options{verbose};

 foreach my $row (@{$data->{data}->[0]->{rows}})
 {
  print join ",", map { m/,/ ? "\"$_\"" : $_ } @$row;
  print "\n";
 }
}

sub print_html
{
 my $data = shift;
 my $ctype = shift || 'unknown';

 print "Processing content type $ctype\n" if $options{verbose};

 # trying to avoid requiring CPAN modules!  sorry this is so primitive!
 print "<html><body><table>\n";
 foreach my $row (@{$data->{data}->[0]->{rows}})
 {
  print "<tr>\n";
  foreach my $item (@$row)
  {
   print "<td>\n";
   print "$item\n";             # should be escaped for proper output
   print "</td>\n";
  }
  print "</tr>\n";
 }
 print "</table></body></html>\n";
}

sub print_hostlist
{
    my $data = shift;
    my %list;

    foreach my $row (@{$data->{data}->[0]->{rows}})
    {
        push @{$list{$row->[1]}}, $row->[0];
    }

    print encode_json(\%list), "\n";
}

sub print_hostinfo
{
    my $data = shift;
    my %info;

    foreach my $row (@{$data->{data}->[0]->{rows}})
    {
        $info{"$row->[0]_$row->[1]"} = $row->[2];
    }

    print encode_json(\%info), "\n";
}

sub print_ansible_emptyhostvars
{
    my $data = shift;
    my %list;

    foreach my $row (@{$data->{data}->[0]->{rows}})
    {
        push @{$list{$row->[1]}}, $row->[0];
    }

    # Insert empty _meta hostvars to speed ansible up when not using host
    # variables. It would be nice to add another output option that combined the
    # --host option for each host in the infrastructure to provide all the
    # variables at once.
    $list{'_meta'}->{'hostvars'} = {};

    print encode_json(\%list), "\n";
}
