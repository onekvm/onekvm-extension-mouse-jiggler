#include <json-c/json.h>

#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kExtensionID = "mouse-jiggler";
constexpr std::string_view kControlSocket = "/run/onekvm/extension-control.sock";
volatile sig_atomic_t stopped = 0;

void Stop(int) { stopped = 1; }

std::string ConfigPath() {
  if (const char *directory = std::getenv("ONEKVM_EXTENSION_CONFIG");
      directory != nullptr && *directory != '\0')
    return std::string(directory) + "/config.json";
  return "/run/onekvm/extensions/mouse-jiggler/config.json";
}

bool ReadFile(const std::string &path, bool allow_missing, std::string &data,
              std::string &error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (allow_missing && errno == ENOENT)
      return true;
    error = "open " + path + ": " + std::strerror(errno);
    return false;
  }
  data.assign(std::istreambuf_iterator<char>(input),
              std::istreambuf_iterator<char>());
  if (data.size() > (1U << 20)) {
    error = path + " exceeds 1 MiB";
    return false;
  }
  return true;
}

json_object *ParseJSON(std::string_view data, std::string &error) {
  json_tokener *tokener = json_tokener_new_ex(32);
  if (tokener == nullptr) {
    error = "allocate JSON parser";
    return nullptr;
  }
  json_tokener_set_flags(tokener, JSON_TOKENER_STRICT);
  json_object *value =
      json_tokener_parse_ex(tokener, data.data(), static_cast<int>(data.size()));
  const auto status = json_tokener_get_error(tokener);
  std::size_t end = json_tokener_get_parse_end(tokener);
  json_tokener_free(tokener);
  while (end < data.size() &&
         std::isspace(static_cast<unsigned char>(data[end])))
    ++end;
  if (status != json_tokener_success || value == nullptr || end != data.size()) {
    if (value != nullptr)
      json_object_put(value);
    error = "invalid JSON";
    return nullptr;
  }
  return value;
}

bool LoadInterval(bool allow_missing, int &interval, std::string &error) {
  interval = 60;
  std::string data;
  if (!ReadFile(ConfigPath(), allow_missing, data, error))
    return false;
  if (data.empty())
    return true;
  json_object *root = ParseJSON(data, error);
  if (root == nullptr)
    return false;
  json_object *value = nullptr;
  const bool valid = json_object_is_type(root, json_type_object) &&
                     json_object_object_get_ex(root, "interval_seconds", &value) &&
                     json_object_is_type(value, json_type_int);
  if (valid)
    interval = json_object_get_int(value);
  json_object_put(root);
  if (!valid || interval < 5 || interval > 86400) {
    error = "interval_seconds must be between 5 and 86400";
    return false;
  }
  return true;
}

bool LoadIdentity(std::string &token, std::string &error) {
  const char *directory = std::getenv("CREDENTIALS_DIRECTORY");
  if (directory == nullptr || *directory == '\0') {
    error = "extension credential directory is unavailable";
    return false;
  }
  std::ifstream input(std::string(directory) + "/onekvm-token");
  if (!input) {
    error = "read extension credential";
    return false;
  }
  token.assign(std::istreambuf_iterator<char>(input),
               std::istreambuf_iterator<char>());
  while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())))
    token.pop_back();
  if (token.size() != 64) {
    error = "extension credential has invalid length";
    return false;
  }
  return true;
}

bool WriteAll(int fd, std::string_view data, std::string &error) {
  while (!data.empty()) {
    const auto written = send(fd, data.data(), data.size(), MSG_NOSIGNAL);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0) {
      error = std::string("write HID request: ") + std::strerror(errno);
      return false;
    }
    data.remove_prefix(static_cast<std::size_t>(written));
  }
  return true;
}

bool SendMouse(const std::string &token, int x, std::string &error) {
  const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    error = std::string("create HID socket: ") + std::strerror(errno);
    return false;
  }
  timeval timeout{2, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, kControlSocket.data(), kControlSocket.size());
  if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
    error = std::string("connect HID socket: ") + std::strerror(errno);
    close(fd);
    return false;
  }
  const std::string payload = "{\"buttons\":0,\"x\":" + std::to_string(x) +
                              ",\"y\":0,\"wheel\":0}";
  const std::string request =
      "POST /api/hid/mouse HTTP/1.1\r\nHost: unix\r\nContent-Type: "
      "application/json\r\nAuthorization: Bearer " +
      token + "\r\nX-OneKVM-Extension: mouse-jiggler\r\nContent-Length: " +
      std::to_string(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload;
  if (!WriteAll(fd, request, error)) {
    close(fd);
    return false;
  }
  std::string response;
  char buffer[1024];
  while (response.find("\r\n") == std::string::npos && response.size() < 8192) {
    const auto received = recv(fd, buffer, sizeof(buffer), 0);
    if (received < 0 && errno == EINTR)
      continue;
    if (received <= 0)
      break;
    response.append(buffer, static_cast<std::size_t>(received));
  }
  close(fd);
  const auto space = response.find(' ');
  int status = 0;
  if (space == std::string::npos || space + 4 > response.size() ||
      std::from_chars(response.data() + space + 1, response.data() + space + 4,
                      status)
              .ec != std::errc{} ||
      status < 200 || status >= 300) {
    error = "OneKVM HID API rejected mouse report";
    return false;
  }
  return true;
}

bool Controller(std::string &error) {
  std::string input((std::istreambuf_iterator<char>(std::cin)),
                    std::istreambuf_iterator<char>());
  json_object *root = ParseJSON(input, error);
  if (root == nullptr || !json_object_is_type(root, json_type_object)) {
    if (root != nullptr)
      json_object_put(root);
    return false;
  }
  json_object *event_value = nullptr;
  json_object *id_value = nullptr;
  json_object *enabled_value = nullptr;
  const bool valid =
      json_object_object_get_ex(root, "event", &event_value) &&
      json_object_is_type(event_value, json_type_string) &&
      json_object_object_get_ex(root, "extension_id", &id_value) &&
      json_object_is_type(id_value, json_type_string) &&
      json_object_object_get_ex(root, "enabled", &enabled_value) &&
      json_object_is_type(enabled_value, json_type_boolean);
  if (!valid || std::string_view(json_object_get_string(id_value)) != kExtensionID) {
    json_object_put(root);
    error = "unsupported controller event";
    return false;
  }
  const std::string event = json_object_get_string(event_value);
  const bool enabled = json_object_get_boolean(enabled_value) != 0;
  json_object_put(root);
  if (event != "enable" && event != "disable" && event != "reconcile" &&
      event != "config_changed") {
    error = "unsupported controller lifecycle event " + event;
    return false;
  }
  if (!enabled || event == "disable") {
    std::cout << R"({"actions":[{"type":"remove","name":"jiggler"}]})";
  } else if (event == "config_changed") {
    std::cout << R"({"actions":[{"type":"remove","name":"jiggler"},{"type":"ensure","name":"jiggler","command":["bin/onekvm-mouse-jiggler"],"restart":"on-failure"}]})";
  } else {
    std::cout << R"({"actions":[{"type":"ensure","name":"jiggler","command":["bin/onekvm-mouse-jiggler"],"restart":"on-failure"}]})";
  }
  return true;
}

int Run() {
  std::string error;
  int interval = 0;
  if (!LoadInterval(false, interval, error)) {
    std::cerr << "onekvm-mouse-jiggler: " << error << '\n';
    return 1;
  }
  std::string token;
  if (!LoadIdentity(token, error)) {
    std::cerr << "onekvm-mouse-jiggler: " << error << '\n';
    return 1;
  }
  struct sigaction action {};
  action.sa_handler = Stop;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGTERM, &action, nullptr);

  const int timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
  if (timer < 0) {
    std::cerr << "onekvm-mouse-jiggler: create timer: " << std::strerror(errno)
              << '\n';
    return 1;
  }
  itimerspec spec{};
  spec.it_value.tv_sec = interval;
  spec.it_interval.tv_sec = interval;
  if (timerfd_settime(timer, 0, &spec, nullptr) < 0) {
    std::cerr << "onekvm-mouse-jiggler: configure timer: "
              << std::strerror(errno) << '\n';
    close(timer);
    return 1;
  }
  while (!stopped) {
    std::uint64_t expirations = 0;
    if (read(timer, &expirations, sizeof(expirations)) < 0) {
      if (errno == EINTR)
        continue;
      std::cerr << "onekvm-mouse-jiggler: read timer: " << std::strerror(errno)
                << '\n';
      close(timer);
      return 1;
    }
    if (stopped)
      break;
    if (!SendMouse(token, 1, error) || !SendMouse(token, -1, error))
      std::cerr << "onekvm-mouse-jiggler: " << error << '\n';
  }
  close(timer);
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  std::string error;
  if (argc == 2 && std::string_view(argv[1]) == "--controller") {
    if (Controller(error))
      return 0;
  } else if (argc == 2 && std::string_view(argv[1]) == "--health") {
    int interval = 0;
    if (LoadInterval(true, interval, error))
      return 0;
  } else if (argc == 1) {
    return Run();
  } else {
    error = "usage: onekvm-mouse-jiggler [--controller|--health]";
  }
  std::cerr << "onekvm-mouse-jiggler: " << error << '\n';
  return 1;
}
