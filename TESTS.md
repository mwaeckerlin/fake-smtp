# Tests

Register of all tests, sorted by the [FEATURES.md](FEATURES.md) number each test covers. `npm test` runs everything; the guard `tests/docs-contract.sh` fails when a feature has no test entry here or a test carries a skip marker.

## E2E (`tests/e2e/test_smtp.py` in `tests/docker-compose.yml`)

- **F1** smtplib_mail_accepted_and_stored — Python's `smtplib` sends a mail and it is accepted.
- **F2** stored_mail_has_headers_and_body, file_name_has_timestamp_from_and_to — the file carries headers and body and its name the time, sender and recipient.
- **F2** dot_stuffing_removed — `..` and `..hidden` arrive as `.` and `.hidden`.
- **F2** unstored_mail_answers_451 — on a read-only mail directory the end of `DATA` answers `451`, never `250`.
- **F3** greeting_220, commands_answer_250_and_354, quit_answers_221, unknown_command_500 — the answers of the dialogue.
- **F3** smtplib_mail_accepted_and_stored — `smtplib` sends its commands in lower case (regression: `ehlo` answered 500 and no Python client could send).
- **F4** traffic_logged — `/mails/log` holds the commands and the answers.
- **F4** log_lines_not_mixed — the lines of two parallel connections stay separate lines.
- **F5** address_cannot_leave_the_mail_directory — `MAIL FROM:<../../../tmp/escape>` is stored inside `/mails` (regression: the address was taken into the path as it was).
- **F6** parallel_clients_served — two clients send while a third holds an idle connection (regression: the server served one connection at a time).

## Image contract

- **F7** `tests/image-contract.sh` › no sh, no bash, no busybox, no perl — the image is headless.

## Workflow contract

- **F8** `tests/workflow-contract.sh` of `mwaeckerlin/scratch` — the reusable workflow selects exactly the images a repository publishes; this repository calls it from `.github/workflows/docker.yml`.
