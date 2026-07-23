---
name: hardware-book-architecture
description: Plan, restructure, expand, typeset, polish, audit, export, or publish the LaTeX book `books/hardware-zero-to-machine` while preserving every existing content unit. Use for changes to parts or chapters, storage coverage, prose, tables, checklists, exercises, pseudocode, diagrams, fonts, PDF layout, content-loss validation, phone export, or final commit and push.
---

# Hardware Book Architecture

Treat the approved 7-part, 26-chapter structure as the default target. Preserve all existing prose, figures, tables, equations, code, traces, examples, exercises, answers, checklists, and diagnostic cases.

## Load the project rules

Read these references completely before planning or editing:

1. **On every invocation, without exception:** `references/user-book-rules.md`. Load it before inspecting, planning, answering a book-status question, or editing, even when the requested change appears small.
2. `references/architecture-26-chapters.md`
3. `references/migration-ledger.md`
4. For prose editing, rewriting, expansion, or polishing, also read `references/editorial-style.md`.

Treat an explicit user instruction as higher priority than these references. Do not change the 26-chapter target merely to make migration easier.

Before finishing any manuscript change, explicitly check the applicable non-negotiable rules from `user-book-rules.md`; a successful LaTeX compile alone is not sufficient.

## Keep rules outside the manuscript

Store architecture rules, migration policy, ledgers, and audit tooling only in this skill directory. Do not put them in `books/hardware-zero-to-machine`, manuscript front matter, chapter prose, `AGENT.md`, or the book Makefile. Add only reader-facing book content to the manuscript.

## Apply the zero-loss invariant

- Interpret restructuring as relocation plus expansion, never summarization or deletion.
- Preserve text byte-for-byte on the first migration pass wherever practical; edit for flow only after coverage is proven.
- Do not delete an apparent duplicate. Assign a primary teaching location, then retain the other material as recap, comparison, case study, or exercise until the user explicitly authorizes consolidation.
- Keep historical processors and platform examples as cases inside mechanism-led chapters.
- Keep every old source file until all of its ledger rows are marked `verified` and the new input graph builds.
- Never reduce coverage to hit the 26-chapter count. Merge chapter containers, not knowledge units.

## Follow the migration workflow

1. Inspect `source/latex/main.tex`, all reachable `.tex` files, and the working tree. Preserve unrelated user changes.
2. Run `python3 .codex/skills/hardware-book-architecture/scripts/check_legacy_baseline.py` from the repository root before moving content. Stop and investigate any mismatch rather than refreshing the baseline automatically.
3. Update the skill's migration ledger before each batch. Give every old section, exercise/answer group, and special case one target chapter and a status.
4. Create the new part and chapter containers in small batches. Move whole semantic blocks first; add transitions and new material second.
5. Keep the storage path contiguous as Chapters 10–16 and explain every interface at the boundary where it becomes necessary.
6. After each batch, run the book's existing input check and build. Check references, labels, figures, tables, exercises, answers, and index/TOC reachability.
7. Mark a ledger row `verified` only after its content is reachable from the new `main.tex`, renders successfully, and has been compared against the legacy source.
8. Switch fully to the new input graph only when every ledger row is `verified`. Retain the legacy files until the user approves cleanup.

## Finish and publish the work

When the user asks to finish the book restructuring, perform this closeout unless they explicitly opt out:

1. Run the manuscript input check and build the PDF and EPUB where supported.
2. Run `make -C books/hardware-zero-to-machine phone-export` and verify the exported book under `/mnt/sdcard/STU/BOOKS/按卷类型/原理卷/硬件从零到整机`.
3. Review `git status`, `git diff --check`, the migration audit, and the build logs. Do not treat pre-existing font warnings as new failures, but report them.
4. Stage only the hardware-book files and this skill's files. Never include unrelated working-tree files merely because the user asked to commit.
5. Commit with a message describing the completed book change, push the current branch to its configured remote, and verify that local and remote branch tips match.
6. Report the export destination, commit hash, pushed branch, build results, and any remaining warnings.

## Validate chapter quality

For each chapter, make the hardware path explicit: input/state, data path, control path, timing or commit boundary, interface contract, and error/recovery path. Include at least one concrete mechanism or historical case and end with concept, trace, boundary-condition, and fault-localization exercises with machine-level answers.

Do not claim completion from line counts alone. Report the ledger status, build results, and any content units still awaiting verification.

## Polish prose without losing content

When improving wording, keep the intact legacy files as the content baseline and make repeatable editorial transformations in `scripts/build_26_chapters.py` or reader-facing additions. Do not hand-edit generated `chapters26/ch*.tex` without updating its source of truth.

After regeneration, run `python3 .codex/skills/hardware-book-architecture/scripts/audit_prose.py`. Treat old numeric chapter references, `§` references, stale legacy chapter names, and vague relative section references as migration defects. Resolve them to stable topic or section names before publishing.

## Keep pseudocode annotated

Treat algorithms, state machines, transactions, training/recovery procedures, control loops, and micro-operation traces as pseudocode. Give every such `lstlisting` at least one comment about how state or ownership advances and one comment about its completion, commit, failure, or recovery boundary. Do not relabel equations, terminal transcripts, data layouts, or real Verilog/C/assembly examples as pseudocode merely to raise the count; preserve their native comments instead.

Register generated-chapter pseudocode in `scripts/build_26_chapters.py` with a stable first-line prefix and expected occurrence count. Regenerate rather than hand-editing `chapters26`, then run `python3 .codex/skills/hardware-book-architecture/scripts/audit_pseudocode_comments.py`. A moved, missing, duplicated, unannotated, or multiply annotated registered block is a publishing failure.

## Enforce table readability

Keep chapter-end exercises out of tables and render them with the numbered exercise/answer environments. Give every reachable body table and checklist visible row and column separators. Split page-filling longtables into contextual chunks instead of shrinking them.

After regeneration, run `python3 .codex/skills/hardware-book-architecture/scripts/audit_table_rules.py`. Then render and inspect representative body tables, split longtables, and every back-matter checklist at normal page size; source-level rules and a clean compile do not replace PDF visual inspection.
