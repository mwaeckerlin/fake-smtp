FROM mwaeckerlin/very-base AS build
RUN $PKG_INSTALL g++
COPY fake-smtp.cpp .
RUN g++ -static -Os -flto=auto -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -Wl,-s -std=c++20 -o /fake-smtp fake-smtp.cpp
RUN strip -s -R .comment -R .gnu.version --strip-unneeded /fake-smtp
RUN mkdir /mails

FROM mwaeckerlin/scratch
ENV CONTAINERNAME="fake-smtp"
COPY --from=build --chown=${RUN_USER}:${RUN_GROUP} /mails /mails
COPY --from=build /fake-smtp /usr/bin/fake-smtp
VOLUME /mails
EXPOSE 25
ENTRYPOINT ["/usr/bin/fake-smtp", "/mails", "25"]
