cf-profile
==========

This simple perl script is aimed at giving more information about cf-agent runs.
It draws out an execution tree together with timings and classes augmented so
that you can see clearly the order of execution together with what part of your
policy cf-agent is spending most time on.

**Note:** timing output must come directly from agent execution; if timing
output is taken from a cached file, e.g. `cat output | ./cf-profile.pl -a` the
timing output will be invalid.

```shell
# cf-agent -v | ./cf-profile.pl -a
```

Requires Perl module Time::HiRes.
- EL6 `yum -y install perl-Time-HiRes`
