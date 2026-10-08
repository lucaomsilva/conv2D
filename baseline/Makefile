# Compiler and Flags Configuration
CC           ?= gcc
BASE_CFLAGS  ?= -Wall -fno-tree-vectorize
CFLAGS_O3    := -O3 $(BASE_CFLAGS)
CFLAGS_NO_O3 := $(BASE_CFLAGS)

# Directories
SRC_DIR     := src
CONV_DIR    := $(SRC_DIR)/conv
WRITE_DIR   := $(SRC_DIR)/write
TEST_DIR    := tests
BIN_DIR     := bin
SCRIPTS_DIR := scripts

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
	@echo -e "$(BLUE)Baseline Convolution Makefile$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(GREEN)<target>$(NC)\n"} /^[a-zA-Z_0-9\/-]+:.*?##/ { printf "  $(GREEN)%-25s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
	@echo ""

##@ Build

.PHONY: build
build: build-o3 build-no_o3 ## Compile baseline convolution executables (with -O3 and without -O3)

.PHONY: build-o3
build-o3: ## Compile baseline convolution executable with -O3 optimization
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_O3) $(CONV_DIR)/conv2d.c $(WRITE_DIR)/write.c $(SRC_DIR)/main.c -o $(BIN_DIR)/baseline_o3

.PHONY: build-no_o3
build-no_o3: ## Compile baseline convolution executable without -O3 optimization (-O0)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_NO_O3) $(CONV_DIR)/conv2d.c $(WRITE_DIR)/write.c $(SRC_DIR)/main.c -o $(BIN_DIR)/baseline_no_o3

##@ Execution

.PHONY: run
run: ## Run binary with -O3 (make run-o3) or without -O3 (make run-no_o3)
	@if [ -d "bin" ]; then \
		make run-O3; \
		make run-no-O3; \
	else \
		echo -e "$(RED)Binary folder was not created$(NC)"; \
		exit 1; \
	fi

.PHONY: run-O3
run-O3:
	@if [ -f "bin/baseline_o3" ]; then \
		bash $(SCRIPTS_DIR)/baseline_o3.sh; \
	else \
		echo -e "$(RED)Binary baseline_o3 was not created$(NC)"; \
		exit 1; \
	fi

.PHONY: run-no-O3
run-no-O3:
	@if [ -f "bin/baseline_no_o3" ]; then \
		bash $(SCRIPTS_DIR)/baseline_no_o3.sh; \
	else \
		echo -e "$(RED)Binary baseline_no_o3 was not created$(NC)"; \
		exit 1; \
	fi

##@ Test

.PHONY: test
test: ## Compile and run unit tests using assert()
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_O3) $(CONV_DIR)/conv2d.c $(WRITE_DIR)/write.c $(TEST_DIR)/test_conv.c -o $(BIN_DIR)/test_conv -lm
	@./$(BIN_DIR)/test_conv

##@ Clean

.PHONY: clean
clean: ## Remove build artifacts
	rm -rf $(BIN_DIR)
