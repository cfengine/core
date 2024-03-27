set -ex
name=cfengine-chroot-agent
# run this script on the host, /var/cfengine/bin should only exist inside the chroot inside the agent container
test ! -d /var/cfengine/bin
docker stop $name
docker rm $name
docker rmi $name
