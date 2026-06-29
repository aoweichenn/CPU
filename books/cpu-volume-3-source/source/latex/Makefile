LATEXMK ?= latexmk
ENGINE ?= -xelatex
MAIN ?= main.tex
EPUB_BUILDER ?= ../../../cpu-volume-1/source/latex/scripts/build_epub.py

.PHONY: all pdf epub clean check

all: pdf epub

pdf:
	$(LATEXMK) $(ENGINE) -interaction=nonstopmode -halt-on-error $(MAIN)

epub:
	@BOOK_TITLE="从 C++ 到 AI 计算：第三册源码卷" \
	BOOK_IMPORT_TITLE="CPU Volume 3 Source" \
	BOOK_ID_SEED="cpu-volume-3-source-weread-20260629" \
	EPUB_ASCII_TITLES=1 \
	BOOK_SUBTITLE="LCQI 本地 CPU 推理引擎完整源码与阅读路径" \
	BOOK_AUTHOR="CPU Performance Study" \
	python3 $(EPUB_BUILDER) --book-dir . --output main.epub --no-cover-page --forbid-images --linearize-tables --wechat-compatible --no-embed-fonts

clean:
	$(LATEXMK) -C

check:
	@python3 scripts/check_inputs.py
	@command -v xelatex >/dev/null 2>&1 || { echo "missing xelatex"; exit 1; }
	@command -v latexmk >/dev/null 2>&1 || { echo "missing latexmk"; exit 1; }
