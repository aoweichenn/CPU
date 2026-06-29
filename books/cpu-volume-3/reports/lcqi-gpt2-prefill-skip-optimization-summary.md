# LCQI GPT-2 Prefill-Skip Optimization Summary

Remote validation: `caw`, Release CMake build through `books/cpu-volume-3-practice`, GPT-2 F32 safetensors, prompt `Hello, my name is`, `--engine cached --threads 0`, 7 rounds per case.

Raw report: `books/cpu-volume-3/reports/lcqi-gpt2-prefill-skip-ab-caw.txt`.

## Decision

The accepted optimization skips unused `lm_head` prediction during cached prompt prefill. For a prompt of `N` tokens, only the last prompt token's prediction is needed as the first generated token. The first `N - 1` prompt tokens only need to update hidden state and KV cache, so `Gpt2CachedGreedyDecoder::advance_without_prediction()` runs the cached layer path without final norm and `lm_head`.

A packed F32 `lm_head` experiment was also tested first and rejected: it reduced `lm_head_ms` only slightly and did not produce stable end-to-end speedup on `caw`. That path is not kept in the main code.

## Results

For `max_new_tokens=4`, median generation time changed from `60.5154 ms` to `52.1949 ms`, about `13.75%` faster. Median prefill time changed from `37.8981 ms` to `30.1768 ms`. Median `lm_head_ms` changed from `13.9000 ms` to `6.89725 ms`.

For `max_new_tokens=16`, median generation time changed from `153.574 ms` to `147.424 ms`, about `4.00%` faster. Median prefill time changed from `39.6590 ms` to `32.7775 ms`. Median `lm_head_ms` changed from `35.1835 ms` to `28.2918 ms`.

Generated ids and text were identical in every round for both 4-token and 16-token cases.

## Interpretation

This optimization is larger for short generation because prompt prefill is a larger fraction of total time. Decode throughput is nearly unchanged, as expected, because decode steps still need the next-token prediction. The change is semantic-preserving: it removes predictions whose results were overwritten before being used.
