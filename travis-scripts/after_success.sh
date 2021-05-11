#!/bin/sh

# For systems with sudo access, make all files readable so that code covarage
# data is readable even for tests that were run as root.
sudo chown -R $USER  .  ||  true
