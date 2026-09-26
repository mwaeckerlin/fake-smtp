"""E2E tests for mwaeckerlin/fake-smtp, run inside the compose network.

Every test talks SMTP to fake-smtp:25 and checks the files the server writes
into the shared volume /mails.
"""
import glob
import os
import smtplib
import socket
import sys
import threading
import time

HOST, PORT, MAILS = "fake-smtp", 25, "/mails"
results = []


def check(name, condition, detail=""):
    results.append(condition)
    print(f"  {'PASS' if condition else 'FAIL'}  {name}" + ("" if condition else f": {detail}"))


def wait_for_server():
    for _ in range(60):
        try:
            socket.create_connection((HOST, PORT), timeout=2).close()
            return
        except OSError:
            time.sleep(1)
    raise SystemExit("fake-smtp does not listen on port 25")


def dialog(lines, host=HOST):
    """Send raw lines, return every answer line of the server."""
    with socket.create_connection((host, PORT), timeout=10) as s:
        f = s.makefile("rwb")
        answers = [f.readline().decode().rstrip("\r\n")]
        for line in lines:
            f.write(line.encode() + b"\r\n")
        f.flush()
        s.shutdown(socket.SHUT_WR)
        for raw in f:
            answers.append(raw.decode().rstrip("\r\n"))
        return answers


def mail_files(pattern="*"):
    return sorted(p for p in glob.glob(os.path.join(MAILS, pattern)) if not p.endswith("/log"))


def wait_for_file(pattern, timeout=10):
    deadline = time.time() + timeout
    while time.time() < deadline:
        found = mail_files(pattern)
        if found:
            return found
        time.sleep(0.2)
    return []


wait_for_server()
print("==> E2E: fake SMTP server")

# F1, F2: a mail sent with smtplib is accepted and stored with headers and body
with smtplib.SMTP(HOST, PORT, timeout=10) as client:
    client.sendmail("alice@example.com", ["bob@example.com"],
                    "Subject: e2e hello\r\nFrom: alice@example.com\r\n\r\nfirst line\r\nsecond line\r\n")
stored = wait_for_file("*alice@example.com*bob@example.com*.txt")
check("smtplib_mail_accepted_and_stored", len(stored) == 1, f"files: {mail_files()}")
content = open(stored[0]).read() if stored else ""
check("stored_mail_has_headers_and_body", "Subject: e2e hello" in content and "first line\nsecond line" in content, content)
check("file_name_has_timestamp_from_and_to", bool(stored) and os.path.basename(stored[0])[:14].isdigit()
      and os.path.basename(stored[0]).endswith("-<alice@example.com>-<bob@example.com>.txt"), stored)

# F2: dot stuffing: a line that starts with a dot arrives with one dot less
answers = dialog(["HELO test", "MAIL FROM:<dots@example.com>", "RCPT TO:<x@example.com>", "DATA",
                  "Subject: dots", "", "..", "..hidden", ".", "QUIT"])
dots = wait_for_file("*dots@example.com*")
dot_content = open(dots[0]).read() if dots else ""
check("dot_stuffing_removed", dot_content.endswith("\n.\n.hidden\n"), repr(dot_content))

# F3: every command answers as documented, unknown ones with 500
check("greeting_220", answers[0].startswith("220 "), answers)
check("commands_answer_250_and_354", answers[1:5] == ["250 OK", "250 OK", "250 OK", "354 start mail input"], answers)
check("quit_answers_221", answers[-1] == "221 closing channel", answers)
unknown = dialog(["NOOPS", "QUIT"])
check("unknown_command_500", unknown[1] == "500 ERROR, unknown command: NOOPS", unknown)

# F4: the traffic is logged to /mails/log
log = open(os.path.join(MAILS, "log")).read() if os.path.exists(os.path.join(MAILS, "log")) else ""
check("traffic_logged", "> mail FROM:<alice@example.com>" in log and "< 250 OK" in log, log[-500:])
lines = [line for line in log.splitlines() if line]
check("log_lines_not_mixed", all(line[:4].isdigit() and line.count(" > ") + line.count(" < ") <= 2 for line in lines)
      and all(line.count("2026-") <= 1 or "[" in line for line in lines), [l for l in lines if not l[:4].isdigit()][:5])

# F5: an address cannot place the file outside /mails
dialog(["HELO test", "MAIL FROM:<../../../tmp/escape>", "RCPT TO:<a/../../b@example.com>", "DATA",
        "Subject: escape", "", "body", ".", "QUIT"])
escaped = wait_for_file("*escape*")
check("address_cannot_leave_the_mail_directory", len(escaped) == 1 and os.path.dirname(escaped[0]) == MAILS
      and "/" not in os.path.basename(escaped[0])[len("YYYYmmddHHMMSS-"):], escaped)

# F6: two clients at the same time are both served
def send(n):
    with smtplib.SMTP(HOST, PORT, timeout=20) as c:
        c.sendmail(f"parallel{n}@example.com", ["x@example.com"], f"Subject: p{n}\r\n\r\nbody\r\n")

slow = socket.create_connection((HOST, PORT), timeout=10)   # an idle client holds a connection
slow.recv(100)
threads = [threading.Thread(target=send, args=(n,)) for n in (1, 2)]
for t in threads:
    t.start()
for t in threads:
    t.join(30)
slow.close()
check("parallel_clients_served", bool(wait_for_file("*parallel1@*")) and bool(wait_for_file("*parallel2@*")), mail_files())

# F2: a mail that cannot be stored is not confirmed
refused = dialog(["HELO test", "MAIL FROM:<a@example.com>", "RCPT TO:<b@example.com>", "DATA",
                  "Subject: lost", "", "body", ".", "QUIT"], host="fake-smtp-readonly")
check("unstored_mail_answers_451", any(a.startswith("451 ") for a in refused) and "250 OK" not in refused[4:], refused)

print(f"\n==> E2E results: {results.count(True)} passed, {results.count(False)} failed")
sys.exit(0 if all(results) else 1)
