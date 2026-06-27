LATEXMK ?= latexmk
ENGINE ?= -xelatex
MAIN ?= main.tex
EPUB_BUILDER ?= ../../../cpu-volume-1/source/latex/scripts/build_epub.py
TEXT_TARGET ?= 120000

.PHONY: all pdf epub text-count text-target clean check

all: pdf

pdf:
	$(LATEXMK) $(ENGINE) -interaction=nonstopmode -halt-on-error $(MAIN)

epub:
	@BOOK_TITLE="Cpp 从零到高级" \
	BOOK_SUBTITLE="语法、资源管理、泛型、并发、性能与工程实践" \
	BOOK_AUTHOR="Cpp Systems Study" \
	python3 $(EPUB_BUILDER) --book-dir . --output main.epub --no-cover-page --forbid-images --linearize-tables --wechat-compatible --no-embed-fonts

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
