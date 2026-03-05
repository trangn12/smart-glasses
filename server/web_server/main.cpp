/**
 * Real-Time Subtitle - Web Server (Phase 1)
 * Serves mobile frontend, WebSocket for real-time speech→translation pipeline.
 * C++ with Boost.Beast (HTTP + WebSocket) and libcurl (MyMemory API).
 */

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <curl/curl.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
const char* STATIC_ROOT = nullptr;  // Set at startup
static unsigned short PORT = 8080;

// ---------------------------------------------------------------------------
// MyMemory API (libcurl) - Free, with profanity filter for crowd-sourced output
// ---------------------------------------------------------------------------
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
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
#ifdef __APPLE__
  /* Use system certs on macOS - DarwinSSL needs explicit native CA */
  curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: SubtitleApp/1.0");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    return std::string("[Translation API error: ") + curl_easy_strerror(res) + "]";
  }
  if (http_code != 200) {
    return std::string("[Translation API error: HTTP ") + std::to_string(http_code) + "]";
  }
  if (response.empty() || response[0] != '{') {
    return "[Translation API error: invalid response]";
  }

  if (response.find("\"quotaFinished\":true") != std::string::npos) {
    return "[Translation quota exceeded - try again tomorrow]";
  }

  // Parse MyMemory response: {"responseData":{"translatedText":"..."},...}
  size_t pos = response.find("\"translatedText\"");
  if (pos == std::string::npos) return "[Parse error]";
  pos = response.find('"', pos + 16);  // skip "translatedText" + ':'
  if (pos == std::string::npos) return "[Parse error]";
  size_t end = pos + 1;
  while (end < response.size() && response[end] != '"') {
    if (response[end] == '\\') ++end;
    ++end;
  }
  if (end >= response.size()) return "[Parse error]";

  std::string result = response.substr(pos + 1, end - pos - 1);
  // Unescape JSON: \\" -> ", \\n -> \n
  std::string out;
  for (size_t i = 0; i < result.size(); ++i) {
    if (result[i] == '\\' && i + 1 < result.size()) {
      if (result[i + 1] == '"') { out += '"'; ++i; continue; }
      if (result[i + 1] == 'n') { out += '\n'; ++i; continue; }
    }
    out += result[i];
  }
  if (contains_bad_phrase(out)) {
    return "[Translation unavailable - try again]";
  }
  return out;
}

// ---------------------------------------------------------------------------
// Simple JSON parse for {"text":"...","sourceLang":"...","targetLang":"..."}
// ---------------------------------------------------------------------------
bool parse_request(const std::string& body, std::string& text, std::string& source_lang, std::string& target_lang) {
  auto extract = [&body](const char* key) -> std::string {
    std::string pattern = std::string("\"") + key + "\"";
    size_t pos = body.find(pattern);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = body.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t start = pos + 1;
    size_t end = start;
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

// ---------------------------------------------------------------------------
// Static file serving
// ---------------------------------------------------------------------------
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
  std::string body = buf.str();

  http::response<http::string_body> res{http::status::ok, 11};
  res.set(http::field::content_type, get_mime_type(path));
  res.body() = body;
  res.prepare_payload();
  return res;
}

// ---------------------------------------------------------------------------
// WebSocket session
// ---------------------------------------------------------------------------
void do_websocket_session(beast::tcp_stream& stream, http::request<http::string_body> req) {
  websocket::stream<beast::tcp_stream> ws(std::move(stream));
  ws.set_option(websocket::stream_base::timeout{
    std::chrono::seconds(30),
    std::chrono::seconds(120),
    false
  });
  ws.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
    res.set(http::field::server, "subtitle-server");
  }));

  try {
    ws.accept(req);
  } catch (const beast::system_error&) {
    return;
  }

  try {
  for (;;) {
    beast::flat_buffer buffer;
    try {
      ws.read(buffer);
    } catch (const beast::system_error& e) {
      if (e.code() == websocket::error::closed) break;
      throw;
    }

    std::string msg = beast::buffers_to_string(buffer.data());
    std::string text, source_lang, target_lang;
    if (parse_request(msg, text, source_lang, target_lang)) {
      std::string translated = translate(text, source_lang, target_lang);
      std::string response = "{\"translated\":\"" + json_escape(translated) + "\"}";
      try {
        ws.text(true);
        ws.write(net::buffer(response));
      } catch (const beast::system_error&) {
        break;
      }
    }
  }
  } catch (const beast::system_error& e) {
    auto ec = e.code();
    if (ec == net::error::connection_reset || ec == net::error::broken_pipe ||
        ec.message().find("reset") != std::string::npos) {
      ;  // Client disconnected - normal
    } else {
      std::cerr << "WebSocket session error: " << e.what() << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << "WebSocket session error: " << e.what() << std::endl;
  }
}

// ---------------------------------------------------------------------------
// HTTP session
// ---------------------------------------------------------------------------
void do_http_session(beast::tcp_stream& stream) {
  try {
  beast::flat_buffer buffer;
  http::request<http::string_body> req;

  try {
    http::read(stream, buffer, req);
  } catch (const beast::system_error&) {
    return;
  }

  if (websocket::is_upgrade(req)) {
    auto target = req.target();
    if (target == "/ws" || target == "/ws/") {
      req.target("/ws");
      do_websocket_session(stream, std::move(req));
      return;
    }
  }

  std::string path = std::string(req.target());
  if (path.find("..") != std::string::npos) path = "/";

  // OPTIONS /translate - CORS preflight
  if (path == "/translate" && req.method() == http::verb::options) {
    http::response<http::string_body> res{http::status::ok, 11};
    res.set("Access-Control-Allow-Origin", "*");
    res.set("Access-Control-Allow-Methods", "POST, OPTIONS");
    res.set("Access-Control-Allow-Headers", "Content-Type");
    res.prepare_payload();
    http::write(stream, res);
    return;
  }

  // POST /translate - HTTP fallback when WebSocket unavailable (e.g. via tunnel)
  if (path == "/translate" && req.method() == http::verb::post) {
    std::string body = req.body();
    std::string text, source_lang, target_lang;
    if (parse_request(body, text, source_lang, target_lang)) {
      std::string translated = translate(text, source_lang, target_lang);
      std::string json_body = "{\"translated\":\"" + json_escape(translated) + "\"}";
      http::response<http::string_body> res{http::status::ok, 11};
      res.set(http::field::content_type, "application/json");
      res.set("Access-Control-Allow-Origin", "*");
      res.body() = json_body;
      res.prepare_payload();
      http::write(stream, res);
    } else {
      http::response<http::string_body> res{http::status::bad_request, 11};
      res.set(http::field::content_type, "application/json");
      res.body() = "{\"translated\":\"[Invalid request]\"}";
      res.prepare_payload();
      http::write(stream, res);
    }
    return;
  }

  if (path.empty() || path == "/") path = "/";

  http::response<http::string_body> res = serve_file(path);
  http::write(stream, res);
  } catch (const std::exception& e) {
    std::cerr << "HTTP session error: " << e.what() << std::endl;
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  fprintf(stderr, "Subtitle server starting...\n");

  std::string static_root;
  if (argc >= 2) {
    static_root = argv[1];
  } else {
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) {
      static_root = std::string(cwd) + "/client/mobile";
    } else {
      static_root = "client/mobile";
    }
  }
  STATIC_ROOT = static_root.c_str();

  if (const char* p = std::getenv("PORT"))
    PORT = static_cast<unsigned short>(std::atoi(p));

  curl_global_init(CURL_GLOBAL_DEFAULT);
  std::signal(SIGPIPE, SIG_IGN);

  try {
    net::io_context ioc{1};
    tcp::acceptor acceptor{ioc};
    acceptor.open(tcp::v4());
    acceptor.set_option(net::socket_base::reuse_address(true));
    acceptor.bind({tcp::v4(), PORT});
    acceptor.listen();

    fprintf(stderr, "Subtitle server listening on http://0.0.0.0:%d\n", PORT);
    fprintf(stderr, "Static files from: %s\n", static_root.c_str());
    std::ifstream check(static_root + "/index.html");
    if (!check) {
      fprintf(stderr, "WARNING: index.html not found at %s/index.html - run from project root (speech_to_text)\n", static_root.c_str());
    }
    fprintf(stderr, "Translation: MyMemory (free, with profanity filter)\n");
    fprintf(stderr, "Press Ctrl+C to stop.\n");

    for (;;) {
      try {
        beast::tcp_stream stream(ioc);
        acceptor.accept(stream.socket());
        std::thread([s = std::move(stream)]() mutable {
          try {
            do_http_session(s);
          } catch (const std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
          }
        }).detach();
      } catch (const std::exception& e) {
        std::cerr << "Accept error (continuing): " << e.what() << std::endl;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::string msg = e.what();
    if (msg.find("Address already in use") != std::string::npos ||
        msg.find("bind") != std::string::npos) {
      std::cerr << "Port " << PORT << " is in use. Kill the other process or use a different port." << std::endl;
    }
    curl_global_cleanup();
    return 1;
  }

  curl_global_cleanup();
  return 0;
}
