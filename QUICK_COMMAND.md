### Quick Commands to Run/Test the Application Build

## Phase 1: Mobile Frontend + Web Server (Python)

**Run (must be inside speech_to_text folder):**
```bash
cd speech_to_text
make run
```
Or: `python3 server/web_server/serve.py`

If port 8080 is in use: `PORT=8081 python3 server/web_server/serve.py` (then open http://localhost:8081), and so on...

**Test:**
- Open http://localhost:8080 on your phone (same WiFi as host) or desktop
- Grant microphone permission
- Tap "Start Listening" and speak

**Mobile speech (HTTPS):** Run `tmole 8080` (Tunnelmole) or `trapdoor 8080` (Trapdoor) in a second terminal, then open the HTTPS URL on your phone.

## Phase 2: ESP32 Subtitle Display

**Run with ESP32:** `ESP32_IP=192.168.1.42 make run` (use your ESP32's IP)