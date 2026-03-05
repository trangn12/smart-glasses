### MOBILE WEBSITE AND FULLSTACK PIPELINE

## Author: Noah Mathew

## Project: Real-Time Subtitle Smart Glasses Using Monocular OLED Projection 

# Implementation 

## Phase 1: Mobile Frontend + Web Server ✓

- **client/mobile/** — HTML5 mobile-friendly UI, Web Speech API, WebSocket client
- **server/web_server/serve.py** — Python HTTP server, static file serving, MyMemory translation API

**Run:**
```bash
cd speech_to_text
make run
```
Or directly: `python3 server/web_server/serve.py`
Then open **http://localhost:8080** in Chrome, Safari, or Firefox (not Cursor's built-in browser).

**From your phone (same WiFi):** When you run the server, it prints your network URL. Open that on your phone to view the UI. **Note:** Speech recognition requires HTTPS on mobile. Use a tunnel (below) or desktop Chrome for full speech-to-translation testing.

**HTTPS for mobile speech (no signup):** Use **Tunnelmole** or **Trapdoor** to get a public HTTPS URL:

| Tool | Install | Run | Result |
|------|---------|-----|--------|
| **Tunnelmole** | `npm install -g tunnelmole` or `curl -O https://install.tunnelmole.com/t357g/install && sudo bash install` | `tmole 8080` | `https://xxx.tunnelmole.com` → localhost:8080 |
| **Trapdoor** | `curl -fsSL https://trapdoor.sh/install.sh \| sh` | `trapdoor 8080` | `https://xxx.trapdoor.sh` → localhost:8080 |

1. Start the server: `make run` or `python3 server/web_server/serve.py`
2. In another terminal, run `tmole 8080` (or `trapdoor 8080`)
3. Open the HTTPS URL on your phone — speech recognition will work

Translation uses MyMemory (free, no key). Profanity filter blocks bad crowd-sourced outputs.

**Local:** Open http://localhost:8080

**From phone/other device:** Use `http://YOUR_MAC_IP:8080` (e.g. `http://192.168.86.249:8080`). Both devices must be on the **same WiFi network**. If WebSocket won't connect, try `http://YOUR_MAC_IP:8080?server=YOUR_MAC_IP:8080`.

If the page won't load or stays "Connecting..." from another device:
1. Run server from project root: `cd speech_to_text && make run`
2. Allow port 8080 in **System Settings → Network → Firewall → Options** (add the server or temporarily disable)
3. Verify your Mac's IP: run `ifconfig | grep "inet "` and use the 192.168.x.x address

**Microphone:** If Chrome uses your iPhone's mic (via Continuity), change the system input in **System Settings → Sound → Input** to "MacBook Pro Microphone", or use a virtual audio app (e.g. BlackHole, Loopback) to route the correct mic.

## Phase 2: ESP32 Subtitle Display ✓

Translated text is sent to the ESP32 via **UDP** (low latency, no broker). The ESP32 runs Arduino firmware that receives and displays the text.

**Why UDP over MQTT:** Direct server→ESP32, no broker, lower latency for real-time subtitles.

**Setup:**

1. **Flash ESP32 firmware**
   - Open `oled/oled.ino` in Arduino IDE
   - Set `WIFI_SSID` and `WIFI_PASS`
   - Select board: ESP32 Dev Module
   - Upload

2. **Get ESP32 IP**
   - Open Serial Monitor (115200 baud)
   - Note the printed IP (e.g. `192.168.1.42`)

3. **Run server with ESP32 target**
   ```bash
   ESP32_IP=192.168.1.42 python3 server/web_server/serve.py
   ```
   Or: `ESP32_IP=192.168.1.42 make run`

4. **Test:** Speak on the mobile app → translated text appears in Serial Monitor (and can be wired to OLED for smart glasses)

**Hardware:** ESP32, same WiFi as server/phone. OLED display (SH1106/SSD1306) can be added for projection.

 # Disclaimers

 All wifi devices must be using the same network, sharing the same subnet mask.
  - Server, Phone, and ESP32
