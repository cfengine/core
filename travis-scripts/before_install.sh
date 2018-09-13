#!/bin/sh
if [ "$TRAVIS_OS_NAME" = osx ]
then
    brew update
    brew install lmdb
    set +e
    rvm get stable
    brew update
    brew install lmdb
    brew install gcc@7 || brew link --overwrite gcc@7
    set -e
    gcc-7 --version
    #brew install python
    #brew install openssl
    #brew install libxml2
    #brew install fakeroot
else
    sudo rm -vf /etc/apt/sources.list.d/*riak*
    sudo apt-get -qq update
    # Needed to build
    sudo apt-get install -y libssl-dev libpam0g-dev libtokyocabinet-dev
    # Needed to test
    sudo apt-get install fakeroot
    # Optional
    sudo apt-get install -y libxml2-dev libacl1-dev
fi
