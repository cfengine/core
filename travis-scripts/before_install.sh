if [[ "$TRAVIS_OS_NAME" == "osx" ]];
then
    brew update
    brew install lmdb
    brew install gcc
    #brew install python
    #brew install openssl
    #brew install libxml2
    #brew install fakeroot
else
    sudo apt-get -qq update
    # Needed to build
    sudo apt-get install -y libssl-dev libpam0g-dev libtokyocabinet-dev
    # Optional
    sudo apt-get install -y libxml2-dev libacl1-dev
fi
