# fake-smtp

A minimal fake SMTP server for testing. Accepts all incoming mails without sending them anywhere — stores each message as a plain-text file and logs all SMTP traffic.

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

## What It Does

- Listens on port **25** inside the container (mapped to `FAKE_SMTP_PORT`, default **2525**)
- Accepts every `MAIL FROM` / `RCPT TO` without authentication or TLS
- On `DATA`: writes the full raw message to `/mails/<timestamp>-<from>-<to>.txt`
- Logs every SMTP command and response to `/mails/log` and stderr
- Supports: `EHLO`, `HELO`, `MAIL FROM`, `RCPT TO`, `DATA`, `QUIT`
- Does **not** support: `AUTH`, `STARTTLS`, extensions

## Inspecting Received Mails

```bash
# list received mails
docker compose exec fake-smtp ls /mails/

# read a mail
docker compose exec fake-smtp cat /mails/<filename>

# tail the SMTP log
docker compose exec fake-smtp tail -f /mails/log

# or mount the volume on the host and access directly
```

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
