image=docker-cfe
docker build -t docker-alpine-cfengine -f Dockerfile ../../../
docker run docker-cfe sh '/var/cfengine/bin/cf-agent -IB $(hostname -i)'
