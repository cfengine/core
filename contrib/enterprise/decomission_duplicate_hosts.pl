#!/usr/bin/perl -w
#
# MIT Public License
# http://www.opensource.org/licenses/MIT
#
# Copyright (C) 2012-2014 Tieto Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

use strict;
#use lib "/srv/xmlrpc/perl-lib";
use LWP::UserAgent;
use HTTP::Request::Common;
eval {
  require IO::Socket::SSL;
  import IO::Socket::SSL;
};
die "Info: Please install Net::SSLeay perl module\n      yum install perl-Net-SSLeay\n      apt-get install libnet-ssleay-perl\n" if $@;
use Net::SSLeay;
use MIME::Base64;
use Getopt::Long;
eval {
  require JSON;
  import JSON;
};
die "Info: Please install JSON perl module\n      yum install perl-JSON\n      apt-get install libjson-perl\n" if $@;
eval {
  require Date::Parse;
  import Date::Parse;
};
die "Info: Please install Date::Parse perl module\n      yum install perl-TimeDate\n      apt-get install libtimedate-perl\n" if $@;

my $verbose = 0;
my %keys;
my %remove;
# Generate password string with "echo -n password | base64"
my $servers = {
	"test1" => {
		url      => "http://localhost",
		username => "admin",
		password => "YWRtaW4=",
	},
	# "server2" => {
	#	url      => "https://server2.acme.com:444",
	#	username => "admin",
	#	password => "YWRtaW4=",
	#	cert     => "/var/cfengine/httpd/ssl/certs/server2.cert",
	#	},
	};

GetOptions('v|verbose' => \$verbose) || die "Usage: $0 [--verbose]\n";

###########################################################################
# Setup server certificate
#
# This is required to make this script work with libnet-http-perl version > 6
# Hostname is checked by devault in versions > 6 and we only use IP address
$ENV{PERL_LWP_SSL_VERIFY_HOSTNAME} = 0;

for my $server (sort keys %$servers) {
	print "Server: $server  ";

	# Set up server cert
	if ( exists $servers->{$server}->{cert} ) {
		eval {
			IO::Socket::SSL::set_ctx_defaults(
				verify_mode => Net::SSLeay->VERIFY_PEER(),
				ca_file => "$servers->{$server}->{cert}",
			);
		};
	}

	my $query = '{ "query": "SELECT Hosts.HostKey AS \"Host key\", Hosts.HostName AS \"Host name\", Hosts.LastReportTimeStamp AS \"Last report time\", Hosts.FirstReportTimeStamp AS \"First report-time\", Hosts.IPAddress AS \"IP address\" FROM Hosts" }';

	my $ua = new LWP::UserAgent(agent => "CLEANUP/1.0");
	my $req = HTTP::Request->new(POST => "$servers->{$server}->{url}/api/query");

	$req->authorization_basic($servers->{$server}->{username}, decode_base64($servers->{$server}->{password}));
	$req->content($query);
	my $res = $ua->request($req);
	unless ( $res->is_success ) {
		print "Query failed\n" if $verbose;
		next;
	}
	print "Query successful\n" if $verbose;
	my $decoded_json = decode_json( $res->content );

	for my $row ( @{$decoded_json->{data}[0]->{rows}} ) {
		my $ip_address = "Unknown";
		my $host_key = $row->[0];
		my $hostname = $row->[1];
		my $report_timestamp = str2time($row->[2]);
		my $first_report = str2time($row->[3]);
		$ip_address = $row->[4] if $row->[4];
		$first_report =~ s/\..*//;
		$report_timestamp =~ s/\..*//;
		# print "$host_key $hostname $first_report $report_timestamp $ip_address\n" if $verbose;
		$keys{$host_key}->{$server}->{first_report} = $first_report;
		$keys{$host_key}->{$server}->{hostname} = $hostname;
		$keys{$host_key}->{$server}->{report_timestamp} = $report_timestamp;
		$keys{$host_key}->{$server}->{ip_address} = $ip_address;
	}
}

# Remove all hosts with report_timestamp older then 90 days
for my $key ( sort keys %keys ) {
	for my $server ( sort keys %{$keys{$key}} ) {
		if ( $keys{$key}->{$server}->{report_timestamp} < time() - 60 * 60 * 24 * 90 ) {
			print "Host key $key, hostname $keys{$key}->{$server}->{hostname} on server $server is older then 90 days\n" if $verbose;
			$remove{$key}->{$server} = 1;
		}
	}
}

# Remove all duplicate keys if report_timestamp is 24 hours older then the newest one
for my $key ( sort keys %keys ) {
	my $newest = 0;
	for my $server ( sort keys %{$keys{$key}} ) {
		$newest = $keys{$key}->{$server}->{report_timestamp} if $keys{$key}->{$server}->{report_timestamp} > $newest;
	}
	for my $server ( sort keys %{$keys{$key}} ) {
		if ( $keys{$key}->{$server}->{report_timestamp} < $newest - 60 * 60 * 24 ) {
			print "Host key $key, hostname $keys{$key}->{$server}->{hostname} on server $server is a duplicate key and it is 24 hours older then the newest one\n";
			$remove{$key}->{$server} = 1;
		}
	}
}

# Remove all duplicate hostnames if report_timestamp is 24 hours older then the newest one
my %hosts;
for my $key ( sort keys %keys ) {
	for my $server ( sort keys %{$keys{$key}} ) {
		$hosts{$keys{$key}->{$server}->{hostname}}->{newest} = 0 unless exists $hosts{$keys{$key}->{$server}->{hostname}};
		$hosts{$keys{$key}->{$server}->{hostname}}->{server}->{$server}->{$key} = $keys{$key}->{$server}->{report_timestamp};
		$hosts{$keys{$key}->{$server}->{hostname}}->{newest} = $keys{$key}->{$server}->{report_timestamp} if $keys{$key}->{$server}->{report_timestamp} > $hosts{$keys{$key}->{$server}->{hostname}}->{newest};
	}
}
for my $hostname ( sort keys %hosts ) {
	for my $server ( sort keys %{$hosts{$hostname}->{server}} ) {
		for my $key ( sort keys %{$hosts{$hostname}->{server}->{$server}} ) {
			if ( $hosts{$hostname}->{server}->{$server}->{$key} < $hosts{$hostname}->{newest} - 60 * 60 * 24 ) {
				print "Host key $key, hostname $hostname on server $server is a duplicate hostname and it is 24 hours older then the newest one\n";
				$remove{$key}->{$server} = 1;
			}
		}
	}
}

# Remove hosts
for my $key ( sort keys %remove ) {
	for my $server ( sort keys %{$remove{$key}} ) {
		print "Delete key $key from server $server: " if $verbose;

		# Set up server cert
		if ( exists $servers->{$server}->{cert} ) {
			eval {
				IO::Socket::SSL::set_ctx_defaults(
					verify_mode => Net::SSLeay->VERIFY_PEER(),
					ca_file => "$servers->{$server}->{cert}",
				);
			};
		}

		my $ua = new LWP::UserAgent(agent => "CLEANUP/1.0");
		my $req = HTTP::Request->new(DELETE => "$servers->{$server}->{url}/api/host/$key");
		$req->authorization_basic($servers->{$server}->{username}, decode_base64($servers->{$server}->{password}));
		my $res = $ua->request($req);
		unless ( $res->is_success ) {
			print "Failed\n" if $verbose;
			exit 1;
		}
		print "Successful\n" if $verbose;
	}
}
