# Project Directories
SCRIPTS_DIR := scripts
DOCS_DIR    := docs

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
	@echo -e "$(BLUE)conv2D Project Makefile$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(GREEN)<target>$(NC)\n"} /^[a-zA-Z_0-9\/-]+:.*?##/ { printf "  $(GREEN)%-25s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)
	@echo ""

##@ Setup

.PHONY: install/tools
install/tools: ## Install performance and energy profiling tools using DNF package manager
	@bash $(SCRIPTS_DIR)/install_tools.sh

.PHONY: uninstall/tools
uninstall/tools: ## Uninstall performance and energy profiling tools using DNF package manager
	@bash $(SCRIPTS_DIR)/uninstall_tools.sh

##@ Baseline

.PHONY: baseline-%
baseline-%: ## General make for baseline
	@make -C baseline $*

##@ Vectorize (SIMD)

.PHONY: vectorize-%
vectorize-%: ## General make for vectorize module (e.g. make vectorize-build, make vectorize-test)
	@make -C vectorize $*

##@ Blocking (Tiling)

.PHONY: blocking-%
blocking-%: ## General make for blocking module (e.g. make blocking-build, make blocking-test)
	@make -C blocking $*

##@ Build

.PHONY: build
build: baseline-build vectorize-build blocking-build ## Compile vectorize 2D convolution program

##@ Run

.PHONY: run
run: baseline-run vectorize-run blocking-run ## Run vectorize benchmark binaries

##@ Test

.PHONY: test
test: baseline-test vectorize-test blocking-test ## Run 2D convolution unit tests

##@ Formatting

.PHONY: format
format: baseline-format vectorize-format blocking-format ## Format all .c/.h files in every module with clang-format

.PHONY: format-check
format-check: baseline-format-check vectorize-format-check blocking-format-check ## Check formatting of all modules without modifying files

##@ Documentation

.PHONY: docs
docs: docs/hardware docs/tools ## Generate both hardware and tools documentation

.PHONY: docs/hardware
docs/hardware: ## Run scripts/hardware.sh to generate hardware documentation
	@bash $(SCRIPTS_DIR)/hardware.sh

.PHONY: docs/tools
docs/tools: ## Run scripts/tools.sh to generate tools documentation
	@bash $(SCRIPTS_DIR)/tools.sh

##@ Clean up

.PHONY: clean/docs/hardware
clean/docs/hardware: ## Remove generated hardware documentation directory
	rm -rf $(DOCS_DIR)/hardware

.PHONY: clean/docs/tools
clean/docs/tools: ## Remove generated tools documentation directory
	rm -rf $(DOCS_DIR)/tools

.PHONY: clean
clean: clean/docs/hardware clean/docs/tools baseline-clean vectorize-clean blocking-clean ## Clean all generated artifacts
