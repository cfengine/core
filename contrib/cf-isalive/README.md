# cf-isalive

Detect and alert when CFEngine is stuck

## 1. About

Script used to detect and alert when CFEngine is stuck or dead and not applying
any promise for a while (`MAX_SECOND_LATE`)

## 2. Usage

Simply run the script. You'll only get output on failure, as illustrated below.

### 2.1. On a system where CFEngine is stuck

```
root@host:~# /etc/cron.hourly/cf-isalive
I'm late ! CFEngine did not run for 13770 seconds ! Check me !
Last /var/cfengine/promise_summary.log line:
1401544020,1401544080: Outcome of version Percolate Promises.cf 1.4 (agent-0):
    Promises observed to be kept 100%, Promises repaired 0%, Promises not
    repaired 0%
root@host:~# echo $?
1
```

### 2.2. On a system where CFEngine is in a good state

```
root@host:~# /etc/cron.hourly/cf-isalive
root@host:~# echo $?
0
```

## 3. Installation

Ideally you want to run this script hourly and get its output by email.
It's easy to do on any debian-based system:

```
root@host:~# cp cf-isalive /etc/cron.hourly/cf-isalive
root@host:~# chmod +x /etc/cron.hourly/cf-isalive
```

## 4. Licensing

cf-isalive
Copyright (C) 2012  Percolate Industries, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
