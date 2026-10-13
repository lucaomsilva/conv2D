# Compiler and Flags Configuration
CXX          ?= g++
BASE_CFLAGS  ?= -Wall -std=c++17
OPENCV_CFLAGS = $(subst -I,-isystem ,$(shell pkg-config --cflags opencv4))
OPENCV_LIBS   := $(shell pkg-config --libs opencv4)
CFLAGS_O3    := -O3 $(BASE_CFLAGS) $(OPENCV_CFLAGS)

# Directories
SRC_DIR     := src
CONV_DIR    := $(SRC_DIR)/conv
TEST_DIR    := tests
BIN_DIR     := bin
SCRIPTS_DIR := scripts

# Source Files
SRC      := $(SRC_DIR)/main.cpp
SRC_TEST := $(TEST_DIR)/test_conv.cpp

# Format Files
CLANG_FORMAT ?= clang-format
FORMAT_FILES := $(shell find $(SRC_DIR) $(TEST_DIR) -name '*.cpp' -o -name '*.hpp')

# Color output
BLUE=\033[0;34m
GREEN=\033[0;32m
YELLOW=\033[0;33m
RED=\033[0;31m
NC=\033[0m # No Color

.DEFAULT_GOAL := help

##@ General

.PHONY: help
help: ## Display this help message
	@echo -e "$(BLUE)Default C++ Convolution Makefile$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(GREEN)<target>$(NC)\n"} /^[a-zA-Z_0-9\/-]+:.*?##/ { printf "  $(GREEN)%-25s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
	@echo ""

##@ Build

.PHONY: build
build: ## Compile default C++ convolution executable with -O3
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CFLAGS_O3) $(SRC) -o $(BIN_DIR)/library_o3 $(OPENCV_LIBS)

##@ Execution

.PHONY: run
run: ## Benchmark default binary via scripts/default_o3.sh
	@if [ -f "$(BIN_DIR)/library_o3" ]; then \
		bash $(SCRIPTS_DIR)/library_o3.sh; \
	else \
		echo -e "$(RED)Error: Binary $(BIN_DIR)/library_o3 not found. Run 'make build' first.$(NC)"; \
		exit 1; \
	fi

##@ Test

.PHONY: test
test: ## Compile and run unit tests using assert()
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CFLAGS_O3) $(SRC_TEST) -o $(BIN_DIR)/test_conv $(OPENCV_LIBS)
	@./$(BIN_DIR)/test_conv

##@ Formatting

.PHONY: format
format: ## Format all .cpp/.hpp files in place using the root .clang-format
	$(CLANG_FORMAT) -i $(FORMAT_FILES)
	@echo -e "$(GREEN)Formatted: $(FORMAT_FILES)$(NC)"

.PHONY: format-check
format-check: ## Check .cpp/.hpp formatting without modifying files (fails if changes are needed)
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)
	@echo -e "$(GREEN)Formatting OK$(NC)"

##@ Clean

.PHONY: clean
clean: ## Remove build artifacts
	rm -rf $(BIN_DIR)
