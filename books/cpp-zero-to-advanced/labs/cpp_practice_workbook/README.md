# Cpp Practice Workbook

This lab is the runnable practice slice for `Cpp 从零到高级`.

It combines three small but complete ideas:

- Parse a tiny configuration format with structured diagnostics.
- Analyze text through normalization, filtering, counting, and deterministic sorting.
- Keep recent analysis results in an LRU cache with explicit invariants.

Build and test:

```bash
cmake -S books/cpp-zero-to-advanced -B books/cpp-zero-to-advanced/build/practice-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build books/cpp-zero-to-advanced/build/practice-debug
ctest --test-dir books/cpp-zero-to-advanced/build/practice-debug --output-on-failure
```

Run the demo:

```bash
books/cpp-zero-to-advanced/build/practice-debug/labs/cpp_practice_workbook/cppbook_practice_demo
```
