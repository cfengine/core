# setup your env like this
#craig@other:/northern.tech/cfengine/core/contrib$ env | grep ANDROID_
#ANDROID_SSH_PORT=8023
#ANDROID_SERIAL=emulator-5556

#set -x
# pre-req is termux installed with sshd configured and running
#export ANDROID_SERIAL=80e3fca9
#export ANDROID_SSH_PORT=8022 # or 8023 or whatever
#adb forward --remove-all
#adb forward tcp:8022 tcp:8022
SSH_HOST="-p $ANDROID_SSH_PORT localhost"
#SCP_HOST="-P $ANDROID_SSH_PORT localhost"
#ssh $SSH_HOST uname -a
#ssh $SSH_HOST hostname
#ssh $SSH_HOST "rm -rf ~/build" # TODO for make clean step
ssh $SSH_HOST "mkdir -p ~/build"
ssh $SSH_HOST "pkg install rsync -y liblmdb openssl libandroid-glob pcre"
rsync -auvzzP --rsh='ssh -p $ANDROID_SSH_PORT' /northern.tech/cfengine/core --exclude ".git" localhost:~/build
rsync -auvzzP --rsh='ssh -p $ANDROID_SSH_PORT' /northern.tech/cfengine/masterfiles --exclude ".git" localhost:~/build
rsync -auvzzP --rsh='ssh -p $ANDROID_SSH_PORT' /northern.tech/cfengine/buildscripts --exclude ".git" localhost:~/build
#scp -P 8022 termux-ci-device.sh localhost:~/
#ssh $SSH_HOST bash termux-ci-device.sh


