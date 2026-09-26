# fake-smtp

A minimal fake SMTP server for testing. Accepts all incoming mails without sending them anywhere — stores each message as a plain-text file and logs all SMTP traffic.

## Purpose

Use `fake-smtp` wherever your stack sends outgoing email during development or CI:

- Verify that your application actually sends an email (and to whom)
- Inspect the full raw message (headers + body) without a real MTA
- Test SMTP integration of services like [mailservice](https://github.com/mwaeckerlin/mailservice) end-to-end

## Quick Start

```bash
npm start          # foreground with logs
npm run start:daemon  # background
```

Then send a test mail:

```bash
swaks --to test@example.com --from sender@example.com \
      --server localhost --port 2525
```

Or with plain netcat:

```bash
{ echo -e "EHLO test\r\nMAIL FROM:<a@b.com>\r\nRCPT TO:<x@y.com>\r\nDATA\r\nSubject: hi\r\n\r\nBody\r\n.\r\nQUIT\r\n"; sleep 1; } \
  | nc localhost 2525
```

## Behaviour

- Listens on port **25** inside the container (mapped to `FAKE_SMTP_PORT`, default **2525**)
- Accepts every `MAIL FROM` / `RCPT TO` without authentication or TLS
- On `DATA`: writes the full raw message to `/mails/<timestamp>-<from>-<to>.txt`; a `/` or control character in an address becomes `_`, so every file stays in `/mails`; the leading dot of a dot-stuffed line is removed
- Answers `451` instead of `250` when the mail cannot be written, e.g. on a read-only volume
- Logs every SMTP command and response to `/mails/log` and stderr
- Supports: `EHLO`, `HELO`, `MAIL FROM`, `RCPT TO`, `DATA`, `QUIT`, in upper or lower case
- Serves several clients at the same time
- Does **not** support: `AUTH`, `STARTTLS`, extensions
- The image is headless — one static program, no shell — runs as an unprivileged user and is published for `linux/amd64` and `linux/arm64`

## Inspecting Received Mails

The image has no shell, so the mails are read through the volume, from another container or from the host:

```bash
# the SMTP log
$ docker compose logs -f fake-smtp

# list and read received mails through the volume
$ docker run --rm -v fake-smtp_mails:/mails:ro mwaeckerlin/very-base ls /mails/
$ docker run --rm -v fake-smtp_mails:/mails:ro mwaeckerlin/very-base cat /mails/<filename>
```

The volume name is `<project>_mails`, where `<project>` is the name of the compose project.

## Configuration

| Variable | Default | Description |
|---|---|---|
| `FAKE_SMTP_PORT` | `2525` | Host port mapped to container port 25 |

## Using in docker-compose

Point any service that sends mail to `fake-smtp:25`:

```yaml
services:
  fake-smtp:
    image: mwaeckerlin/fake-smtp
    ports:
      - "${FAKE_SMTP_PORT:-2525}:25"
    volumes:
      - mails:/mails

  myapp:
    image: myapp
    environment:
      SMTP_HOST: fake-smtp
      SMTP_PORT: 25

volumes:
  mails:
```

## End-to-End Testing with mailservice

Replace the outbound relay with `fake-smtp` to capture mails sent by postfix/mailforward without a real internet connection:

```yaml
services:
  smtp-relay:
    image: mwaeckerlin/fake-smtp   # swap real relay
    networks:
      - relay-net

  fake-smtp:
    image: mwaeckerlin/fake-smtp
    volumes:
      - mails:/mails
    networks:
      - relay-net
```

Then in your test:

1. Trigger an action that causes the application to send an email
2. Wait for a file to appear in `/mails/`
3. Assert filename (recipient), headers, and body content

## Development

```bash
$ npm run build
$ npm test
```

`npm test` checks the feature register ([FEATURES.md](FEATURES.md), [TESTS.md](TESTS.md)), the headless image contract, and runs an SMTP client with Python's `smtplib` and raw sockets against the server (`tests/e2e/`). The image is built and published by the reusable workflow of [mwaeckerlin/scratch](https://github.com/mwaeckerlin/scratch#publishing-on-docker-hub).
