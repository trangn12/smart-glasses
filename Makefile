# Real-Time Subtitle - Python server only
.PHONY: run

run:
	@chmod +x run_server.sh 2>/dev/null || true
	@./run_server.sh
