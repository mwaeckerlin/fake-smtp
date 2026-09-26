# Features

Numbered register of every feature; a number is never reused. Every feature is covered by tests listed in [TESTS.md](TESTS.md); the guard `tests/docs-contract.sh` fails when a feature has no test.

- **F1 — Accepts every mail.** Every `MAIL FROM` and `RCPT TO` is accepted on port 25, without authentication and without TLS, so an application under test can send to any address.
- **F2 — One file per mail.** The raw message, headers and body, is written to `/mails/<YYYYmmddHHMMSS>-<from>-<to>.txt`, one line per line; the leading dot of a dot-stuffed line is removed, as SMTP requires. A mail that cannot be written is answered with `451` instead of `250`, so the client sees the failure.
- **F3 — Plain SMTP dialogue.** `EHLO`, `HELO`, `MAIL FROM`, `RCPT TO` answer `250 OK`, `DATA` answers `354`, `QUIT` answers `221`; any other command answers `500 ERROR, unknown command: <line>`. Commands are recognised in upper and lower case, as SMTP requires, so clients that send `ehlo` work.
- **F4 — Traffic log.** Every command and answer is written with a timestamp to `/mails/log` and to the container log.
- **F5 — Files stay in /mails.** A `/` or a control character in an address becomes `_` in the file name, so no address can place a file outside `/mails`.
- **F6 — Several clients at once.** Every connection is served on its own, so a client that waits does not block the others.
- **F7 — Headless.** The image contains one static program: no shell, no netcat, no busybox; it runs as the unprivileged user.
- **F8 — Published for amd64 and arm64.** Every push builds the image natively for both architectures and publishes it under one tag on Docker Hub, with the reusable workflow of `mwaeckerlin/scratch`.
