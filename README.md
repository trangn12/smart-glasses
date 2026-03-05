# Real-Time Subtitle Smart Glasses

Author: Noah Mathew

Speech-to-text and translation app for smart glasses. Speak into your phone, get transcribed and translated text, optionally displayed on an ESP32/OLED.

---

## What You Need

- Python 3
- Chrome browser (desktop or mobile)
- Same WiFi for all devices

---

## Quick Start (Desktop)

1. Open a terminal and go to the project folder:
   ```bash
   cd speech_to_text
   ```

2. Start the server:
   ```bash
   make run
   ```
   Or: `python3 server/serve.py`

3. Open Chrome and go to: **http://localhost:8080**

4. Click "Start Listening", allow the microphone, and speak. Transcribed and translated text will appear.

---

## Using on Your Phone

Speech recognition on mobile requires HTTPS. Use a tunnel:

1. Install Tunnelmole: `npm install -g tunnelmole`

2. Start the server (from project folder):
   ```bash
   make run
   ```

3. In a second terminal, run:
   ```bash
   tmole 8080
   ```

4. Copy the HTTPS URL shown (e.g. `https://xxx.tunnelmole.net`) and open it in **Chrome** on your phone.

5. Allow the microphone and speak.

**Important:** Use Chrome on your phone only. Safari and other browsers may not work.

---

## Sending Text to ESP32 (Optional)

To display translated text on an ESP32:

1. Open `client/oled/oled.ino` in Arduino IDE.

2. Set `WIFI_SSID` and `WIFI_PASS` to your WiFi.

3. Upload to the ESP32 and open Serial Monitor (115200 baud) to see its IP.

4. Start the server with the ESP32 IP:
   ```bash
   ESP32_IP=192.168.1.42 make run
   ```
   Replace with your ESP32's IP.

5. Speak on the app. Translated text is sent to the ESP32 via UDP.

---

## Troubleshooting

**Port 8080 in use:** Run `lsof -ti :8080 | xargs kill -9` then start the server again. Or use a different port: `PORT=8081 python3 server/serve.py` (then open http://localhost:8081).

**Page won't load from phone:** Ensure the server and phone are on the same WiFi. Check your Mac's firewall allows port 8080. Get your IP with `ifconfig | grep "inet "` and try `http://YOUR_IP:8080`.

**Microphone issues:** On Mac, if the wrong mic is used, change the input in System Settings > Sound > Input.

---

## Disclaimers

- Use Chrome on mobile. Safari and other browsers are not supported.
- All devices (server, phone, ESP32) must be on the same WiFi network.
