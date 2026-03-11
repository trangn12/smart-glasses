# Real-Time Subtitle - C++ server
.PHONY: run build

build:
	@cd server && cmake -B build && cmake --build build

run: build
	@cd server && ./build/serve
