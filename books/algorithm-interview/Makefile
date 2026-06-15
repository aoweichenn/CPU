.PHONY: check pdf test clean

BUILD_DIR := build/labs-debug
LATEX_DIR := source/latex

check:
	$(MAKE) -C $(LATEX_DIR) check

pdf:
	$(MAKE) -C $(LATEX_DIR) pdf

test:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf build
	$(MAKE) -C $(LATEX_DIR) clean
