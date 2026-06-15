.PHONY: check test clean

BUILD_DIR := build/labs-debug

check:
	@python3 tools/check_book.py

test:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf build
