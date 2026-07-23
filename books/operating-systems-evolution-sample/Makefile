LATEX_DIR := source/latex
PHONE_EXPORT_ROOT ?= /mnt/sdcard/STU/BOOKS
PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/操作系统第一册

.PHONY: pdf phone-export clean

pdf:
	cd $(LATEX_DIR) && latexmk -xelatex -interaction=nonstopmode -halt-on-error main.tex

phone-export: pdf
	python3 ../../tools/export_book.py --source $(LATEX_DIR) --dest "$(PHONE_EXPORT_DIR)" --pdf-name "操作系统演进样本"

clean:
	cd $(LATEX_DIR) && latexmk -C
