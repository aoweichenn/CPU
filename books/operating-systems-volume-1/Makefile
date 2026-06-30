BUILD_DIR ?= build
LATEX_SOURCE_DIR := source/latex
PHONE_EXPORT_ROOT ?= /mnt/sdcard/STU/BOOKS
PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/操作系统第一册
EPUB_BUILDER ?= $(LATEX_SOURCE_DIR)/scripts/build_epub.py

.PHONY: check pdf epub phone-export clean

check:
	python3 $(LATEX_SOURCE_DIR)/scripts/check_inputs.py
	@command -v xelatex >/dev/null 2>&1 || { echo "missing xelatex"; exit 1; }
	@command -v latexmk >/dev/null 2>&1 || { echo "missing latexmk"; exit 1; }

pdf: check
	cd $(LATEX_SOURCE_DIR) && latexmk -xelatex -interaction=nonstopmode -halt-on-error main.tex

epub: check
	@BOOK_TITLE="操作系统第一册" \
	BOOK_SUBTITLE="硬件演进、内核对象、并发控制、内存、I/O、网络入口与恢复" \
	BOOK_AUTHOR="CPU Books" \
	python3 $(EPUB_BUILDER) --book-dir $(LATEX_SOURCE_DIR) --output $(LATEX_SOURCE_DIR)/main.epub

phone-export: pdf epub
	python3 ../../tools/export_book.py --source $(LATEX_SOURCE_DIR) --dest "$(PHONE_EXPORT_DIR)" --pdf-name "操作系统第一册" --epub-name "操作系统第一册"

clean:
	rm -rf $(BUILD_DIR)
	cd $(LATEX_SOURCE_DIR) && latexmk -C
	rm -f $(LATEX_SOURCE_DIR)/main.epub
