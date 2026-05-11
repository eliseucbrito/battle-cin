# Battle-CIn Makefile

.PHONY: all build clean conan-build run-local run-solo

BUILD_DIR := build
CMAKE_BUILD_TYPE ?= Release

all: conan-build

build:
	@echo "==> Configuring with CMake..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
	@echo "==> Building..."
	@cmake --build $(BUILD_DIR)
	@echo "==> Build complete. Binary: $(BUILD_DIR)/meu_projeto"

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
	@echo "==> Build complete. Binary: $(BUILD_DIR)/meu_projeto"

clean:
	@rm -rf $(BUILD_DIR)
	@echo "==> Build directory removed."

run-local: build
	@echo "==> Starting local multiplayer..."
	@cd $(BUILD_DIR) && ./meu_projeto

run-solo: build
	@echo "==> Starting solo mode (P2 = bot)..."
	@cd $(BUILD_DIR) && ./meu_projeto --solo

help:
	@echo "Battle-CIn Build Commands"
	@echo "========================="
	@echo "  make              - Build with Conan (default, installs deps)"
	@echo "  make build        - Build with simple CMake (requires raylib installed)"
	@echo "  make conan-build  - Same as 'make'"
	@echo "  make clean        - Remove build directory"
	@echo "  make run-local    - Run local multiplayer (P1 + P2 on one keyboard)"
	@echo "  make run-solo     - Run solo mode (P1 human, P2 bot)"
	@echo ""
	@echo "Controls:"
	@echo "  P1: Arrow keys + Enter"
	@echo "  P2: WASD + Space"
