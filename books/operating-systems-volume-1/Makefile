BUILD_DIR ?= build
LATEX_SOURCE_DIR := source/latex
PHONE_EXPORT_ROOT ?= /mnt/sdcard/STU/BOOKS
PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/操作系统第一册
SAMPLE_BOOK_DIR := ../operating-systems-evolution-sample
PROJECT_SYNC_CHECK := ../../.codex/skills/operating-systems-book-architecture/scripts/audit_project_sync.py
CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Werror
LISTING_SOURCES := \
	$(LATEX_SOURCE_DIR)/listings/stage_one_hardware_model.cpp \
	$(LATEX_SOURCE_DIR)/listings/x86_64_stage_one_model.cpp \
	$(LATEX_SOURCE_DIR)/listings/x86_64_page_fault_cow_model.cpp \
	$(LATEX_SOURCE_DIR)/listings/os_reference_implementation.cpp \
	$(LATEX_SOURCE_DIR)/listings/minifs_lab.cpp

.PHONY: check project-audit project-sync test pdf phone-export clean

check: project-audit test
	python3 $(LATEX_SOURCE_DIR)/scripts/check_inputs.py
	@command -v xelatex >/dev/null 2>&1 || { echo "missing xelatex"; exit 1; }
	@command -v latexmk >/dev/null 2>&1 || { echo "missing latexmk"; exit 1; }

project-audit:
	python3 $(PROJECT_SYNC_CHECK) --book-only

project-sync:
	python3 $(PROJECT_SYNC_CHECK)

test:
	@set -e; \
	for source in $(LISTING_SOURCES); do \
		name=$$(basename "$$source" .cpp); \
		output="/tmp/os-book-$$name"; \
		echo "CXX $$source"; \
		$(CXX) $(CXXFLAGS) "$$source" -o "$$output"; \
		"$$output"; \
	done

pdf: check
	cd $(LATEX_SOURCE_DIR) && latexmk -xelatex -interaction=nonstopmode -halt-on-error main.tex

phone-export: pdf
	python3 $(PROJECT_SYNC_CHECK)
	$(MAKE) -C $(SAMPLE_BOOK_DIR) pdf
	python3 $(SAMPLE_BOOK_DIR)/scripts/export_phone_pair.py \
		--formal $(LATEX_SOURCE_DIR)/main.pdf \
		--sample $(SAMPLE_BOOK_DIR)/source/latex/main.pdf \
		--dest "$(PHONE_EXPORT_DIR)"

clean:
	rm -rf $(BUILD_DIR)
	cd $(LATEX_SOURCE_DIR) && latexmk -C
