# Quick Commands

## Run the Server

```bash
cd speech_to_text
make run
```

Or: `python3 server/serve.py`

Then open **http://localhost:8080** in Chrome.

---

## Port 8080 In Use

```bash
lsof -ti :8080 | xargs kill -9
```

Or use another port: `PORT=8081 python3 server/serve.py` (then open http://localhost:8081)

---

## Mobile (HTTPS)

1. Start server: `make run`
2. In another terminal: `tmole 8080`
3. Open the HTTPS URL in Chrome on your phone

---

## With ESP32

```bash
ESP32_IP=192.168.1.42 make run
```

Replace with your ESP32's IP.
