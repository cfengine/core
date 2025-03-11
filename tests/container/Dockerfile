FROM alpine
RUN apk update && apk add alpine-sdk lmdb-dev openssl-dev bison flex-dev acl-dev pcre2-dev autoconf automake libtool git python3 gdb librsync-dev
RUN mkdir /core /masterfiles
COPY core /core
COPY masterfiles /mpf
WORKDIR /core
RUN ./autogen.sh --without-pam
RUN make install
WORKDIR /mpf
RUN ./autogen.sh
RUN make install
RUN apk add procps
RUN /var/cfengine/bin/cf-key
#RUN /var/cfengine/bin/cf-agent  --debug --bootstrap $(hostname -i)
#RUN /var/cfengine/bin/cf-agent -KIf update.cf
#RUN /var/cfengine/bin/cf-agent -KI
#RUN /var/cfengine/bin/cf-agent -KI
#RUN /var/cfengine/bin/cf-agent -KI
CMD [ "sleep infinity" ]
