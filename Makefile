.PHONY: all build-c build-cpp build-rust build-all clean install

all: build-all

build-c:
	@echo "[*] Building C engine..."
	@$(MAKE) -s -C core install

build-cpp:
	@echo "[*] Building C++ wrapper..."
	@$(MAKE) -s -C cpp install 2>/dev/null; true

build-rust:
	@echo "[*] Building Rust wrapper..."
	@cd rustcore && cargo build --release --quiet 2>/dev/null; true
	@cp rustcore/target/release/brutepin-rust ./ 2>/dev/null; true
	@chmod +x brutepin-rust 2>/dev/null; true

build-all: build-c build-cpp build-rust
	@chmod +x bash/*.sh brute.py 2>/dev/null; true
	@echo "[+] All engines ready."

clean:
	@$(MAKE) -C core -s clean 2>/dev/null; true
	@$(MAKE) -C cpp -s clean 2>/dev/null; true
	@rm -f brutepin brutepin-cpp brutepin-rust brutepin_state.txt
	@echo "[+] Clean."

install: build-all
	@echo "[+] Build complete."
