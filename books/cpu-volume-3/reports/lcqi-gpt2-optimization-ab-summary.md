# LCQI GPT-2 Optimization A/B Summary

Command:

```bash
python3 books/cpu-volume-3/tools/run_gpt2_optimization_ab.py --rounds 2 \
  --report books/cpu-volume-3/results/lcqi-gpt2-optimization-ab.txt
```

Scope:

- Baseline ref: `4febf3f`
- Model: `~/.cache/lcqi-gpt2-smoke/openai-community--gpt2`
- Prompt: `Hello, my name is`
- New tokens: `4`
- Build mode: Release through `books/cpu-volume-3-practice`

Latest smoke result:

| variant | engine | generate median ms | generate token/s median | decode token/s median |
| --- | --- | ---: | ---: | ---: |
| baseline | cached | 847.353 | 4.85652 | 10.8154 |
| current | cached | 764.604 | 5.23178 | 11.2225 |
| baseline | full | 1848.98 | 2.28202 | 2.28202 |
| current | full | 1829.82 | 2.18714 | 2.18714 |

Interpretation:

- The accepted optimization targets cached-KV generation, not full-prefix recomputation.
- The current path reuses `Gpt2ForwardWorkspace`, keeps KV cache and workspace alive in `Gpt2CachedGreedyDecoder`, and avoids returning a full logits vector when greedy generation only needs the predicted token.
- Tiny GPT-2 tests compare full-prefix logits, cached logits, optimized decoder logits, and logits-free greedy prediction before accepting the optimized path.
- End-to-end GPT-2 benchmark numbers are noisy on shared machines. Use the generated report's median/min/max and output text consistency, not a single fastest run.
