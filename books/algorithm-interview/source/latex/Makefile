LATEXMK ?= latexmk
ENGINE ?= -xelatex
MAIN ?= main.tex
EPUB_BUILDER ?= ../../../cpu-volume-1/source/latex/scripts/build_epub.py
TEXT_TARGET ?= 180000

.PHONY: all pdf epub text-count text-target clean check

all: pdf

pdf:
	$(LATEXMK) $(ENGINE) -interaction=nonstopmode -halt-on-error $(MAIN)

epub:
	@BOOK_TITLE="算法刷题与 C++ 面试教材" \
	BOOK_SUBTITLE="数据结构、算法设计、复杂度证明与工程化题解" \
	BOOK_AUTHOR="Algorithm Interview Study" \
	python3 $(EPUB_BUILDER) --book-dir . --output main.epub

text-count:
	@python3 scripts/count_text_units.py

text-target:
	@python3 scripts/count_text_units.py --min $(TEXT_TARGET)

clean:
	$(LATEXMK) -C

check:
	@python3 scripts/check_inputs.py
	@command -v xelatex >/dev/null 2>&1 || { echo "missing xelatex"; exit 1; }
	@command -v latexmk >/dev/null 2>&1 || { echo "missing latexmk"; exit 1; }
