LATEXMK ?= latexmk
ENGINE ?= -xelatex
MAIN ?= main.tex

.PHONY: all pdf clean check text-count text-count-chapters

all: pdf

pdf:
	$(LATEXMK) $(ENGINE) -interaction=nonstopmode -halt-on-error $(MAIN)

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
	@python3 scripts/count_text_units.py --min 360000
