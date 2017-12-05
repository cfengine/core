#!/bin/sh
if [ "$TRAVIS_OS_NAME" = osx ]
then
    brew update
    brew install lmdb
    brew install gcc@7 || echo "Warning: Errors during OSX gcc install"
    gcc-7 --version
    #brew install python
    #brew install openssl
    #brew install libxml2
    #brew install fakeroot
else
    sudo rm -vf /etc/apt/sources.list.d/*riak*
    sudo apt-get --quiet update
    # Needed to build
    sudo apt-get install -y libssl-dev libpam0g-dev libtokyocabinet-dev
    # Needed to test
    sudo apt-get install -y fakeroot
    # Optional
    sudo apt-get install -y libxml2-dev libacl1-dev
    # Ensure traditional yacc compatibility
    sudo apt-get purge      -y bison
    sudo apt-get autoremove -y
    sudo apt-get install    -y byacc
fi
