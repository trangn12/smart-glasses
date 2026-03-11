/**
 * Real-Time Subtitle - ESP32 UDP Receiver
 * Phase 2: Receives translated text from server via UDP, displays on Serial/OLED
 *
 * Setup:
 * 1. Set WIFI_SSID and WIFI_PASS below
 * 2. Flash to ESP32
 * 3. Open Serial Monitor (115200) to see ESP32's IP
 * 4. Run server with: ESP32_IP=<esp32_ip> make run


  ********** DISCLAIMER: **********
    THIS IS A SKELETAL CODE STRUCTURE
    FOR THE ESP32 TO RECEIVE AND DISPLAY SUBTITLES
    FROM THE SERVER.
    IT IS NOT A FULLY FUNCTIONAL CODE.
    IT IS ONLY A SKELETAL CODE STRUCTURE.
    IT IS NOT A FULLY FUNCTIONAL CODE.
    ********** DISCLAIMER: **********
 */

#include <WiFi.h>
#include <WiFiUdp.h>

// --- CONFIG: Edit these variables to match your WiFi network ---
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const unsigned int UDP_PORT = 4210;

WiFiUDP udp;
char packetBuffer[512];

void setup() {
  Serial.begin(115200); // baud rate, but may be different for your device
  delay(1000);
  Serial.println("\nSubtitle Receiver - ESP32");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("UDP listening on port ");
  Serial.println(UDP_PORT);

  udp.begin(UDP_PORT);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    if (len > 0) {
      packetBuffer[len] = '\0';
      Serial.print("[Subtitle] ");
      Serial.println(packetBuffer);

      // TODO: Display on OLED (SH1106/SSD1306) for smart glasses
      // e.g. display.clear(); display.setTextSize(2); display.println(packetBuffer);
    }
  }
  delay(10);
}
