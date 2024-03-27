#!/bin/bash

set -ex

# mount FSs from the host VM to /chroot folder
for i in proc sys run dev; do mkdir -p /chroot/$i; done
# special filesystem type: proc
mount --types proc /rootproc /chroot/proc
# for the following: sys, dev and run we only want changes to come from host to chroot, not the other way, so we use --make-slave
mount --bind /rootsys /chroot/sys
mount --make-slave /chroot/sys
mount --bind /rootdev /chroot/dev
mount --make-slave /chroot/dev
mount --bind /rootrun /chroot/run
mount --make-slave /chroot/run
for i in bin lib lib64 sbin; do test -d /rootfs/$i && cp -a /rootfs/$i /chroot/; done

# hack to install cfengine-nova, logged ticket ENT-11403 to make quick-install script used later for installation
# for now, contact sales if you are interested in acquiring needed tarballs
if [ -f /dunfell-cfengine-nova.tar.gz ]; then
  cd /chroot
  tar xf /dunfell-cfengine-nova.tar.gz
  cd /chroot/cfengine
  tar xf /liblmdb.tar.gz
fi

for i in $(ls -1 /rootfs/ | grep -v -E "bin|dev|lib|lib64|proc|run|sbin|sys|var"); do
  test -d /rootfs/$i && mkdir /chroot/$i && mount -o bind /rootfs/$i /chroot/$i
done
# /var is a special case as we will install CFEngine at /var/cfengine inside chroot
# so bind mount everything INSIDE /var on host to /chroot/var/. so /var/cfengine is only in the chroot
mkdir /chroot/var
for i in $(ls -1 /rootfs/var/); do
  test -d /rootfs/$i && mkdir /chroot/var/$i && mount -o bind /rootfs/var/$i /chroot/var/$i
done

# CFENGINE_HUB_IP should be provided as an ENV var from the docker run command


# entrypoint script for installing and running the agent to be executed from chroot jail
cat > /chroot/entry.sh << EOF
#!/bin/bash

set -ex

if [ -d /cfengine ]; then
  mkdir /var/cfengine
  mount -o bind /cfengine /var/cfengine
fi
if [ ! -f /var/cfengine/bin/cf-agent ]; then
  cd /tmp
  curl -O https://s3.amazonaws.com/cfengine.packages/quick-install-cfengine-enterprise.sh && bash ./quick-install-cfengine-enterprise.sh agent
fi

if ! /var/cfengine/bin/cf-key -p; then
  /var/cfengine/bin/cf-key # generate a new hostkey if none present already
fi
/var/cfengine/bin/cf-agent --bootstrap ${CFENGINE_HUB_IP} --log-level info
/var/cfengine/bin/cf-agent -KIf update.cf
/var/cfengine/bin/cf-agent -KI
/var/cfengine/bin/cf-agent -KI

while true; do sleep 3600; done
EOF
chmod +x /chroot/entry.sh

chroot /chroot/ /entry.sh | tee /entry.log
