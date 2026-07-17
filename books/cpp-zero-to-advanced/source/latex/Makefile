LATEXMK ?= latexmk
ENGINE ?= -xelatex
MAIN ?= main.tex
TEXT_TARGET ?= 120000

.PHONY: all pdf text-count text-target clean check

all: pdf

pdf:
	$(LATEXMK) $(ENGINE) -interaction=nonstopmode -halt-on-error $(MAIN)

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
