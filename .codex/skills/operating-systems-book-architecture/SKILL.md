---
name: operating-systems-book-architecture
description: Plan, restructure, expand, typeset, audit, export, or publish the LaTeX textbook `books/operating-systems-volume-1` and keep its main spine synchronized with the live sibling `../os` project. Use for project-version tracking, operating-system chapter architecture, first-principles explanations, concrete kernel structures, state-transition traces, system-call/process/memory/device/filesystem/network/security coverage, diagrams, tables, exercises, PDF layout, phone export, or final commit and push.
---

# Operating Systems Book Architecture

## Load the binding rules

Before inspecting, planning, editing, or reporting progress, read these files completely:

1. `references/user-book-rules.md` on every invocation.
2. `references/project-sync-workflow.md` on every invocation.
3. `references/progressive-book-architecture.md` for book or chapter design, ordering, restructuring, and expansion.
4. `references/chapter-workflow.md` for chapter expansion, layout, validation, export, commit, or push work.

Treat the current user instruction as higher priority. Keep editorial and publishing rules in this skill, not in reader-facing manuscript prose.

## Track the live OS project first

Treat the sibling `../os` project as the book's evolving main spine and the existing textbook
chapters as its retained deepening layer. Before planning, editing, auditing, or reporting on the
book, run:

```text
python3 .codex/skills/operating-systems-book-architecture/scripts/audit_project_sync.py
```

If the check fails, do not continue from the stale book boundary. Follow
`references/project-sync-workflow.md`: audit the live roadmap, latest release, ADRs, affected
source and tests; run the project's full verification entry; update the project spine, part
bridges, affected chapter openings, diagrams, evidence and current-versus-future wording; only
then refresh the recorded snapshot. A fingerprint change requires re-audit even when the milestone
number is unchanged. Never update the snapshot merely to silence the check.

At each handoff, report the live completed milestone, the implementation boundary, the verification
result, and which book nodes were synchronized.

## Build explanations from causes

Start with a concrete operation, limitation, conflict, or failure. Establish what state already exists and why the simple approach is insufficient before naming the mechanism that resolves it. Never open a topic with an unexplained term list.

Do not open a chapter with the final end-to-end chain. Derive one local structure at a time. Use the complete chain only after every participating object has already been introduced and justified; at that point the chain is a synthesis and verification device.

For each mechanism, make this chain explicit:

1. input and preconditions;
2. participating objects, exact fields, ownership, and lifetime;
3. data path and control decisions;
4. synchronization, ordering, and commit boundary;
5. externally visible result;
6. partial completion, error, rollback, retry, and recovery.

Use object graphs, field tables, before/after snapshots, stack layouts, and event timelines when prose alone would hide state. Distinguish hardware-saved state, software-saved state, derived state, and state that is merely referenced.

For every consequential address, add an address-decision explanation in the nearby causal
route: identify who fixes or chooses it, the coordinate system and interpreter, why that exact
value or range is useful, whether the next step copies bytes or merely changes their
interpretation, the concrete failure avoided, and the tradeoff of a plausible alternative.
When one payload crosses several address systems, keep a continuous address passport instead of
presenting disconnected hexadecimal constants.

## Maintain operating-system boundaries

Treat hardware facts as interface contracts already established by the hardware book. State only the hardware behavior required to derive the kernel mechanism.

Do not collapse distinct events into “the system switches”: separate privilege transition, system-call entry, exception or interrupt entry, kernel-stack selection, task scheduling, register context switch, address-space switch, return-to-user checks, and architectural return.

For asynchronous work, state who owns the request and buffer at every stage, what wakes the waiter, what “complete” means at that layer, and whether completion implies visibility or persistence.

## Expand chapters to textbook depth

Use the coverage and page criteria in `references/user-book-rules.md`. Treat the 50-page chapter requirement as a minimum body-content baseline, not a quota. Do not count front matter, chapter-opening whitespace, exercises, answers, or artificial layout inflation as substantive expansion.

Plan a chapter as one continuous causal route with several complete mechanism units. Preserve existing knowledge units unless the user explicitly authorizes deletion or consolidation.

## Validate the artifact

Follow `references/chapter-workflow.md`. A clean build is necessary but not sufficient:

- run `python3 .codex/skills/operating-systems-book-architecture/scripts/normalize_table_grid.py --check` and treat any reachable non-grid table as a publishing failure;
- inspect the chapter’s actual PDF pages;
- verify concrete structures and state changes against the prose;
- check figure spacing, arrow routing, table borders, page balance, and missing glyphs;
- audit formal wording and repetitive templates;
- report the chapter’s body-page span before and after expansion.

## Publish scoped changes

Unless the user explicitly opts out, complete the book build, phone export, hash comparison, scoped commit, push, and remote-tip verification. Stage only this skill and the operating-systems book files changed for the task. Preserve unrelated worktree content.
