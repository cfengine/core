#!/usr/bin/perl -w

# Takes two arguments, the field to get, and the user to get it from.
open(PASSWD, "< /etc/security/passwd") or die("Could not open password file");

my $in_user_section = 0;
my $hash = "";
while (<PASSWD>)
{
    if (!$in_user_section)
    {
        if (/^$ARGV[1]:/)
        {
            $in_user_section = 1;
        }
    }
    else
    {
        if (/^\S/)
        {
            $in_user_section = 0;
        }
        elsif (/$ARGV[0] *= *(\S+)/)
        {
            $hash = $1;
        }
    }
}

print("$hash\n") if ($hash);

close(PASSWD);
