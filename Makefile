# Project Directories
SCRIPTS_DIR  := scripts
ANALYSIS_DIR := $(SCRIPTS_DIR)/analysis
DOCS_DIR     := docs

# Python virtual environment for the analysis scripts (matplotlib)
VENV   := .venv
PYTHON := $(VENV)/bin/python

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

.PHONY: install/analysis-tools
install/analysis-tools: ## Create .venv and install the analysis libraries from requirements.txt
	python3 -m venv $(VENV)
	$(PYTHON) -m pip install -r requirements.txt

.PHONY: uninstall/analysis-tools
uninstall/analysis-tools: ## Remove the .venv of the analysis libraries
	rm -rf $(VENV)

##@ Analysis

.PHONY: check/analysis-tools
check/analysis-tools:
	@if [ ! -x "$(PYTHON)" ]; then \
		echo -e "$(RED)Error: $(PYTHON) not found. Run 'make install/analysis-tools' first.$(NC)"; \
		exit 1; \
	fi

.PHONY: analysis
analysis: analysis/ap1 analysis/ap2 ## Compile the data and make the tables and graphs of AP1 and AP2

.PHONY: analysis/data
analysis/data: check/analysis-tools ## Merge all benchmark CSVs into docs/data/compiled/
	$(PYTHON) $(SCRIPTS_DIR)/compile_data.py

.PHONY: analysis/ap1
analysis/ap1: analysis/data ## AP1 tables (docs/report/ap1/resume.md) and graphs (docs/report/ap1/figures/)
	$(PYTHON) $(ANALYSIS_DIR)/compare.py
	$(PYTHON) $(ANALYSIS_DIR)/plot_results.py

.PHONY: analysis/ap2
analysis/ap2: analysis/data ## AP2 tables (docs/report/ap2/resume.md) and graphs (docs/report/ap2/figures/)
	$(PYTHON) $(ANALYSIS_DIR)/ap2_compare.py
	$(PYTHON) $(ANALYSIS_DIR)/ap2_plots.py

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

##@ Library (OpenCV)

.PHONY: library-%
library-%: ## General make for library module (e.g. make library-build, make library-test)
	@make -C library $*

##@ Parallel (OpenMP)

.PHONY: parallel-%
parallel-%: ## General make for parallel module (e.g. make parallel-build, make parallel-test)
	@make -C parallel $*

##@ Build

.PHONY: build
build: baseline-build vectorize-build blocking-build library-build parallel-build ## Compile vectorize 2D convolution program

##@ Run

.PHONY: run
run: baseline-run vectorize-run blocking-run library-run parallel-run ## Run vectorize benchmark binaries
	./scripts/compile_data.py

##@ Test

.PHONY: test
test: baseline-test vectorize-test blocking-test library-test parallel-test ## Run 2D convolution unit tests

##@ Formatting

.PHONY: format
format: baseline-format vectorize-format blocking-format library-format parallel-format ## Format all .c/.h files in every module with clang-format

.PHONY: format-check
format-check: baseline-format-check vectorize-format-check blocking-format-check library-format-check parallel-format-check ## Check formatting of all modules without modifying files

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
clean: clean/docs/hardware clean/docs/tools baseline-clean vectorize-clean blocking-clean library-clean parallel-clean ## Clean all generated artifacts
