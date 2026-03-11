# Real-Time Subtitle Smart Glasses

Author: Noah Mathew

Speech-to-text and translation app for smart glasses. Speak into your phone, get transcribed and translated text, optionally displayed on an ESP32/OLED.

---

## What You Need

- CMake, C++17 compiler
- [Boost](https://www.boost.org/) (1.70+), [libcurl](https://curl.se/libcurl/)
- Chrome browser (desktop or mobile)
- Same WiFi for all devices

**Install dependencies (macOS):** `brew install boost curl cmake`

---

## Running the Server

1. Open a terminal and go to the project folder:
   ```bash
   cd server
   ```

2. Start the server:
   ```bash
   make run
   ```
   This builds the C++ server (if needed) and starts it on port 8080.

3. Open Chrome and go to: **http://localhost:8080**

4. Click "Start Listening", allow the microphone, and speak. Transcribed and translated text will appear.

**Different port:** `PORT=8081 make run` (then open http://localhost:8081)

---

## Accessing on iPhone / iOS

Speech recognition on mobile requires HTTPS. Use Tunnelmole:

1. Install Tunnelmole: `npm install -g tunnelmole`

2. Start the server (from project folder):
   ```bash
   make run
   ```
   Or: `python3 server/serve.py`
3. In a second terminal, run:
   ```bash
   tmole 8080
   ```
   or `tmole 8080`

4. Copy the HTTPS URL shown (e.g. `https://xxx.tunnelmole.net`) and open it in **Chrome** on your iPhone.

5. Allow the microphone when prompted and speak.

**Important:** Use Chrome on your iPhone. Safari and other browsers may not support the Web Speech API for speech recognition.

---

## ESP32 Setup (Optional)

To display translated text on an ESP32 with an OLED:

1. Open `client/oled/oled.ino` in Arduino IDE.

2. Set `WIFI_SSID` and `WIFI_PASS` to your WiFi credentials.

3. Upload the sketch to the ESP32 and open Serial Monitor (115200 baud) to see its IP address.

4. Start the server with the ESP32 IP:
   ```bash
   ESP32_IP=192.168.1.42 make run
   ```
   Replace with your ESP32's IP.

5. Speak in the app. Translated text is sent to the ESP32 via UDP (port 4210 by default; set `ESP32_PORT` to change).

**Disclaimer:** The ESP32 portion has not been tested. Use at your own risk.

---

## Translation API: MyMemory

This project uses the [MyMemory Translation API](https://mymemory.translated.net/) for translation.

### How MyMemory Works

MyMemory is a **translation memory** service. It does not use a dedicated neural machine translation engine. Instead, it:

- **Searches** a large database of previously translated segments (from professional translators, crowd contributions, and web-crawled bilingual content)
- **Matches** your input text against existing translations
- **Returns** the best match, or falls back to machine translation when no good match exists

Because it relies on stored segments and crowd-sourced data, translation quality can be inconsistent—especially for novel phrases, slang, or context-dependent text. It is not as accurate as commercial APIs like Google Translate or DeepL.

### Why Use It Here

MyMemory is used in this project because it is **free to use** (with usage limits) and **requires no API key**. It is suitable for **proof-of-concept** and demonstration of live speech-to-text translation. For production or higher-quality translations, consider a paid API.

---

## Troubleshooting

**Port 8080 in use:** Run `lsof -ti :8080 | xargs kill -9` then start the server again.

**Page won't load from phone:** Ensure the server and phone are on the same WiFi. Check your Mac's firewall allows port 8080. Get your IP with `ifconfig | grep "inet "` and try `http://YOUR_IP:8080`.

**Microphone issues:** On Mac, if the wrong mic is used, change the input in System Settings > Sound > Input.

---

## Disclaimers

- Use Chrome on mobile. Safari and other browsers are not supported.
- All devices (server, phone, ESP32) must be on the same WiFi network.
