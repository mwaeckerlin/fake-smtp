# Changelog

- 2026-09-26 **1.0.1**
    - The image is now headless: one small static program replaces the shell script with netcat, so the image has no shell any more and runs on the runtime base
    - Several clients are served at the same time; before, the server took one connection at a time and refused the next one while it was busy
    - Commands are recognised in lower case as well, so Python's `smtplib` and other clients that send `ehlo` can send mail
    - An address can no longer place a file outside the mail directory: a `/` in it becomes `_`
    - A line starting with a dot is stored with the dot the sender meant, as SMTP requires
    - A mail that cannot be written is answered with an error instead of `250 OK`
    - The image is published for amd64 and arm64 under one tag, built and published automatically on every change and every week

- 2026-05-23 **1.0.0**
    - Fake SMTP server for tests: accepts every mail on port 25, stores it as a file in `/mails` and logs the whole dialogue
