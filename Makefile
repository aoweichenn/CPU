CPU_BOOK_DIR := books/cpu-volume-1
CPU_LATEX_DIR := $(CPU_BOOK_DIR)/source/latex
CPU2_BOOK_DIR := books/cpu-volume-2
CPU2_LATEX_DIR := $(CPU2_BOOK_DIR)/source/latex
CPU3_BOOK_DIR := books/cpu-volume-3
CPU3_LATEX_DIR := $(CPU3_BOOK_DIR)/source/latex
ALGO_BOOK_DIR := books/algorithm-interview
EXPORT_ROOT := book-exports
CPU_EXPORT_DIR := $(EXPORT_ROOT)/从C++到计算系统第一册
CPU2_EXPORT_DIR := $(EXPORT_ROOT)/从C++到计算系统第二册
CPU3_EXPORT_DIR := $(EXPORT_ROOT)/从C++到AI计算第三册
CPU_EXPORT_NAME := 从C++到计算系统第一册
CPU2_EXPORT_NAME := 从C++到计算系统第二册
CPU3_EXPORT_NAME := 从C++到AI计算第三册
CPU_EPUB_NAME := $(CPU_EXPORT_NAME)
CPU2_EPUB_NAME := $(CPU2_EXPORT_NAME)
CPU3_EPUB_NAME := $(CPU3_EXPORT_NAME)

.PHONY: all cpu-check cpu-pdf cpu-epub cpu-export cpu-text-count cpu-text-target cpu-lab00 cpu-coverage cpu2-check cpu2-pdf cpu2-epub cpu2-export cpu2-text-count cpu2-text-count-chapters cpu2-text-target cpu3-check cpu3-pdf cpu3-epub cpu3-export cpu3-text-count cpu3-text-count-chapters cpu3-text-target books-export algo-check algo-pdf algo-epub algo-text-count algo-text-target algo-test clean

all: cpu-pdf cpu-epub

cpu-check:
	$(MAKE) -C $(CPU_LATEX_DIR) check

cpu-pdf:
	$(MAKE) -C $(CPU_LATEX_DIR) pdf

cpu-epub:
	$(MAKE) -C $(CPU_LATEX_DIR) epub

cpu-export: cpu-pdf cpu-epub
	python3 tools/export_book.py --source $(CPU_LATEX_DIR) --dest "$(CPU_EXPORT_DIR)" --pdf-name "$(CPU_EXPORT_NAME)" --epub-name "$(CPU_EPUB_NAME)"

cpu-text-count:
	$(MAKE) -C $(CPU_LATEX_DIR) text-count

cpu-text-target:
	$(MAKE) -C $(CPU_LATEX_DIR) text-target

cpu-lab00:
	bash $(CPU_BOOK_DIR)/labs/lab00_benchmark_foundation/scripts/run_lab00.sh

cpu-coverage:
	bash $(CPU_BOOK_DIR)/tools/run_coverage.sh

cpu2-check:
	$(MAKE) -C $(CPU2_LATEX_DIR) check

cpu2-pdf:
	$(MAKE) -C $(CPU2_LATEX_DIR) pdf

cpu2-epub:
	$(MAKE) -C $(CPU2_LATEX_DIR) epub

cpu2-export: cpu2-pdf cpu2-epub
	python3 tools/export_book.py --source $(CPU2_LATEX_DIR) --dest "$(CPU2_EXPORT_DIR)" --pdf-name "$(CPU2_EXPORT_NAME)" --epub-name "$(CPU2_EPUB_NAME)"

cpu2-text-count:
	$(MAKE) -C $(CPU2_LATEX_DIR) text-count

cpu2-text-count-chapters:
	$(MAKE) -C $(CPU2_LATEX_DIR) text-count-chapters

cpu2-text-target:
	$(MAKE) -C $(CPU2_LATEX_DIR) text-target

cpu3-check:
	$(MAKE) -C $(CPU3_LATEX_DIR) check

cpu3-pdf:
	$(MAKE) -C $(CPU3_LATEX_DIR) pdf

cpu3-epub:
	$(MAKE) -C $(CPU3_LATEX_DIR) epub

cpu3-export: cpu3-pdf cpu3-epub
	python3 tools/export_book.py --source $(CPU3_LATEX_DIR) --dest "$(CPU3_EXPORT_DIR)" --pdf-name "$(CPU3_EXPORT_NAME)" --epub-name "$(CPU3_EPUB_NAME)"

cpu3-text-count:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-count

cpu3-text-count-chapters:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-count-chapters

cpu3-text-target:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-target

books-export: cpu-export cpu2-export cpu3-export

algo-check:
	$(MAKE) -C $(ALGO_BOOK_DIR) check

algo-pdf:
	$(MAKE) -C $(ALGO_BOOK_DIR) pdf

algo-epub:
	$(MAKE) -C $(ALGO_BOOK_DIR) epub

algo-text-count:
	$(MAKE) -C $(ALGO_BOOK_DIR) text-count

algo-text-target:
	$(MAKE) -C $(ALGO_BOOK_DIR) text-target

algo-test:
	$(MAKE) -C $(ALGO_BOOK_DIR) test

clean:
	$(MAKE) -C $(CPU_LATEX_DIR) clean
	$(MAKE) -C $(CPU2_LATEX_DIR) clean
	$(MAKE) -C $(CPU3_LATEX_DIR) clean
	$(MAKE) -C $(ALGO_BOOK_DIR) clean
