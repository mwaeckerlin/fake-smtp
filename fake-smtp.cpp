/**

fake-smtp: accepts every mail and stores it as a file, for tests.

Replaces the former bash script with netcat, so the image needs no shell.
Protocol and file layout stay the same:

  - greeting `220 Welcome to Fake SMTP Server - all mails stored in <dir>`
  - EHLO, HELO, MAIL FROM, RCPT TO answer `250 OK`, DATA `354 start mail
    input`, QUIT `221 closing channel`, anything else
    `500 ERROR, unknown command: <line>`
  - the mail goes to `<dir>/<YYYYmmddHHMMSS>-<from>-<to>.txt`, one line per
    line without the CR, with the leading dot of a dot-stuffed line removed
  - every command and answer is logged to `<dir>/log` and to stderr

Each connection is served in its own process, so a client that waits does
not block the others. A `/` or a control character in an address is replaced
by `_` in the file name, so no address can place the file outside <dir>.

Usage: fake-smtp [<dir> [<port>]]

*/

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string dir = "/mails";

std::string
now(const char *format) {
  char buffer[32];
  const std::time_t t = std::time(nullptr);
  std::strftime(buffer, sizeof buffer, format, std::localtime(&t));
  return buffer;
}

// one write per line, so the lines of parallel connections never mix
void
log(const std::string &message) {
  const std::string line = now("%Y-%m-%d %H:%M:%S") + " " + message + "\n";
  const int fd = open((dir + "/log").c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (fd >= 0) {
    if (write(fd, line.data(), line.size()) < 0) {}
    close(fd);
  }
  if (write(STDERR_FILENO, line.data(), line.size()) < 0) {}
}

std::string
safe_name(std::string name) {
  for (auto &c : name)
    if (c == '/' || static_cast<unsigned char>(c) < 0x20 || c == 0x7f) c = '_';
  return name;
}

class Connection {
public:
  explicit Connection(int fd) : fd_(fd) {}

  bool receive(std::string &line) {
    line.clear();
    char c;
    while (true) {
      const ssize_t n = read(fd_, &c, 1);
      if (n <= 0) {
        if (line.empty()) {
          log("EOF");
          return false;
        }
        break;
      }
      if (c == '\n') break;
      line += c;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    log("> " + line);
    return true;
  }

  void send(const std::string &line, bool logged = true) {
    if (logged) log("< " + line);
    const std::string data = line + "\r\n";
    for (size_t done = 0; done < data.size();) {
      const ssize_t n = write(fd_, data.data() + done, data.size() - done);
      if (n <= 0) return;
      done += static_cast<size_t>(n);
    }
  }

private:
  int fd_;
};

bool
starts_with(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

// SMTP commands are case-insensitive (RFC 5321, 2.4): `ehlo` is `EHLO`
std::string
upper(std::string s) {
  for (auto &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

void
serve(int fd) {
  Connection c(fd);
  c.send("220 Welcome to Fake SMTP Server - all mails stored in " + dir, false);
  std::string line, from, to;
  while (c.receive(line)) {
    const std::string command = upper(line);
    if (starts_with(command, "EHLO ") || starts_with(command, "HELO ")) {
      c.send("250 OK");
    } else if (command == "QUIT") {
      c.send("221 closing channel");
      break;
    } else if (starts_with(command, "MAIL FROM:")) {
      c.send("250 OK");
      from = line.substr(10);
    } else if (starts_with(command, "RCPT TO:")) {
      c.send("250 OK");
      to = line.substr(8);
    } else if (command == "DATA") {
      c.send("354 start mail input");
      const std::string name = safe_name(now("%Y%m%d%H%M%S") + "-" + from + "-" + to + ".txt");
      std::ofstream mail(dir + "/" + name, std::ios::app);
      std::string data;
      while (c.receive(data)) {
        if (data == ".") {
          // a mail that could not be stored must not be confirmed
          c.send(mail.flush() ? "250 OK" : "451 cannot store the mail in " + dir);
          break;
        }
        if (starts_with(data, ".")) data.erase(0, 1);
        log("[" + name + "] > " + data);
        mail << data << '\n';
      }
    } else {
      c.send("500 ERROR, unknown command: " + line);
    }
  }
}

}  // namespace

int main(int argc, char *argv[]) try {
  if (argc > 1) dir = argv[1];
  const int port = argc > 2 ? std::atoi(argv[2]) : 25;
  if (port < 1 || port > 65535) throw std::runtime_error("invalid port");
  mkdir(dir.c_str(), 0755);
  signal(SIGCHLD, SIG_IGN);  // served connections end without a zombie

  const int server = socket(AF_INET6, SOCK_STREAM, 0);
  if (server < 0) throw std::runtime_error("cannot open socket");
  const int no = 0, yes = 1;
  setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof no);
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = htons(static_cast<uint16_t>(port));
  if (bind(server, reinterpret_cast<sockaddr *>(&address), sizeof address) != 0)
    throw std::runtime_error("cannot listen on port " + std::to_string(port));
  if (listen(server, 64) != 0) throw std::runtime_error("cannot listen");
  log("**** opening port " + std::to_string(port));

  while (true) {
    const int client = accept(server, nullptr, nullptr);
    if (client < 0) continue;
    const pid_t pid = fork();
    if (pid == 0) {
      close(server);
      serve(client);
      close(client);
      _exit(0);
    }
    close(client);
  }
} catch (const std::exception &e) {
  std::cerr << "EXCEPTION: " << e.what() << std::endl;
  return 1;
}
