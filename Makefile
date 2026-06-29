CPU_BOOK_DIR := books/cpu-volume-1
CPU_LATEX_DIR := $(CPU_BOOK_DIR)/source/latex
CPU1P_BOOK_DIR := books/cpu-volume-1-practice
CPU1P_LATEX_DIR := $(CPU1P_BOOK_DIR)/source/latex
CPU2_BOOK_DIR := books/cpu-volume-2
CPU2_LATEX_DIR := $(CPU2_BOOK_DIR)/source/latex
CPU3_BOOK_DIR := books/cpu-volume-3
CPU3_LATEX_DIR := $(CPU3_BOOK_DIR)/source/latex
CPU3P_BOOK_DIR := books/cpu-volume-3-practice
CPU3P_LATEX_DIR := $(CPU3P_BOOK_DIR)/source/latex
ALGO_BOOK_DIR := books/algorithm-interview
ALGO_LATEX_DIR := $(ALGO_BOOK_DIR)/source/latex
CPP_BOOK_DIR := books/cpp-zero-to-advanced
CPP_LATEX_DIR := $(CPP_BOOK_DIR)/source/latex
CSE_BOOK_DIR := books/compute-systems-engine-code
CSE_LATEX_DIR := $(CSE_BOOK_DIR)/source/latex
EXPORT_ROOT := book-exports
CPU_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到计算系统第一册
CPU1P_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到计算系统第一册实践卷
CPU2_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到计算系统第二册
CPU3_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到AI计算第三册
CPU3P_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到AI计算第三册实践卷
ALGO_EXPORT_DIR := $(EXPORT_ROOT)/算法刷题与Cpp面试教材
CPP_EXPORT_DIR := $(EXPORT_ROOT)/Cpp从零到高级
CSE_EXPORT_DIR := $(EXPORT_ROOT)/计算系统引擎代码实践卷
CPU_EXPORT_NAME := 从Cpp到计算系统第一册
CPU1P_EXPORT_NAME := 从Cpp到计算系统第一册实践卷
CPU2_EXPORT_NAME := 从Cpp到计算系统第二册
CPU3_EXPORT_NAME := 从Cpp到AI计算第三册
CPU3P_EXPORT_NAME := 从Cpp到AI计算第三册实践卷
ALGO_EXPORT_NAME := 算法刷题与Cpp面试教材
CPP_EXPORT_NAME := Cpp从零到高级
CSE_EXPORT_NAME := 计算系统引擎代码实践卷
CPU_EPUB_NAME := $(CPU_EXPORT_NAME)
CPU1P_EPUB_NAME := $(CPU1P_EXPORT_NAME)
CPU2_EPUB_NAME := $(CPU2_EXPORT_NAME)
CPU3_EPUB_NAME := $(CPU3_EXPORT_NAME)
CPU3P_EPUB_NAME := $(CPU3P_EXPORT_NAME)
ALGO_EPUB_NAME := $(ALGO_EXPORT_NAME)
CPP_EPUB_NAME := $(CPP_EXPORT_NAME)
CSE_EPUB_NAME := $(CSE_EXPORT_NAME)

.PHONY: all cpu-check cpu-pdf cpu-epub cpu-export cpu-text-count cpu-text-target cpu-lab00 cpu-coverage cpu1p-check cpu1p-pdf cpu1p-epub cpu1p-export cpu1p-test cpu2-check cpu2-pdf cpu2-epub cpu2-export cpu2-text-count cpu2-text-count-chapters cpu2-text-target cpu3-check cpu3-pdf cpu3-epub cpu3-export cpu3-smollm2-smoke cpu3-text-count cpu3-text-count-chapters cpu3-text-target cpu3p-check cpu3p-pdf cpu3p-epub cpu3p-export cpu3p-test books-export algo-check algo-pdf algo-epub algo-export algo-text-count algo-text-target algo-test cpp-check cpp-pdf cpp-epub cpp-export cpp-text-count cpp-text-target cse-check cse-pdf cse-epub cse-export clean

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

cpu1p-check:
	$(MAKE) -C $(CPU1P_LATEX_DIR) check

cpu1p-pdf:
	$(MAKE) -C $(CPU1P_LATEX_DIR) pdf

cpu1p-epub:
	$(MAKE) -C $(CPU1P_LATEX_DIR) epub

cpu1p-export: cpu1p-pdf cpu1p-epub
	python3 tools/export_book.py --source $(CPU1P_LATEX_DIR) --dest "$(CPU1P_EXPORT_DIR)" --pdf-name "$(CPU1P_EXPORT_NAME)" --epub-name "$(CPU1P_EPUB_NAME)"

cpu1p-test:
	$(MAKE) -C $(CPU1P_BOOK_DIR) test

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

cpu3-smollm2-smoke:
	python3 $(CPU3_BOOK_DIR)/tools/run_smollm2_small_smoke.py

cpu3p-check:
	$(MAKE) -C $(CPU3P_LATEX_DIR) check

cpu3p-pdf:
	$(MAKE) -C $(CPU3P_LATEX_DIR) pdf

cpu3p-epub:
	$(MAKE) -C $(CPU3P_LATEX_DIR) epub

cpu3p-export: cpu3p-pdf cpu3p-epub
	python3 tools/export_book.py --source $(CPU3P_LATEX_DIR) --dest "$(CPU3P_EXPORT_DIR)" --pdf-name "$(CPU3P_EXPORT_NAME)" --epub-name "$(CPU3P_EPUB_NAME)"

cpu3p-test:
	$(MAKE) -C $(CPU3P_BOOK_DIR) test

cpu3-text-count:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-count

cpu3-text-count-chapters:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-count-chapters

cpu3-text-target:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-target

books-export: cpu-export cpu1p-export cpu2-export cpu3-export cpu3p-export algo-export cpp-export cse-export

algo-check:
	$(MAKE) -C $(ALGO_BOOK_DIR) check

algo-pdf:
	$(MAKE) -C $(ALGO_BOOK_DIR) pdf

algo-epub:
	$(MAKE) -C $(ALGO_BOOK_DIR) epub

algo-export: algo-pdf algo-epub
	python3 tools/export_book.py --source $(ALGO_LATEX_DIR) --dest "$(ALGO_EXPORT_DIR)" --pdf-name "$(ALGO_EXPORT_NAME)" --epub-name "$(ALGO_EPUB_NAME)"

algo-text-count:
	$(MAKE) -C $(ALGO_BOOK_DIR) text-count

algo-text-target:
	$(MAKE) -C $(ALGO_BOOK_DIR) text-target

algo-test:
	$(MAKE) -C $(ALGO_BOOK_DIR) test

cpp-check:
	$(MAKE) -C $(CPP_BOOK_DIR) check

cpp-pdf:
	$(MAKE) -C $(CPP_BOOK_DIR) pdf

cpp-epub:
	$(MAKE) -C $(CPP_BOOK_DIR) epub

cpp-export: cpp-pdf cpp-epub
	python3 tools/export_book.py --source $(CPP_LATEX_DIR) --dest "$(CPP_EXPORT_DIR)" --pdf-name "$(CPP_EXPORT_NAME)" --epub-name "$(CPP_EPUB_NAME)"

cpp-text-count:
	$(MAKE) -C $(CPP_BOOK_DIR) text-count

cpp-text-target:
	$(MAKE) -C $(CPP_BOOK_DIR) text-target

cse-check:
	$(MAKE) -C $(CSE_LATEX_DIR) check

cse-pdf:
	$(MAKE) -C $(CSE_LATEX_DIR) pdf

cse-epub:
	$(MAKE) -C $(CSE_LATEX_DIR) epub

cse-export: cse-pdf cse-epub
	python3 tools/export_book.py --source $(CSE_LATEX_DIR) --dest "$(CSE_EXPORT_DIR)" --pdf-name "$(CSE_EXPORT_NAME)" --epub-name "$(CSE_EPUB_NAME)"

clean:
	$(MAKE) -C $(CPU_LATEX_DIR) clean
	$(MAKE) -C $(CPU1P_LATEX_DIR) clean
	$(MAKE) -C $(CPU2_LATEX_DIR) clean
	$(MAKE) -C $(CPU3_LATEX_DIR) clean
	$(MAKE) -C $(CPU3P_LATEX_DIR) clean
	$(MAKE) -C $(ALGO_BOOK_DIR) clean
	$(MAKE) -C $(CPP_BOOK_DIR) clean
	$(MAKE) -C $(CSE_LATEX_DIR) clean
