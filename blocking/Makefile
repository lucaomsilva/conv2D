# Compiler and Flags Configuration
CC              ?= gcc
BASE_CFLAGS     ?= -Wall
CFLAGS_SEQ      := -O3 -fno-tree-vectorize $(BASE_CFLAGS)
CFLAGS_SIMD     := -O3 -mavx2 -mfma -fopenmp-simd -fopt-info-vec-optimized -DMODE_SIMD $(BASE_CFLAGS)
CFLAGS_TEST_SEQ := -O3 -fno-tree-vectorize $(BASE_CFLAGS)
CFLAGS_TEST_SIMD:= -O3 -mavx2 -mfma -fopenmp-simd -fopt-info-vec-optimized -DMODE_SIMD $(BASE_CFLAGS)

# Blocked data layout (matrix of blocks with halo) and experiments
CFLAGS_LAYOUT_SEQ       := $(CFLAGS_SEQ) -DLAYOUT_BLOCKED
CFLAGS_LAYOUT_SIMD      := $(CFLAGS_SIMD) -DLAYOUT_BLOCKED
CFLAGS_LAYOUT_NT        := $(CFLAGS_LAYOUT_SIMD) -DSTORE_NT
CFLAGS_LAYOUT_NT_PRAGMA := $(CFLAGS_LAYOUT_SIMD) -DSTORE_NT_PRAGMA
HP                      := -DHUGEPAGES

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
	@echo -e "$(BLUE)Blocking (Tiling) 2D Convolution Makefile$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(GREEN)<target>$(NC)\n"} /^[a-zA-Z_0-9\/-]+:.*?##/ { printf "  $(GREEN)%-25s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
	@echo ""

##@ Build

.PHONY: build
build: build-seq build-simd build-layout ## Compile all blocking executables (tiling and blocked layout)

.PHONY: build-seq
build-seq: ## Compile isolated memory tiling executable (no SIMD, -fno-tree-vectorize)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_SEQ) $(SRC) -o $(BIN_DIR)/blocking_seq

.PHONY: build-simd
build-simd: ## Compile combined memory tiling + SIMD executable (-mavx2 -fopenmp-simd -DMODE_SIMD)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_SIMD) $(SRC) -o $(BIN_DIR)/blocking_simd

.PHONY: build-layout
build-layout: ## Compile the 6 blocked-layout executables (seq/simd/simd_nt, each with and without huge pages)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_LAYOUT_SEQ) $(SRC) -o $(BIN_DIR)/blocking_layout_seq
	$(CC) $(CFLAGS_LAYOUT_SEQ) $(HP) $(SRC) -o $(BIN_DIR)/blocking_layout_seq_hp
	$(CC) $(CFLAGS_LAYOUT_SIMD) $(SRC) -o $(BIN_DIR)/blocking_layout_simd
	$(CC) $(CFLAGS_LAYOUT_SIMD) $(HP) $(SRC) -o $(BIN_DIR)/blocking_layout_simd_hp
	$(CC) $(CFLAGS_LAYOUT_NT) $(SRC) -o $(BIN_DIR)/blocking_layout_simd_nt
	$(CC) $(CFLAGS_LAYOUT_NT) $(HP) $(SRC) -o $(BIN_DIR)/blocking_layout_simd_nt_hp

##@ Execution & Profiling

.PHONY: run
run: run-seq run-simd run-layout ## Run benchmarks for all blocking variants

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

.PHONY: run-layout
run-layout: run-layout-seq run-layout-simd ## Benchmark all 6 blocked-layout binaries

.PHONY: run-layout-seq
run-layout-seq: ## Benchmark blocking_layout_seq and _hp via scripts/blocking_layout_seq.sh
	bash $(SCRIPTS_DIR)/blocking_layout_seq.sh

.PHONY: run-layout-simd
run-layout-simd: ## Benchmark blocking_layout_simd, _hp, _nt, _nt_hp via scripts/blocking_layout_simd.sh
	bash $(SCRIPTS_DIR)/blocking_layout_simd.sh

##@ Testing & Verification

.PHONY: test
test: test-seq test-simd test-layout ## Compile and run unit tests for all blocking variants across all block sizes

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

.PHONY: test-layout
test-layout: ## Run unit tests for the blocked layout (seq, simd, simd_nt); huge pages only change main.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_LAYOUT_SEQ) $(SRC_TEST) -o $(BIN_DIR)/test_layout_seq -lm
	$(CC) $(CFLAGS_LAYOUT_SIMD) $(SRC_TEST) -o $(BIN_DIR)/test_layout_simd -lm
	$(CC) $(CFLAGS_LAYOUT_NT) $(SRC_TEST) -o $(BIN_DIR)/test_layout_simd_nt -lm
	@echo -e "$(YELLOW)Testing: Blocked Layout (seq, simd, simd_nt)...$(NC)"
	@./$(BIN_DIR)/test_layout_seq
	@./$(BIN_DIR)/test_layout_simd
	@./$(BIN_DIR)/test_layout_simd_nt

##@ Assembly Inspection

.PHONY: asm
asm: ## Generate assembly files (.s) in bin/ for compiler vectorization and loop inspection
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_SEQ) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_seq.s
	$(CC) $(CFLAGS_SIMD) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_simd.s
	$(CC) $(CFLAGS_LAYOUT_SEQ) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_layout_seq.s
	$(CC) $(CFLAGS_LAYOUT_SIMD) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_layout_simd.s
	$(CC) $(CFLAGS_LAYOUT_NT) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_layout_simd_nt.s
	$(CC) $(CFLAGS_LAYOUT_NT_PRAGMA) -S -fverbose-asm $(CONV_DIR)/conv2d.c -o $(BIN_DIR)/conv2d_layout_simd_nt_pragma.s
	@echo -e "$(GREEN)Assembly files generated in $(BIN_DIR)/ (conv2d_*.s)$(NC)"
	@echo "vmovntps count: intrinsics=$$(grep -c vmovntps $(BIN_DIR)/conv2d_layout_simd_nt.s) pragma=$$(grep -c vmovntps $(BIN_DIR)/conv2d_layout_simd_nt_pragma.s)"

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
