#!/bin/bash
cd "$(dirname "$0")"
pkill -f "serve.py" 2>/dev/null
lsof -ti :8080 | xargs kill -9 2>/dev/null
sleep 2

if [ ! -f "client/mobile/index.html" ]; then
  echo "ERROR: client/mobile/index.html not found. Run from speech_to_text directory."
  exit 1
fi

echo "Starting Python web server..."
echo "Open http://localhost:8080 in Chrome, Safari, or Firefox."
echo "Press Ctrl+C to stop."
echo ""

exec python3 server/serve.py
