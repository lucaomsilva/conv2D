# Compiler and Flags Configuration
CC               ?= gcc
BASE_CFLAGS      ?= -fopt-info-vec-optimized -Wall
CFLAGS_AUTOVEC   := -O3 -mavx2 -mfma  $(BASE_CFLAGS)
CFLAGS_UNALIGNED := -O3 -mavx2 -mfma -fopenmp-simd -DMODE_UNALIGNED $(BASE_CFLAGS)
CFLAGS_ALIGNED   := -O3 -mavx2 -mfma -fopenmp-simd -DMODE_ALIGNED $(BASE_CFLAGS)
CFLAGS_TEST      := -O3 -mavx2 -mfma -fopenmp-simd $(BASE_CFLAGS)

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
	@echo -e "$(BLUE)Vectorize (SIMD) 2D Convolution Makefile$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(GREEN)<target>$(NC)\n"} /^[a-zA-Z_0-9\/-]+:.*?##/ { printf "  $(GREEN)%-25s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
	@echo ""

##@ Build

.PHONY: build
build: build-autovec build-unaligned build-aligned ## Compile all 3 SIMD variants (autovec, unaligned, aligned)

.PHONY: build-autovec
build-autovec: ## Compile auto-vectorized variant (no pragmas, standard malloc)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_AUTOVEC) $(SRC) -o $(BIN_DIR)/simd_autovec

.PHONY: build-unaligned
build-unaligned: ## Compile OpenMP SIMD unaligned pragma variant (-DMODE_UNALIGNED)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_UNALIGNED) $(SRC) -o $(BIN_DIR)/simd_unaligned

.PHONY: build-aligned
build-aligned: ## Compile OpenMP SIMD aligned pragma variant with 32-byte alignment (-DMODE_ALIGNED)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_ALIGNED) $(SRC) -o $(BIN_DIR)/simd_aligned

##@ Execution & Profiling

.PHONY: run
run: run-all ## Run benchmarks for all SIMD variants

.PHONY: run-all
run-all: run-autovec run-unaligned run-aligned ## Run benchmarks for autovec, unaligned, and aligned

.PHONY: run-autovec
run-autovec: ## Benchmark auto-vectorized binary via scripts/simd_autovec.sh
	@if [ -f "$(BIN_DIR)/simd_autovec" ]; then \
		bash $(SCRIPTS_DIR)/simd_autovec.sh; \
	else \
		echo -e "$(RED)Error: Binary $(BIN_DIR)/simd_autovec not found. Run 'make build-autovec' first.$(NC)"; \
		exit 1; \
	fi

.PHONY: run-unaligned
run-unaligned: ## Benchmark unaligned SIMD binary via scripts/simd_unaligned.sh
	@if [ -f "$(BIN_DIR)/simd_unaligned" ]; then \
		bash $(SCRIPTS_DIR)/simd_unaligned.sh; \
	else \
		echo -e "$(RED)Error: Binary $(BIN_DIR)/simd_unaligned not found. Run 'make build-unaligned' first.$(NC)"; \
		exit 1; \
	fi

.PHONY: run-aligned
run-aligned: ## Benchmark aligned SIMD binary via scripts/simd_aligned.sh
	@if [ -f "$(BIN_DIR)/simd_aligned" ]; then \
		bash $(SCRIPTS_DIR)/simd_aligned.sh; \
	else \
		echo -e "$(RED)Error: Binary $(BIN_DIR)/simd_aligned not found. Run 'make build-aligned' first.$(NC)"; \
		exit 1; \
	fi

##@ Testing & Verification

.PHONY: test
test: test-autovec test-unaligned test-aligned ## Compile and run unit tests for all 3 SIMD variants

.PHONY: test-autovec
test-autovec: ## Run unit tests against auto-vectorized variant
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_TEST) $(SRC_TEST) -o $(BIN_DIR)/test_autovec -lm
	@echo -e "$(YELLOW)Testing Variant: Auto-Vectorization...$(NC)"
	@./$(BIN_DIR)/test_autovec

.PHONY: test-unaligned
test-unaligned: ## Run unit tests against unaligned SIMD pragma variant
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_TEST) -DMODE_UNALIGNED $(SRC_TEST) -o $(BIN_DIR)/test_unaligned -lm
	@echo -e "$(YELLOW)Testing Variant: SIMD Pragma (Unaligned)...$(NC)"
	@./$(BIN_DIR)/test_unaligned

.PHONY: test-aligned
test-aligned: ## Run unit tests against aligned SIMD pragma variant
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_TEST) -DMODE_ALIGNED $(SRC_TEST) -o $(BIN_DIR)/test_aligned -lm
	@echo -e "$(YELLOW)Testing Variant: SIMD Pragma (32-byte Aligned)...$(NC)"
	@./$(BIN_DIR)/test_aligned

##@ Assembly Inspection

.PHONY: asm
asm: ## Generate assembly files (.s) in bin/ for compiler vectorization inspection
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_AUTOVEC) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_autovec.s
	$(CC) $(CFLAGS_UNALIGNED) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_unaligned.s
	$(CC) $(CFLAGS_ALIGNED) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_aligned.s
	@echo -e "$(GREEN)Assembly files generated in $(BIN_DIR)/ (conv2d_*.s)$(NC)"

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
