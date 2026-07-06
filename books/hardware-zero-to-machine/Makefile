LATEX_SOURCE_DIR := source/latex
PHONE_EXPORT_ROOT ?= /mnt/sdcard/STU/BOOKS
PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/硬件从零到整机

.PHONY: check pdf phone-export clean

check:
	python3 $(LATEX_SOURCE_DIR)/scripts/check_inputs.py
	@command -v xelatex >/dev/null 2>&1 || { echo "missing xelatex"; exit 1; }
	@command -v latexmk >/dev/null 2>&1 || { echo "missing latexmk"; exit 1; }

pdf: check
	cd $(LATEX_SOURCE_DIR) && latexmk -xelatex -interaction=nonstopmode -halt-on-error main.tex

phone-export: pdf
	python3 ../../tools/export_book.py --source $(LATEX_SOURCE_DIR) --dest "$(PHONE_EXPORT_DIR)" --pdf-name "硬件从零到整机"

clean:
	cd $(LATEX_SOURCE_DIR) && latexmk -C
