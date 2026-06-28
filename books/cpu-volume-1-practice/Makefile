LATEX_DIR := source/latex

.PHONY: all check pdf epub test clean

all: pdf epub

check:
	$(MAKE) -C $(LATEX_DIR) check

pdf:
	$(MAKE) -C $(LATEX_DIR) pdf

epub:
	$(MAKE) -C $(LATEX_DIR) epub

test:
	cmake -S . -B build/executable-evidence-debug -DCMAKE_BUILD_TYPE=Debug
	cmake --build build/executable-evidence-debug
	ctest --test-dir build/executable-evidence-debug --output-on-failure

clean:
	$(MAKE) -C $(LATEX_DIR) clean
