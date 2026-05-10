# Battle-CIn Makefile
# Single-command build for the auto-battle game

.PHONY: all build clean conan-build run-server run-client0 run-client1

BUILD_DIR := build
CMAKE_BUILD_TYPE ?= Release

# ── Default target: Conan build (installs deps + builds) ──
all: conan-build

# ── Simple CMake build (requires raylib already installed on system) ──
build:
	@echo "==> Configuring with CMake..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
	@echo "==> Building..."
	@cmake --build $(BUILD_DIR)
	@echo "==> Build complete. Binaries: $(BUILD_DIR)/meu_projeto  $(BUILD_DIR)/servidor"

# ── Conan-based build (default — installs dependencies automatically) ──
conan-build:
	@echo "==> Installing dependencies with Conan..."
	@mkdir -p $(BUILD_DIR)
	@conan install . --output-folder=$(BUILD_DIR) --build=missing
	@echo "==> Configuring with CMake (Conan toolchain)..."
	@cd $(BUILD_DIR) && cmake .. \
		-G "Unix Makefiles" \
		-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
		-DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
	@echo "==> Building..."
	@cmake --build $(BUILD_DIR)
	@echo "==> Build complete. Binaries: $(BUILD_DIR)/meu_projeto  $(BUILD_DIR)/servidor"

# ── Clean build artifacts ──
clean:
	@rm -rf $(BUILD_DIR)
	@echo "==> Build directory removed."

# ── Run targets ──
run-server:
	@cd $(BUILD_DIR) && ./servidor

run-client0:
	@cd $(BUILD_DIR) && ./meu_projeto 0

run-client1:
	@cd $(BUILD_DIR) && ./meu_projeto 1

# ── Help ──
help:
	@echo "Battle-CIn Build Commands"
	@echo "========================="
	@echo "  make              - Build with Conan (default, installs deps)"
	@echo "  make build        - Build with simple CMake (requires raylib installed)"
	@echo "  make conan-build  - Same as 'make' — build using Conan"
	@echo "  make clean        - Remove build directory"
	@echo "  make run-server   - Run the server"
	@echo "  make run-client0  - Run client as Player 0 (left side)"
	@echo "  make run-client1  - Run client as Player 1 (right side)"
	@echo ""
	@echo "Quick start:"
	@echo "  Terminal 1: make && make run-server"
	@echo "  Terminal 2: make run-client0"
	@echo "  Terminal 3: make run-client1"
