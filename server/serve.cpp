/**
 * HTTP server with translation API and UDP to ESP32.
 * Build: cd server && cmake -B build && cmake --build build
 * Run: ./build/serve [path/to/client/mobile]  (default: ../client/mobile from server/)
 */

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <curl/curl.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

const char* STATIC_ROOT = nullptr;
unsigned short PORT = 8080;
std::string ESP32_IP;
unsigned short ESP32_PORT = 4210;

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, std::string* out) {
  size_t total = size * nmemb;
  out->append(static_cast<char*>(contents), total);
  return total;
}

std::string url_encode(const std::string& value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;
  for (unsigned char c : value) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << static_cast<char>(c);
    } else if (c == ' ') {
      escaped << '+';
    } else {
      escaped << '%' << std::setw(2) << static_cast<int>(c);
    }
  }
  return escaped.str();
}

std::string json_escape(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

static bool contains_bad_phrase(const std::string& s) {
  static const char* bad[] = {"puta madre", "puta", "mierda", "carajo", "joder",
                             "fuck", "shit", "bitch", "crap", nullptr};
  std::string lower = s;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  for (int i = 0; bad[i]; ++i) {
    if (lower.find(bad[i]) != std::string::npos) return true;
  }
  return false;
}

std::string translate(const std::string& text, const std::string& source_lang, const std::string& target_lang) {
  if (text.empty()) return "";
  if (source_lang == target_lang) return text;

  std::string to_translate = (text.size() > 450) ? text.substr(0, 450) : text;
  CURL* curl = curl_easy_init();
  if (!curl) return "[Curl init failed]";

  std::string encoded = url_encode(to_translate);
  std::string url = "https://api.mymemory.translated.net/get?q=" + encoded +
                    "&langpair=" + source_lang + "|" + target_lang;

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: SubtitleApp/1.0");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) return std::string("[Translation API error: ") + curl_easy_strerror(res) + "]";
  if (http_code != 200) return std::string("[Translation API error: HTTP ") + std::to_string(http_code) + "]";
  if (response.empty() || response[0] != '{') return "[Translation API error: invalid response]";
  if (response.find("\"quotaFinished\":true") != std::string::npos) return "[Translation quota exceeded]";

  size_t pos = response.find("\"translatedText\"");
  if (pos == std::string::npos) return "[Parse error]";
  pos = response.find('"', pos + 16);
  if (pos == std::string::npos) return "[Parse error]";
  size_t end = pos + 1;
  while (end < response.size() && response[end] != '"') {
    if (response[end] == '\\') ++end;
    ++end;
  }
  if (end >= response.size()) return "[Parse error]";

  std::string result = response.substr(pos + 1, end - pos - 1);
  std::string out;
  for (size_t i = 0; i < result.size(); ++i) {
    if (result[i] == '\\' && i + 1 < result.size()) {
      if (result[i + 1] == '"') { out += '"'; ++i; continue; }
      if (result[i + 1] == 'n') { out += '\n'; ++i; continue; }
    }
    out += result[i];
  }
  if (contains_bad_phrase(out)) return "[Translation unavailable - try again]";
  return out;
}

void send_udp(const std::string& msg) {
  if (ESP32_IP.empty() || msg.empty()) return;
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) return;
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(ESP32_PORT);
  inet_pton(AF_INET, ESP32_IP.c_str(), &addr.sin_addr);
  std::string to_send = msg.size() > 512 ? msg.substr(0, 512) : msg;
  sendto(sock, to_send.c_str(), to_send.size(), 0, (struct sockaddr*)&addr, sizeof(addr));
  close(sock);
}

bool parse_request(const std::string& body, std::string& text, std::string& source_lang, std::string& target_lang) {
  auto extract = [&body](const char* key) -> std::string {
    std::string pattern = std::string("\"") + key + "\"";
    size_t pos = body.find(pattern);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = body.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t start = pos + 1, end = start;
    while (end < body.size()) {
      if (body[end] == '\\') { ++end; if (end < body.size()) ++end; continue; }
      if (body[end] == '"') break;
      ++end;
    }
    return body.substr(start, end - start);
  };
  text = extract("text");
  source_lang = extract("sourceLang");
  target_lang = extract("targetLang");
  if (source_lang.empty()) source_lang = "en";
  if (target_lang.empty()) target_lang = "es";
  return !text.empty();
}

std::string get_mime_type(const std::string& path) {
  if (path.find(".css") != std::string::npos) return "text/css";
  if (path.find(".js") != std::string::npos) return "application/javascript";
  if (path.find(".html") != std::string::npos || path == "/") return "text/html";
  return "application/octet-stream";
}

http::response<http::string_body> serve_file(const std::string& path) {
  std::string file_path = std::string(STATIC_ROOT) + (path == "/" ? "/index.html" : path);
  std::ifstream f(file_path, std::ios::binary);
  if (!f) {
    http::response<http::string_body> res{http::status::not_found, 11};
    res.set(http::field::content_type, "text/plain");
    res.body() = "Not Found";
    res.prepare_payload();
    return res;
  }
  std::stringstream buf;
  buf << f.rdbuf();
  http::response<http::string_body> res{http::status::ok, 11};
  res.set(http::field::content_type, get_mime_type(path));
  res.body() = buf.str();
  res.prepare_payload();
  return res;
}

void do_session(beast::tcp_stream& stream) {
  beast::flat_buffer buffer;
  http::request<http::string_body> req;
  http::read(stream, buffer, req);

  std::string path = std::string(req.target());
  if (path.find("..") != std::string::npos) path = "/";

  if (path == "/translate" && req.method() == http::verb::options) {
    http::response<http::string_body> res{http::status::ok, 11};
    res.set("Access-Control-Allow-Origin", "*");
    res.set("Access-Control-Allow-Methods", "POST, OPTIONS");
    res.set("Access-Control-Allow-Headers", "Content-Type");
    res.prepare_payload();
    http::write(stream, res);
    return;
  }

  if (path == "/translate" && req.method() == http::verb::post) {
    std::string body = req.body();
    std::string text, source_lang, target_lang;
    std::string result;
    if (parse_request(body, text, source_lang, target_lang)) {
      result = translate(text, source_lang, target_lang);
      send_udp(result);
    } else {
      result = "[Invalid request]";
    }
    std::string json_body = "{\"translated\":\"" + json_escape(result) + "\"}";
    http::response<http::string_body> res{http::status::ok, 11};
    res.set(http::field::content_type, "application/json");
    res.set("Access-Control-Allow-Origin", "*");
    res.body() = json_body;
    res.prepare_payload();
    http::write(stream, res);
    return;
  }

  if (path.empty() || path == "/") path = "/";
  http::response<http::string_body> res = serve_file(path);
  http::write(stream, res);
}

int main(int argc, char* argv[]) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);

  static std::string static_root_storage;
  char cwd[4096];
  std::string root = getcwd(cwd, sizeof(cwd)) ? std::string(cwd) : ".";
  if (argc >= 2) {
    STATIC_ROOT = argv[1];
  } else {
    std::string cand1 = root + "/client/mobile";
    std::string cand2 = root + "/../client/mobile";
    std::ifstream f1(cand1 + "/index.html");
    std::ifstream f2(cand2 + "/index.html");
    static_root_storage = f1 ? cand1 : (f2 ? cand2 : cand1);
    STATIC_ROOT = static_root_storage.c_str();
  }

  if (const char* p = std::getenv("PORT")) PORT = static_cast<unsigned short>(std::atoi(p));
  if (const char* p = std::getenv("ESP32_IP")) ESP32_IP = p;
  if (const char* p = std::getenv("ESP32_PORT")) ESP32_PORT = static_cast<unsigned short>(std::atoi(p));

  curl_global_init(CURL_GLOBAL_DEFAULT);
  std::signal(SIGPIPE, SIG_IGN);

  net::io_context ioc{1};
  tcp::acceptor acceptor{ioc};
  acceptor.open(tcp::v4());
  acceptor.set_option(net::socket_base::reuse_address(true));
  acceptor.bind({tcp::v4(), PORT});
  acceptor.listen();

  fprintf(stderr, "Serving at http://localhost:%d\n", PORT);
  if (ESP32_IP.empty()) {
    fprintf(stderr, "ESP32: set ESP32_IP env to send translations via UDP\n");
  } else {
    fprintf(stderr, "ESP32 UDP: sending translations to %s:%d\n", ESP32_IP.c_str(), ESP32_PORT);
  }
  fprintf(stderr, "Press Ctrl+C to stop.\n\n");

  for (;;) {
    beast::tcp_stream stream(ioc);
    acceptor.accept(stream.socket());
    std::thread([s = std::move(stream)]() mutable {
      try { do_session(s); } catch (const std::exception& e) { std::cerr << e.what() << std::endl; }
    }).detach();
  }

  curl_global_cleanup();
  return 0;
}
