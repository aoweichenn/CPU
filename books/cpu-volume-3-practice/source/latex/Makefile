LATEXMK ?= latexmk
ENGINE ?= -xelatex
MAIN ?= main.tex
EPUB_BUILDER ?= ../../../cpu-volume-1/source/latex/scripts/build_epub.py

.PHONY: all pdf epub clean check

all: pdf epub

pdf:
	$(LATEXMK) $(ENGINE) -interaction=nonstopmode -halt-on-error $(MAIN)

epub:
	@BOOK_TITLE="从 C++ 到 AI 计算：第三册实践与代码卷" \
	BOOK_IMPORT_TITLE="CPU Volume 3 Practice Code" \
	BOOK_ID_SEED="cpu-volume-3-practice-code-weread-20260629" \
	EPUB_ASCII_TITLES=1 \
	BOOK_SUBTITLE="本地量化推理引擎、完整源码、KV Cache、后端优化与验收" \
	BOOK_AUTHOR="CPU Performance Study" \
	python3 $(EPUB_BUILDER) --book-dir . --output main.epub --no-cover-page --forbid-images --linearize-tables --wechat-compatible --no-embed-fonts

clean:
	$(LATEXMK) -C

check:
	@python3 scripts/check_inputs.py
	@command -v xelatex >/dev/null 2>&1 || { echo "missing xelatex"; exit 1; }
	@command -v latexmk >/dev/null 2>&1 || { echo "missing latexmk"; exit 1; }
