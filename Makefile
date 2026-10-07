# Compiler and Flags Configuration
CC       ?= gcc
CXX      ?= g++
CFLAGS   ?= -O3 -Wall
CXXFLAGS ?= -O3 -Wall

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
clean: clean/docs/hardware clean/docs/tools ## Clean all generated artifacts
