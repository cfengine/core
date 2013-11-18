cf-profile
==========
This simple perl script is aimed at giving more information about cf-agent runs. It draws out a execution tree
together with timings and classes augmented so that you can see clearly the order of execution together with what
part of your policy cf-agent is spending most time on.

For version < 3.5.0:
```shell
# cf-agent -v | ./cf-profile.pl -a 
```

For version >= 3.5.0:
```shell
# cf-agent -v --legacy-output | ./cf-profile.pl -a
```
Requires Perl module Time::HiRes.
