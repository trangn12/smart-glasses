#!/usr/bin/env python3
"""
Primary web server. Serves static files + translation API + UDP to ESP32.
Run: python3 serve.py (from project root) or make run
"""
import http.server
import socketserver
import json
import urllib.request
import urllib.parse
import ssl
import os
import subprocess
import socket

try:
    import certifi
    SSL_CTX = ssl.create_default_context(cafile=certifi.where())
except ImportError:
    SSL_CTX = ssl.create_default_context()

PORT = int(os.environ.get("PORT", "8080"))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(ROOT, "client", "mobile")
ESP32_PORT = int(os.environ.get("ESP32_PORT", "4210"))
ESP32_IP = os.environ.get("ESP32_IP", "")  # Set to ESP32's IP to send translated text via UDP

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIR, **kwargs)

    def do_OPTIONS(self):
        if self.path == "/translate":
            self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            return
        super().do_OPTIONS(self)

    def do_POST(self):
        if self.path == "/translate":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode()
            try:
                data = json.loads(body)
                text = data.get("text", "")
                target = data.get("targetLang", "es")
                source = data.get("sourceLang", "en")
                if source == target:
                    result = text
                else:
                    # MyMemory (LibreTranslate requires paid API key now)
                    # Filter known bad crowd-sourced outputs (profanity, wrong matches)
                    BAD_PHRASES = ("puta madre", "puta", "mierda", "carajo", "joder",
                                   "fuck", "shit", "bitch", "crap")
                    def is_bad(t):
                        t = t.lower()
                        return any(p in t for p in BAD_PHRASES)
                    try:
                        url = f"https://api.mymemory.translated.net/get?q={urllib.parse.quote(text)}&langpair={source}|{target}"
                        req = urllib.request.Request(url, headers={"User-Agent": "SubtitleApp/1.0"})
                        with urllib.request.urlopen(req, timeout=8, context=SSL_CTX) as r:
                            resp = json.loads(r.read().decode())
                            result = resp.get("responseData", {}).get("translatedText", text)
                        if is_bad(result):
                            result = "[Translation unavailable - try again]"
                    except Exception:
                        result = "[Translation service error - try again]"
                # Send translated text to ESP32 via UDP (Phase 2)
                if ESP32_IP and result:
                    try:
                        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                        msg = result.encode("utf-8")
                        if len(msg) > 512:
                            msg = msg[:512]
                        sock.sendto(msg, (ESP32_IP, ESP32_PORT))
                        sock.close()
                    except Exception as e:
                        print(f"[UDP] Failed to send to ESP32: {e}")

                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps({"translated": result}).encode())
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"translated": f"[Error: {e}]"}).encode())
            return
        self.send_error(404)

    def log_message(self, format, *args):
        print(f"[{self.log_date_time_string()}] {format % args}")

if not os.path.exists(os.path.join(DIR, "index.html")):
    print(f"ERROR: {DIR}/index.html not found. Run from project root.")
    exit(1)

# Kill anything using port 8080
try:
    result = subprocess.run(["lsof", "-ti", f":{PORT}"], capture_output=True, text=True)
    if result.stdout and result.stdout.strip():
        for pid in result.stdout.strip().split():
            subprocess.run(["kill", "-9", pid], capture_output=True)
        import time
        time.sleep(2)
except Exception:
    pass

# Allow reuse if port was just freed (TIME_WAIT)
class ReuseAddrServer(socketserver.TCPServer):
    allow_reuse_address = True

def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(0)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return None

local_ip = get_local_ip()
print(f"Serving at http://localhost:{PORT}")
if local_ip:
    print(f"On same WiFi, open on your phone: http://{local_ip}:{PORT}")
if ESP32_IP:
    print(f"ESP32 UDP: sending translations to {ESP32_IP}:{ESP32_PORT}")
else:
    print("ESP32: set ESP32_IP env to send translations via UDP")
print("(Chrome, Safari, Firefox - not Cursor's browser)")
print("Press Ctrl+C to stop.\n")

try:
    with ReuseAddrServer(("", PORT), Handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")
except OSError as e:
    if "Address already in use" in str(e) or "48" in str(e):
        print(f"\nPort {PORT} is in use. Run: lsof -ti :{PORT} | xargs kill -9")
        print("Then try again.")
    raise
