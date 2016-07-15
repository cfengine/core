#!/usr/bin/env bash
# @brief Re-install CFEngine Enterprise hub
#
# @description This quick and dirty script will re-install a cfengine hub while
# preserving the hubs license, existing trust relationshipts, upstream VCS
# configuration, and current masterfiles content.
#
# @param $1 The fully qualified path to the package the hub should be
# re-installed with.
#
# Copyright (c) 2016 Nick Anderson <nick@cmdln.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

if [[ $# -eq 0 ]]; then
    echo "Usage: $0 /path/to/cfengine-nova-hub.rpm"
    exit 1
fi
PKG=$1


CURRENT_VERSION=$(/var/cfengine/bin/cf-promises -V | awk '/Enterprise/ { print $3 }')
BACKUP=/tmp/CFEngine_Enterprise_${CURRENT_VERSION}.tar.gz
EXTRACTION_ROOT=/

hub_backup () {

# The key things to backup are the ppkeys, license, and upstream repo
# settings, the private key for the upstream repository, and masterfiles

  echo "Creating backup '${BACKUP}'"
  tar --gzip --create --file ${BACKUP} \
      /var/cfengine/ppkeys/ \
      /var/cfengine/license.dat \
      /var/cfengine/policy_server.dat \
      /var/cfengine/licenses \
      /var/cfengine/masterfiles \
      /opt/cfengine/dc-scripts/params.sh \
      /opt/cfengine/userworkdir/admin/.ssh/id_rsa.pvt \
      > /dev/null 2>&1
}

hub_restore () {
  echo "Restoring backup '${BACKUP}'"
  tar --extract --gunzip --file ${BACKUP} --directory ${EXTRACTION_ROOT}
}

hub_reinstall () {
local UNINSTALL="rpm -e cfengine-nova-hub"
local INSTALL="rpm -i ${PKG}"

  if [ -e "${PKG}" ]; then
    echo "Stopping Services"
    service cfengine3 stop
    sleep 5

    echo "Uninstalling cfengine-nova-hub"
    ${UNINSTALL} || echo "Failed to uninstall"

    echo "Cleaning up remenants"
    rm -rf /var/cfengine /opt/cfengine /var/log/CFEngineHub-Install.log /var/log/postgresql.log

    echo "Installing '${PKG}'"
    ${INSTALL}   || echo "Failed to install"

    echo "Removing newly generated ppkeys"
    rm -rf /var/cfengine/ppkeys/*

  else
    echo "Not uninstalling because '${PKG}' does not exist."
    exit 1
  fi
}

hub_rebootstrap(){
    echo "Rebootstrapping hub to $(cat /var/cfengine/policy_server.dat)"
    /var/cfengine/bin/cf-agent --bootstrap $(cat /var/cfengine/policy_server.dat)

    echo "Inititaing Policy Run"
    /var/cfengine/bin/cf-execd -OKv

    echo "Collecting hub reports"
    /var/cfengine/bin/cf-hub --query delta -H $(cat /var/cfengine/policy_server.dat) -v

}

hub_backup
hub_reinstall
hub_restore
hub_rebootstrap
