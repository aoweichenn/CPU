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
	cmake -S ../cpu-volume-3 -B ../cpu-volume-3/build/lcqi-debug -DCMAKE_BUILD_TYPE=Debug
	cmake --build ../cpu-volume-3/build/lcqi-debug
	ctest --test-dir ../cpu-volume-3/build/lcqi-debug --output-on-failure

clean:
	$(MAKE) -C $(LATEX_DIR) clean
