LATEX_DIR := source/latex
PHONE_EXPORT_ROOT ?= /mnt/sdcard/STU/BOOKS
PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/操作系统第一册
FORMAL_BOOK_DIR := ../operating-systems-volume-1

.PHONY: pdf phone-export clean

pdf:
	cd $(LATEX_DIR) && latexmk -xelatex -interaction=nonstopmode -halt-on-error main.tex

phone-export: pdf
	$(MAKE) -C $(FORMAL_BOOK_DIR) pdf
	python3 scripts/export_phone_pair.py \
		--formal $(FORMAL_BOOK_DIR)/source/latex/main.pdf \
		--sample $(LATEX_DIR)/main.pdf \
		--dest "$(PHONE_EXPORT_DIR)"

clean:
	cd $(LATEX_DIR) && latexmk -C
