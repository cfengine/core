#!/bin/sh

# For systems with sudo access, make all files readable so that codecovarage
# data is readable even for tests that were run as root.
sudo chown -R $USER  .  ||  true

# Code coverage by codecov.io
curl -s https://codecov.io/bash | bash
