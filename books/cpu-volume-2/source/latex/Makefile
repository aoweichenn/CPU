LATEXMK ?= latexmk
ENGINE ?= -xelatex
MAIN ?= main.tex
EPUB_BUILDER ?= ../../../cpu-volume-1/source/latex/scripts/build_epub.py
TEXT_TARGET ?= 600000

.PHONY: all pdf epub clean check text-count text-count-chapters text-target

all: pdf epub

pdf:
	$(LATEXMK) $(ENGINE) -interaction=nonstopmode -halt-on-error $(MAIN)

epub:
	@BOOK_TITLE="从 C++ 到计算系统：第二册" \
	BOOK_SUBTITLE="硬件原理、多核并行、高性能计算、同步、无锁结构与分布式计算" \
	BOOK_AUTHOR="CPU Performance Study" \
	python3 $(EPUB_BUILDER) --book-dir . --output main.epub --no-cover-page --forbid-images --linearize-tables

clean:
	$(LATEXMK) -C

check:
	@python3 scripts/check_inputs.py
	@command -v xelatex >/dev/null 2>&1 || { echo "missing xelatex"; exit 1; }
	@command -v latexmk >/dev/null 2>&1 || { echo "missing latexmk"; exit 1; }

text-count:
	@python3 scripts/count_text_units.py

text-count-chapters:
	@python3 scripts/count_text_units.py --chapters-only

text-target:
	@python3 scripts/count_text_units.py --chapters-only --min $(TEXT_TARGET)
