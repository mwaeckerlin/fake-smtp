FROM ubuntu:latest
MAINTAINER "Marc Wäckerlin"

RUN apt-get install -y netcat
ADD smtp-fake-server.sh /smtp-fake-server.sh

VOLUME /mails
EXPOSE 25
WORKDIR /
ENTRYPOINT ["/smtp-fake-server.sh", "/mails"]
