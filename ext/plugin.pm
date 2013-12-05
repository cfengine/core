package CFEngine::Plugin;

our $VERSION = "0.0.2";
our $DEBUG = 0;

use Data::Dumper;
use JSON;
use IO::Select;

use constant CODER => JSON->new()->allow_barekey()->relaxed()->utf8()->allow_nonref();
use constant CAN_CODER => JSON->new()->canonical()->utf8()->allow_nonref();

sub debug
{
    return unless $CFEngine::Plugin::DEBUG;

    print STDERR @_;
}

sub protocol_line
{
    my $s = IO::Select->new();
    $s->add(\*STDIN);

    # 15-second timeout on any commands
    my @ready = $s->can_read(15);

    die unless scalar @ready;

    my $line = <STDIN>;
    chomp $line;
    return unless length $line;

    debug("<<< $line\n");

    my $data;

    eval
    {
        $data = CODER->decode($line);
    };

    if ($@)
    {
        die "Malformed JSON data: $@";
    }

    die "Malformed protocol line (must be a key-value map): $line"
     unless ref $data eq 'HASH';

    my $v = $data->{cfe_module_protocol_version} || $data->{cmpv} || 'unknown';

    die "Mismatched protocol version: expected '$VERSION', got '$v'"
     unless $v eq $VERSION;

    die "Missing command in input: $line"
     unless exists $data->{command};

    return $data;
}

sub make_protocol_line
{
    my $success = shift @_;

    # leave null alone
    $success = json_boolean($success) if defined $success;

    my $data = shift @_ || {};

    my %data = (cmpv => $VERSION, success => $success, %$data);

    return CAN_CODER->encode(\%data) . "\n";
}

sub json_boolean
{
    my $v = shift;
    return $v ? JSON::true : JSON::false;
}

package CFEngine::Plugin::Promise;

sub promise_loop
{
    my $init_data = shift @_;
    my $verify_repair_handler = shift @_;
    my $shutdown_handler = shift @_;

    my $have_initalize = 0;
    my $have_verify = 0;
    my $state;
    my $attributes;

    while (1)
    {
        my $in = CFEngine::Plugin::protocol_line();

        last unless $in;

        my $command = $in->{command};

        $state = $in->{state} if exists $in->{state};

        if (exists $in->{attributes})
        {
            die "Attributes can only be specified once in the exchange!"
             if defined $attributes;

            $attributes = $in->{attributes};
         }


        if ($command eq 'initialize' && !$have_initialize)
        {
            $have_initialize = 1;

            print CFEngine::Plugin::make_protocol_line(1, $init_data);
        }
        elsif ($command eq 'initialize' && $have_initialize)
        {
            die "Got initialize command twice, aborting";
        }
        elsif ($command ne 'initialize' && !$have_initialize)
        {
            die "Got command '$command' before 'initialize', aborting";
        }

        if ($command eq 'shutdown')
        {
            print CFEngine::Plugin::make_protocol_line(1,
                                                       {
                                                        shutdown => CFEngine::Plugin::json_boolean(1),
                                                       });
            last;
        }

        if ($command eq 'verify')
        {
            $have_verify = 1;

            die "Got 'verify' command without attributes"
             unless defined $attributes;

            my $success = $verify_repair_handler->(0, $attributes, $state);
            print CFEngine::Plugin::make_protocol_line($success,
                                                       {
                                                        command => $command,
                                                        log_INFORM => ["verifying simple promise"],
                                                        state => $state,
                                                        attributes => $attributes,
                                                       });
        }

        if ($command eq 'repair')
        {
            die "Got 'repair' command before 'verify'"
             unless $have_verify;

            die "Got 'repair' command without attributes"
             unless defined $attributes;

            my $success = $verify_repair_handler->(1, $attributes, $state);
            print CFEngine::Plugin::make_protocol_line($success,
                                                       {
                                                        command => $command,
                                                        log_INFORM => ["repairing simple promise"],
                                                        state => $state,
                                                        attributes => $attributes,
                                                       });
        }
    }
}

1;
