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
CPU3S_BOOK_DIR := books/cpu-volume-3-source
CPU3S_LATEX_DIR := $(CPU3S_BOOK_DIR)/source/latex
ALGO_BOOK_DIR := books/algorithm-interview
ALGO_LATEX_DIR := $(ALGO_BOOK_DIR)/source/latex
CPP_BOOK_DIR := books/cpp-zero-to-advanced
CPP_LATEX_DIR := $(CPP_BOOK_DIR)/source/latex
CSE_BOOK_DIR := books/compute-systems-engine-code
CSE_LATEX_DIR := $(CSE_BOOK_DIR)/source/latex
EXPORT_ROOT := book-exports
PHONE_EXPORT_ROOT := /mnt/sdcard/STU/BOOKS
CPU_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到计算系统第一册
CPU1P_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到计算系统第一册实践卷
CPU2_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到计算系统第二册
CPU3_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到AI计算第三册
CPU3P_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到AI计算第三册实践与代码卷
CPU3S_EXPORT_DIR := $(EXPORT_ROOT)/从Cpp到AI计算第三册源码卷
ALGO_EXPORT_DIR := $(EXPORT_ROOT)/算法刷题与Cpp面试教材
CPP_EXPORT_DIR := $(EXPORT_ROOT)/Cpp从零到高级
CSE_EXPORT_DIR := $(EXPORT_ROOT)/计算系统引擎代码实践卷
CPU_PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/从Cpp到计算系统第一册
CPU1P_PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/实践与代码卷/从Cpp到计算系统第一册实践卷
CPU2_PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/从Cpp到计算系统第二册
CPU3_PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/从Cpp到AI计算第三册
CPU3P_PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/实践与代码卷/从Cpp到AI计算第三册实践与代码卷
ALGO_PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/算法刷题与Cpp面试教材
CPP_PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/原理卷/Cpp从零到高级
CSE_PHONE_EXPORT_DIR := $(PHONE_EXPORT_ROOT)/按卷类型/实践与代码卷/计算系统引擎代码实践卷
CPU_EXPORT_NAME := 从Cpp到计算系统第一册
CPU1P_EXPORT_NAME := 从Cpp到计算系统第一册实践卷
CPU2_EXPORT_NAME := 从Cpp到计算系统第二册
CPU3_EXPORT_NAME := 从Cpp到AI计算第三册
CPU3P_EXPORT_NAME := 从Cpp到AI计算第三册实践与代码卷
CPU3S_EXPORT_NAME := 从Cpp到AI计算第三册源码卷
ALGO_EXPORT_NAME := 算法刷题与Cpp面试教材
CPP_EXPORT_NAME := Cpp从零到高级
CSE_EXPORT_NAME := 计算系统引擎代码实践卷
CPU_EPUB_NAME := $(CPU_EXPORT_NAME)
CPU1P_EPUB_NAME := $(CPU1P_EXPORT_NAME)
CPU2_EPUB_NAME := $(CPU2_EXPORT_NAME)
CPU3_EPUB_NAME := $(CPU3_EXPORT_NAME)
CPU3P_EPUB_NAME := $(CPU3P_EXPORT_NAME)
CPU3S_EPUB_NAME := $(CPU3S_EXPORT_NAME)
ALGO_EPUB_NAME := $(ALGO_EXPORT_NAME)
CPP_EPUB_NAME := $(CPP_EXPORT_NAME)
CSE_EPUB_NAME := $(CSE_EXPORT_NAME)

.PHONY: all cpu-check cpu-pdf cpu-epub cpu-export cpu-phone-export cpu-text-count cpu-text-target cpu-lab00 cpu-coverage cpu1p-check cpu1p-pdf cpu1p-epub cpu1p-export cpu1p-phone-export cpu1p-test cpu2-check cpu2-pdf cpu2-epub cpu2-export cpu2-phone-export cpu2-text-count cpu2-text-count-chapters cpu2-text-target cpu3-check cpu3-pdf cpu3-epub cpu3-export cpu3-phone-export cpu3-smollm2-smoke cpu3-smollm2-q4-benchmark cpu3-gpt2-smoke cpu3-gpt2-benchmark-compare cpu3-gpt2-hotspot-profile cpu3-text-count cpu3-text-count-chapters cpu3-text-target cpu3p-check cpu3p-pdf cpu3p-epub cpu3p-export cpu3p-phone-export cpu3p-test cpu3s-check cpu3s-pdf cpu3s-epub cpu3s-export cpu3s-phone-export cpu3s-test books-export phone-books-export phone-export-organize phone-export-organize-allow-missing algo-check algo-pdf algo-epub algo-export algo-phone-export algo-text-count algo-text-target algo-test cpp-check cpp-pdf cpp-epub cpp-export cpp-phone-export cpp-text-count cpp-text-target cse-check cse-pdf cse-epub cse-export cse-phone-export clean

all: cpu-pdf cpu-epub

cpu-check:
	$(MAKE) -C $(CPU_LATEX_DIR) check

cpu-pdf:
	$(MAKE) -C $(CPU_LATEX_DIR) pdf

cpu-epub:
	$(MAKE) -C $(CPU_LATEX_DIR) epub

cpu-export: cpu-pdf cpu-epub
	python3 tools/export_book.py --source $(CPU_LATEX_DIR) --dest "$(CPU_EXPORT_DIR)" --pdf-name "$(CPU_EXPORT_NAME)" --epub-name "$(CPU_EPUB_NAME)"

cpu-phone-export: cpu-pdf cpu-epub
	python3 tools/export_book.py --source $(CPU_LATEX_DIR) --dest "$(CPU_PHONE_EXPORT_DIR)" --pdf-name "$(CPU_EXPORT_NAME)" --epub-name "$(CPU_EPUB_NAME)"
	$(MAKE) phone-export-organize-allow-missing

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

cpu1p-phone-export: cpu1p-pdf cpu1p-epub
	python3 tools/export_book.py --source $(CPU1P_LATEX_DIR) --dest "$(CPU1P_PHONE_EXPORT_DIR)" --pdf-name "$(CPU1P_EXPORT_NAME)" --epub-name "$(CPU1P_EPUB_NAME)"
	$(MAKE) phone-export-organize-allow-missing

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

cpu2-phone-export: cpu2-pdf cpu2-epub
	python3 tools/export_book.py --source $(CPU2_LATEX_DIR) --dest "$(CPU2_PHONE_EXPORT_DIR)" --pdf-name "$(CPU2_EXPORT_NAME)" --epub-name "$(CPU2_EPUB_NAME)"
	$(MAKE) phone-export-organize-allow-missing

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

cpu3-phone-export: cpu3-pdf cpu3-epub
	python3 tools/export_book.py --source $(CPU3_LATEX_DIR) --dest "$(CPU3_PHONE_EXPORT_DIR)" --pdf-name "$(CPU3_EXPORT_NAME)" --epub-name "$(CPU3_EPUB_NAME)"
	$(MAKE) phone-export-organize-allow-missing

cpu3-smollm2-smoke:
	python3 $(CPU3_BOOK_DIR)/tools/run_smollm2_small_smoke.py

cpu3-smollm2-q4-benchmark:
	python3 $(CPU3_BOOK_DIR)/tools/run_smollm2_q4_k_benchmark.py

cpu3-gpt2-smoke:
	python3 $(CPU3_BOOK_DIR)/tools/run_gpt2_smoke.py

cpu3-gpt2-benchmark-compare:
	python3 $(CPU3_BOOK_DIR)/tools/run_gpt2_benchmark_compare.py

cpu3-gpt2-hotspot-profile:
	python3 $(CPU3_BOOK_DIR)/tools/run_gpt2_hotspot_profile.py

cpu3p-check:
	$(MAKE) -C $(CPU3P_LATEX_DIR) check

cpu3p-pdf:
	$(MAKE) -C $(CPU3P_LATEX_DIR) pdf

cpu3p-epub:
	$(MAKE) -C $(CPU3P_LATEX_DIR) epub

cpu3p-export: cpu3p-pdf cpu3p-epub
	python3 tools/export_book.py --source $(CPU3P_LATEX_DIR) --dest "$(CPU3P_EXPORT_DIR)" --pdf-name "$(CPU3P_EXPORT_NAME)" --epub-name "$(CPU3P_EPUB_NAME)"

cpu3p-phone-export: cpu3p-pdf cpu3p-epub
	python3 tools/export_book.py --source $(CPU3P_LATEX_DIR) --dest "$(CPU3P_PHONE_EXPORT_DIR)" --pdf-name "$(CPU3P_EXPORT_NAME)" --epub-name "$(CPU3P_EPUB_NAME)"
	$(MAKE) phone-export-organize-allow-missing

cpu3p-test:
	$(MAKE) -C $(CPU3P_BOOK_DIR) test

cpu3s-check:
	$(MAKE) -C $(CPU3S_LATEX_DIR) check

cpu3s-pdf:
	$(MAKE) -C $(CPU3S_LATEX_DIR) pdf

cpu3s-epub:
	$(MAKE) -C $(CPU3S_LATEX_DIR) epub

cpu3s-export: cpu3s-pdf cpu3s-epub
	python3 tools/export_book.py --source $(CPU3S_LATEX_DIR) --dest "$(CPU3S_EXPORT_DIR)" --pdf-name "$(CPU3S_EXPORT_NAME)" --epub-name "$(CPU3S_EPUB_NAME)"

cpu3s-phone-export:
	@echo "cpu-volume-3-source is historical; phone export is merged into cpu3p-phone-export"

cpu3s-test:
	$(MAKE) -C $(CPU3S_BOOK_DIR) test

cpu3-text-count:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-count

cpu3-text-count-chapters:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-count-chapters

cpu3-text-target:
	$(MAKE) -C $(CPU3_LATEX_DIR) text-target

books-export: cpu-export cpu1p-export cpu2-export cpu3-export cpu3p-export algo-export cpp-export cse-export

phone-books-export:
	$(MAKE) cpu-phone-export
	$(MAKE) cpu1p-phone-export
	$(MAKE) cpu2-phone-export
	$(MAKE) cpu3-phone-export
	$(MAKE) cpu3p-phone-export
	$(MAKE) algo-phone-export
	$(MAKE) cpp-phone-export
	$(MAKE) cse-phone-export
	$(MAKE) phone-export-organize

phone-export-organize:
	python3 tools/organize_phone_exports.py --root "$(PHONE_EXPORT_ROOT)"

phone-export-organize-allow-missing:
	python3 tools/organize_phone_exports.py --root "$(PHONE_EXPORT_ROOT)" --allow-missing

algo-check:
	$(MAKE) -C $(ALGO_BOOK_DIR) check

algo-pdf:
	$(MAKE) -C $(ALGO_BOOK_DIR) pdf

algo-epub:
	$(MAKE) -C $(ALGO_BOOK_DIR) epub

algo-export: algo-pdf algo-epub
	python3 tools/export_book.py --source $(ALGO_LATEX_DIR) --dest "$(ALGO_EXPORT_DIR)" --pdf-name "$(ALGO_EXPORT_NAME)" --epub-name "$(ALGO_EPUB_NAME)"

algo-phone-export: algo-pdf algo-epub
	python3 tools/export_book.py --source $(ALGO_LATEX_DIR) --dest "$(ALGO_PHONE_EXPORT_DIR)" --pdf-name "$(ALGO_EXPORT_NAME)" --epub-name "$(ALGO_EPUB_NAME)"
	$(MAKE) phone-export-organize-allow-missing

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

cpp-phone-export: cpp-pdf cpp-epub
	python3 tools/export_book.py --source $(CPP_LATEX_DIR) --dest "$(CPP_PHONE_EXPORT_DIR)" --pdf-name "$(CPP_EXPORT_NAME)" --epub-name "$(CPP_EPUB_NAME)"
	$(MAKE) phone-export-organize-allow-missing

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

cse-phone-export: cse-pdf cse-epub
	python3 tools/export_book.py --source $(CSE_LATEX_DIR) --dest "$(CSE_PHONE_EXPORT_DIR)" --pdf-name "$(CSE_EXPORT_NAME)" --epub-name "$(CSE_EPUB_NAME)"
	$(MAKE) phone-export-organize-allow-missing

clean:
	$(MAKE) -C $(CPU_LATEX_DIR) clean
	$(MAKE) -C $(CPU1P_LATEX_DIR) clean
	$(MAKE) -C $(CPU2_LATEX_DIR) clean
	$(MAKE) -C $(CPU3_LATEX_DIR) clean
	$(MAKE) -C $(CPU3P_LATEX_DIR) clean
	$(MAKE) -C $(CPU3S_LATEX_DIR) clean
	$(MAKE) -C $(ALGO_BOOK_DIR) clean
	$(MAKE) -C $(CPP_BOOK_DIR) clean
	$(MAKE) -C $(CSE_LATEX_DIR) clean
