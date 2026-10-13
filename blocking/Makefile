# Compiler and Flags Configuration
CC              ?= gcc
BASE_CFLAGS     ?= -Wall
CFLAGS_SEQ      := -O3 -fno-tree-vectorize $(BASE_CFLAGS)
CFLAGS_SIMD     := -O3 -mavx2 -mfma -fopenmp-simd -fopt-info-vec-optimized -DMODE_SIMD $(BASE_CFLAGS)
CFLAGS_TEST_SEQ := -O3 -fno-tree-vectorize $(BASE_CFLAGS)
CFLAGS_TEST_SIMD:= -O3 -mavx2 -mfma -fopenmp-simd -fopt-info-vec-optimized -DMODE_SIMD $(BASE_CFLAGS)

# Directories
SRC_DIR     := src
CONV_DIR    := $(SRC_DIR)/conv
TEST_DIR    := tests
BIN_DIR     := bin
SCRIPTS_DIR := scripts

# Source Files
SRC      := $(CONV_DIR)/conv2d.c $(SRC_DIR)/main.c
SRC_TEST := $(CONV_DIR)/conv2d.c $(TEST_DIR)/test_conv.c

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
	@echo -e "$(BLUE)Blocking (Tiling) 2D Convolution Makefile$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(GREEN)<target>$(NC)\n"} /^[a-zA-Z_0-9\/-]+:.*?##/ { printf "  $(GREEN)%-25s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
	@echo ""

##@ Build

.PHONY: build
build: build-seq build-simd ## Compile both blocking executables (isolated sequential and combined SIMD)

.PHONY: build-seq
build-seq: ## Compile isolated memory tiling executable (no SIMD, -fno-tree-vectorize)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_SEQ) $(SRC) -o $(BIN_DIR)/blocking_seq

.PHONY: build-simd
build-simd: ## Compile combined memory tiling + SIMD executable (-mavx2 -fopenmp-simd -DMODE_SIMD)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_SIMD) $(SRC) -o $(BIN_DIR)/blocking_simd

##@ Execution & Profiling

.PHONY: run
run: run-seq run-simd ## Run benchmarks for both sequential and SIMD variants

.PHONY: run-seq
run-seq: ## Benchmark isolated tiling binary across block sizes via scripts/blocking_seq.sh
	@if [ -f "$(BIN_DIR)/blocking_seq" ]; then \
		bash $(SCRIPTS_DIR)/blocking_seq.sh; \
	else \
		echo -e "$(RED)Error: Binary $(BIN_DIR)/blocking_seq not found. Run 'make build-seq' first.$(NC)"; \
		exit 1; \
	fi

.PHONY: run-simd
run-simd: ## Benchmark combined tiling + SIMD binary across block sizes via scripts/blocking_simd.sh
	@if [ -f "$(BIN_DIR)/blocking_simd" ]; then \
		bash $(SCRIPTS_DIR)/blocking_simd.sh; \
	else \
		echo -e "$(RED)Error: Binary $(BIN_DIR)/blocking_simd not found. Run 'make build-simd' first.$(NC)"; \
		exit 1; \
	fi

##@ Testing & Verification

.PHONY: test
test: test-seq test-simd ## Compile and run unit tests for both blocking variants across all block sizes

.PHONY: test-seq
test-seq: ## Run unit tests against isolated sequential memory tiling
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_TEST_SEQ) $(SRC_TEST) -o $(BIN_DIR)/test_seq -lm
	@echo -e "$(YELLOW)Testing: Isolated Memory Tiling (Sequential)...$(NC)"
	@./$(BIN_DIR)/test_seq

.PHONY: test-simd
test-simd: ## Run unit tests against combined tiling + SIMD
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_TEST_SIMD) $(SRC_TEST) -o $(BIN_DIR)/test_simd -lm
	@echo -e "$(YELLOW)Testing: Combined Tiling + SIMD...$(NC)"
	@./$(BIN_DIR)/test_simd

##@ Assembly Inspection

.PHONY: asm
asm: ## Generate assembly files (.s) in bin/ for compiler vectorization and loop inspection
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_SEQ) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_seq.s
	$(CC) $(CFLAGS_SIMD) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_simd.s
	@echo -e "$(GREEN)Assembly files generated in $(BIN_DIR)/ (conv2d_seq.s, conv2d_simd.s)$(NC)"

##@ Clean

.PHONY: clean
clean: ## Remove build artifacts
	rm -rf $(BIN_DIR)
