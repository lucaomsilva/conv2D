# Compiler and Flags Configuration
CC          ?= gcc
BASE_CFLAGS ?= -fopt-info-vec-optimized -Wall
CFLAGS      := -O3 -mavx2 -mfma -fopenmp $(BASE_CFLAGS)

# Directories
SRC_DIR     := src
CONV_DIR    := $(SRC_DIR)/conv
TEST_DIR    := tests
BIN_DIR     := bin
SCRIPTS_DIR := scripts

# Source Files
SRC      := $(CONV_DIR)/conv2d.c $(SRC_DIR)/main.c
SRC_TEST := $(CONV_DIR)/conv2d.c $(TEST_DIR)/test_conv.c

# Format Files
CLANG_FORMAT ?= clang-format
FORMAT_FILES := $(shell find $(SRC_DIR) $(TEST_DIR) -name '*.[ch]')

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
	@echo -e "$(BLUE)Parallel (OpenMP) 2D Convolution Makefile$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(GREEN)<target>$(NC)\n"} /^[a-zA-Z_0-9\/-]+:.*?##/ { printf "  $(GREEN)%-25s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
	@echo ""

##@ Build

.PHONY: build
build: ## Compile OpenMP + aligned SIMD variant (bin/parallel_omp <N> <threads>)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN_DIR)/parallel_omp

##@ Execution & Profiling

.PHONY: run
run: ## Benchmark parallel_omp via scripts/parallel_omp.sh (sizes x threads)
	@if [ -f "$(BIN_DIR)/parallel_omp" ]; then \
		bash $(SCRIPTS_DIR)/parallel_omp.sh; \
	else \
		echo -e "$(RED)Error: Binary $(BIN_DIR)/parallel_omp not found. Run 'make build' first.$(NC)"; \
		exit 1; \
	fi

##@ Testing & Verification

.PHONY: test
test: ## Compile and run unit tests (threads x schedules x sizes)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_TEST) -o $(BIN_DIR)/test_parallel -lm
	@echo -e "$(YELLOW)Testing Variant: OpenMP + SIMD Aligned...$(NC)"
	@./$(BIN_DIR)/test_parallel

##@ Assembly Inspection

.PHONY: asm
asm: ## Generate assembly file (.s) in bin/ for vectorization inspection
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_parallel.s
	@echo -e "$(GREEN)Assembly file generated: $(BIN_DIR)/conv2d_parallel.s$(NC)"

##@ Formatting

.PHONY: format
format: ## Format all .c/.h files in place using the root .clang-format
	$(CLANG_FORMAT) -i $(FORMAT_FILES)
	@echo -e "$(GREEN)Formatted: $(FORMAT_FILES)$(NC)"

.PHONY: format-check
format-check: ## Check .c/.h formatting without modifying files (fails if changes are needed)
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)
	@echo -e "$(GREEN)Formatting OK$(NC)"

##@ Clean

.PHONY: clean
clean: ## Remove build artifacts
	rm -rf $(BIN_DIR)
