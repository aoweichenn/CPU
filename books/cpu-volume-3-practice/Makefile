LATEX_DIR := source/latex

.PHONY: all check pdf test clean

all: pdf

check:
	$(MAKE) -C $(LATEX_DIR) check

pdf:
	$(MAKE) -C $(LATEX_DIR) pdf

test:
	cmake -S . -B build/lcqi-debug -DCMAKE_BUILD_TYPE=Debug
	cmake --build build/lcqi-debug
	ctest --test-dir build/lcqi-debug --output-on-failure

clean:
	$(MAKE) -C $(LATEX_DIR) clean
