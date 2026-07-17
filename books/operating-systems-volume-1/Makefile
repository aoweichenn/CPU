BUILD_DIR ?= build
LATEX_SOURCE_DIR := source/latex
PHONE_EXPORT_ROOT ?= /mnt/sdcard/STU/BOOKS
PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/操作系统第一册
CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Werror
LISTING_SOURCES := \
	$(LATEX_SOURCE_DIR)/listings/stage_one_hardware_model.cpp \
	$(LATEX_SOURCE_DIR)/listings/x86_64_stage_one_model.cpp \
	$(LATEX_SOURCE_DIR)/listings/x86_64_page_fault_cow_model.cpp \
	$(LATEX_SOURCE_DIR)/listings/os_reference_implementation.cpp

.PHONY: check test pdf phone-export clean

check: test
	python3 $(LATEX_SOURCE_DIR)/scripts/check_inputs.py
	@command -v xelatex >/dev/null 2>&1 || { echo "missing xelatex"; exit 1; }
	@command -v latexmk >/dev/null 2>&1 || { echo "missing latexmk"; exit 1; }

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
	python3 ../../tools/export_book.py --source $(LATEX_SOURCE_DIR) --dest "$(PHONE_EXPORT_DIR)" --pdf-name "操作系统第一册"

clean:
	rm -rf $(BUILD_DIR)
	cd $(LATEX_SOURCE_DIR) && latexmk -C
