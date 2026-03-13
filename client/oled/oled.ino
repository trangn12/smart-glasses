#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* WIFI_SSID = "Sweat Shack";
const char* WIFI_PASS = "laptopidk";

const char* SERVER_BASE_URL = "http://vonday-ip-70-165-50-20.tunnelmole.net";
const unsigned long POLL_INTERVAL_MS = 500;

String lastSubtitle = "";
unsigned long lastPollMs = 0;

// ---- OLED (SSD1306 0.96" I2C) ----
// Most 0.96" modules are 128x64 at I2C address 0x3C.
// If yours is 128x32 or uses 0x3D, adjust these.
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static void oledPrintWrapped(const String& text) {
  display.clearDisplay();
  display.setCursor(0, 0);

  // Bigger text (size 2). On 128x64 this is ~10 chars/line and 4 lines max.
  const int textSize = 2;
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);

  const int charW = 6 * textSize;
  const int charH = 8 * textSize;
  const int maxCharsPerLine = SCREEN_WIDTH / charW;
  const int maxLines = SCREEN_HEIGHT / charH;

  int lineCount = 0;
  int start = 0;
  int n = text.length();

  while (start < n && lineCount < maxLines) {
    int end = start + maxCharsPerLine;
    if (end >= n) {
      display.println(text.substring(start));
      lineCount++;
      break;
    }

    // Prefer breaking on a space
    int breakAt = -1;
    for (int i = end; i > start; i--) {
      if (text[i] == ' ') {
        breakAt = i;
        break;
      }
    }

    if (breakAt == -1) breakAt = end;

    display.println(text.substring(start, breakAt));
    lineCount++;

    // Skip spaces for next line
    start = breakAt;
    while (start < n && text[start] == ' ') start++;
  }

  display.display();
}

void setup() {
  Serial.begin(115200);

  // I2C init. On ESP32-C3 dev boards, default pins are often GPIO8 (SDA) and GPIO9 (SCL),
  // but many boards expose different pins. If you get a blank screen, set explicit pins:
  // Wire.begin(SDA_PIN, SCL_PIN);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] SSD1306 allocation/init failed");
  } else {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OLED OK");
    display.display();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected!");
}

void loop() {
  if (millis() - lastPollMs < POLL_INTERVAL_MS) return;
  lastPollMs = millis();

  HTTPClient http;
  String url = String(SERVER_BASE_URL) + "/latest-subtitle";

  if (!http.begin(url)) {
    Serial.println("[HTTP] begin failed");
    return;
  }

  int code = http.GET();
  Serial.printf("[HTTP] code=%d\n", code);

  if (code == 200) {
    String body = http.getString();
    body.trim();
    if (body.length() && body != lastSubtitle) {
      lastSubtitle = body;
      Serial.print("[Subtitle] ");
      Serial.println(body);
      oledPrintWrapped(body);
    }
  }

  http.end();
  delay(10);
}