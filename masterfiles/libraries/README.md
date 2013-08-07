Layout of this `libraries` directory:

* `cfengine_stdlib.cf`: compatibility version of the CFEngine Standard Library for 3.4.x and 3.5.0 (but 3.5.0 can use `../lib/3.5/*.cf` as well)
* `../lib/MAJOR.MINOR/*.cf`: modularized version of the CFEngine Standard Library for 3.5.1 and newer.  It will *NOT* work with 3.5.0 and older!

In general, any new releases will use `lib/MAJOR.MINOR/*.cf` to include all the necessary standard library pieces.
