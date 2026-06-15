CPU_BOOK_DIR := books/cpu-volume-1
CPU_LATEX_DIR := $(CPU_BOOK_DIR)/source/latex
ALGO_BOOK_DIR := books/algorithm-interview

.PHONY: all cpu-check cpu-pdf cpu-epub cpu-text-count cpu-text-target cpu-lab00 cpu-coverage algo-check algo-pdf algo-epub algo-test clean

all: cpu-pdf cpu-epub

cpu-check:
	$(MAKE) -C $(CPU_LATEX_DIR) check

cpu-pdf:
	$(MAKE) -C $(CPU_LATEX_DIR) pdf

cpu-epub:
	$(MAKE) -C $(CPU_LATEX_DIR) epub

cpu-text-count:
	$(MAKE) -C $(CPU_LATEX_DIR) text-count

cpu-text-target:
	$(MAKE) -C $(CPU_LATEX_DIR) text-target

cpu-lab00:
	bash $(CPU_BOOK_DIR)/labs/lab00_benchmark_foundation/scripts/run_lab00.sh

cpu-coverage:
	bash $(CPU_BOOK_DIR)/tools/run_coverage.sh

algo-check:
	$(MAKE) -C $(ALGO_BOOK_DIR) check

algo-pdf:
	$(MAKE) -C $(ALGO_BOOK_DIR) pdf

algo-epub:
	$(MAKE) -C $(ALGO_BOOK_DIR) epub

algo-test:
	$(MAKE) -C $(ALGO_BOOK_DIR) test

clean:
	$(MAKE) -C $(CPU_LATEX_DIR) clean
	$(MAKE) -C $(ALGO_BOOK_DIR) clean
