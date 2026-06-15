LATEX_DIR := source/latex

.PHONY: all check pdf epub text-count text-target lab00 coverage clean

all: pdf epub

check:
	$(MAKE) -C $(LATEX_DIR) check

pdf:
	$(MAKE) -C $(LATEX_DIR) pdf

epub:
	$(MAKE) -C $(LATEX_DIR) epub

text-count:
	$(MAKE) -C $(LATEX_DIR) text-count

text-target:
	$(MAKE) -C $(LATEX_DIR) text-target

lab00:
	bash labs/lab00_benchmark_foundation/scripts/run_lab00.sh

coverage:
	bash tools/run_coverage.sh

clean:
	$(MAKE) -C $(LATEX_DIR) clean
