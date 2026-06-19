---
name: technical-book-diagrams
description: Design, revise, and validate diagrams for technical textbooks, especially LaTeX/TikZ/PDF book diagrams. Use when Codex is asked to draw, redraw, polish, de-crowd, correct, or standardize conceptual diagrams, timelines, system flows, state machines, CPU/book figures, or when a diagram must become a reusable visual style for other chapters.
---

# Technical Book Diagrams

## Core Rule

Treat every diagram as a claim, not decoration. Before editing shapes, write the one sentence the figure must prove. If the proposed layout cannot make that sentence obvious, choose a different structure.

## Workflow

1. Read the surrounding prose and identify the semantic contrast: identity vs execution, event time vs arrival time, physical fact vs committed fact, state vs transition, data path vs cost.
2. Choose the simplest structure that separates those meanings:
   - Use aligned columns for side-by-side concepts, evidence tables, and "fact / observation / decision" diagrams.
   - Use rows for layered contracts, pipeline stages, and repeated cases.
   - Use swimlanes for actors over time.
   - Use a state diagram only when transitions are the main concept.
   - Use a timeline only when one real clock is the main concept.
3. Allocate space before drawing details. Set column centers, row baselines, text widths, and lane boxes first. Prefer fewer wider nodes over many tiny nodes.
4. Add arrows only where they carry meaning. Avoid crossed arrows, arrows through text, decorative loops, and backtracking curves used only to compensate for a bad layout.
5. Render the final PDF page and inspect it visually. A diagram is not done until the rendered page shows readable text, clear spacing, no overlap, and correct semantic grouping.

## Layout Standards

- Keep repeated nodes aligned on a grid. If coordinates are manual, make the grid obvious from the numbers.
- Give each node enough text width for the longest label; do not solve crowding by shrinking every label.
- Separate labels from data. Put column headers above groups; do not squeeze lane names beside crowded nodes.
- Keep arrows short and mostly horizontal or vertical. If many arrows are needed, the diagram probably wants a table, matrix, or lanes.
- Prefer `book lane`/background boxes, rows, and tables for textbook figures; avoid node clouds.
- Use color to encode categories consistently, not to decorate every item differently.
- Keep explanatory prose outside the diagram when it would crowd the drawing.

## Timeline And Ordering Rules

- Do not draw two horizontal timelines with the same tick marks unless they truly share the same clock and scale.
- For event-stream ordering, separate at least these meanings:
  - `event_time`: when the event happened and what semantic order it has.
  - `ingest_time`: when the system observed the event.
  - watermark or policy: when the system decides a window is stable and how late data is handled.
- For late events, show the decision as a policy result, not as a geometric "back arrow" unless the arrow itself is the subject.
- If arrival order differs from semantic order, use columns such as "发生顺序 / 到达顺序 / 窗口判定" or swimlanes by actor. This prevents readers from confusing observation order with business fact.

## Textbook Prose Coupling

When revising a figure inside a chapter, revise nearby prose if needed. The prose should introduce the concrete failure or counterexample first, then the diagram should clarify the mechanism that fixes it. Avoid leaving the figure as a standalone concept list.

Good surrounding prose usually follows:

1. Naive interpretation.
2. Counterexample that breaks it.
3. Mechanism or boundary introduced by the counterexample.
4. Diagram showing the mechanism.
5. Consequence for later implementation, performance, or recovery.

## Validation Checklist

Before finishing:

- Build the document target that contains the figure.
- Render the changed PDF page to an image with `mutool`, Ghostscript, or an equivalent renderer.
- Inspect the rendered image, not just the source.
- Check for overlap, cramped blocks, unreadable labels, arrow crossings, ambiguous clocks, and mismatches between prose and figure.
- If the project also builds EPUB, verify that diagram handling still matches the book's EPUB policy.
