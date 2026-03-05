# Real-Time Subtitle - Build
.PHONY: all clean run

all:
	@echo "Web server: Python (serve.py). Run: make run"

clean:
	rm -rf bin/server bin/web_server server/web_server/build

run:
	@chmod +x run_server.sh 2>/dev/null || true
	@./run_server.sh
