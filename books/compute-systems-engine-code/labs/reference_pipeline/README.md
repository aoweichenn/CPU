# Reference Pipeline

This lab is the first executable slice of the Compute Systems Engine code practice volume.

It fixes the smallest contract that later optimized versions must preserve:

- Every input line has a stable `RecordId`.
- Accepted records update deterministic event counts.
- Rejected records produce structured diagnostics.
- Reports and manifest rows are stable enough for regression tests.

Build and test:

```bash
cmake -S books/compute-systems-engine-code -B books/compute-systems-engine-code/build/reference-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build books/compute-systems-engine-code/build/reference-debug
ctest --test-dir books/compute-systems-engine-code/build/reference-debug --output-on-failure
```

Run the demo:

```bash
books/compute-systems-engine-code/build/reference-debug/labs/reference_pipeline/cse_reference_demo
```
