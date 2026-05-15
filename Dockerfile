FROM mwaeckerlin/very-base
VOLUME /mails
RUN mkdir /mails
RUN ${ALLOW_USER} /mails
RUN ${PKG_INSTALL} bash netcat-openbsd coreutils
RUN ln -s /usr/bin/nc /usr/bin/netcat
ADD --chmod=755 smtp-fake-server.sh /smtp-fake-server.sh

USER ${RUN_USER}
EXPOSE 25
WORKDIR /
ENTRYPOINT ["/smtp-fake-server.sh", "/mails", "25", "/tmp/mail.fifo"]
